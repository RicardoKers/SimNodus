// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "avr_cpu.hpp"

#include <algorithm>
#include <cstdio>

namespace simnodus::emu {

namespace {

// Operations; the order is the order of op_name().
enum AvrOp : std::uint8_t {
    OP_NONE = 0,
    OP_UNDEF,
    OP_NOP,
    OP_MOVW,
    OP_MULS,
    OP_MULSU,
    OP_FMUL,
    OP_FMULS,
    OP_FMULSU,
    OP_CPC,
    OP_SBC,
    OP_ADD,
    OP_CPSE,
    OP_CP,
    OP_SUB,
    OP_ADC,
    OP_AND,
    OP_EOR,
    OP_OR,
    OP_MOV,
    OP_CPI,
    OP_SBCI,
    OP_SUBI,
    OP_ORI,
    OP_ANDI,
    OP_LDD_Y, // LD Rd, Y and LDD Rd, Y+q
    OP_LDD_Z,
    OP_STD_Y,
    OP_STD_Z,
    OP_LDS,
    OP_LD_ZP, // Z+
    OP_LD_MZ, // -Z
    OP_LPM_Z,
    OP_LPM_ZP,
    OP_LD_YP,
    OP_LD_MY,
    OP_LD_X,
    OP_LD_XP,
    OP_LD_MX,
    OP_POP,
    OP_STS,
    OP_ST_ZP,
    OP_ST_MZ,
    OP_ST_YP,
    OP_ST_MY,
    OP_ST_X,
    OP_ST_XP,
    OP_ST_MX,
    OP_PUSH,
    OP_COM,
    OP_NEG,
    OP_SWAP,
    OP_INC,
    OP_ASR,
    OP_LSR,
    OP_ROR,
    OP_DEC,
    OP_JMP,
    OP_CALL,
    OP_BSET,
    OP_BCLR,
    OP_RET,
    OP_RETI,
    OP_SLEEP,
    OP_BREAK,
    OP_WDR,
    OP_LPM,
    OP_SPM,
    OP_IJMP,
    OP_ICALL,
    OP_ADIW,
    OP_SBIW,
    OP_CBI,
    OP_SBIC,
    OP_SBI,
    OP_SBIS,
    OP_MUL,
    OP_IN,
    OP_OUT,
    OP_RJMP,
    OP_RCALL,
    OP_LDI,
    OP_BRBS,
    OP_BRBC,
    OP_BLD,
    OP_BST,
    OP_SBRC,
    OP_SBRS,
    OP_COUNT
};

const char* const op_names[OP_COUNT] = {
    "none", "undefined", "NOP", "MOVW", "MULS", "MULSU", "FMUL", "FMULS", "FMULSU", "CPC", "SBC", "ADD", "CPSE",
    "CP", "SUB", "ADC", "AND", "EOR", "OR", "MOV", "CPI", "SBCI", "SUBI", "ORI", "ANDI", "LDD Y", "LDD Z", "STD Y",
    "STD Z", "LDS", "LD Z+", "LD -Z", "LPM Z", "LPM Z+", "LD Y+", "LD -Y", "LD X", "LD X+", "LD -X", "POP", "STS",
    "ST Z+", "ST -Z", "ST Y+", "ST -Y", "ST X", "ST X+", "ST -X", "PUSH", "COM", "NEG", "SWAP", "INC", "ASR", "LSR",
    "ROR", "DEC", "JMP", "CALL", "BSET", "BCLR", "RET", "RETI", "SLEEP", "BREAK", "WDR", "LPM", "SPM", "IJMP",
    "ICALL", "ADIW", "SBIW", "CBI", "SBIC", "SBI", "SBIS", "MUL", "IN", "OUT", "RJMP", "RCALL", "LDI", "BRBS",
    "BRBC", "BLD", "BST", "SBRC", "SBRS"};

constexpr std::uint8_t bit(unsigned value, unsigned n) { return static_cast<std::uint8_t>((value >> n) & 1u); }

// The low `bits` bits as a two's complement number.
int signed_field(unsigned value, unsigned bits)
{
    value &= (1u << bits) - 1;
    return (value & (1u << (bits - 1))) ? static_cast<int>(value) - static_cast<int>(1u << bits) : static_cast<int>(value);
}

} // namespace

AvrCpu::AvrCpu(const AvrConfig& config)
    : config_(config),
      flash_mask_(config.flash_words - 1),
      sp_mask_(0xFFFF),
      flash_(config.flash_words, 0xFFFF),
      decoded_(config.flash_words),
      data_(config.data_size, 0)
{
    // SPH implements the bits needed to address the data space.
    unsigned bits = 1;
    while ((1u << bits) < config.data_size) {
        ++bits;
    }
    sp_mask_ = static_cast<std::uint16_t>((1u << bits) - 1);
    reset();
}

void AvrCpu::reset()
{
    pc_ = reset_pc_ & flash_mask_;
    sreg_ = 0;
    sp_ = static_cast<std::uint16_t>(config_.sp_reset & sp_mask_);
    state_ = AvrState::Running;
    halt_reason_.clear();
    inhibit_interrupt_ = false;
}

void AvrCpu::halt(const std::string& reason)
{
    state_ = AvrState::Halted;
    halt_reason_ = reason;
    stop_requested_ = true;
}

void AvrCpu::load_flash(const std::vector<std::uint16_t>& words, std::uint32_t at)
{
    for (std::size_t i = 0; i < words.size() && at + i < flash_.size(); ++i) {
        flash_[at + i] = words[i];
    }
    for (auto& d : decoded_) {
        d = AvrInsn{};
    }
}

void AvrCpu::write_flash_word(std::uint32_t word, std::uint16_t value)
{
    word &= flash_mask_;
    flash_[word] = value;
    decoded_[word] = AvrInsn{};
    decoded_[(word - 1) & flash_mask_] = AvrInsn{}; // a two-word instruction before it
}

std::uint8_t AvrCpu::peek(std::uint16_t address) const
{
    if (address == sreg_address) {
        return sreg_;
    }
    if (address == spl_address) {
        return static_cast<std::uint8_t>(sp_);
    }
    if (address == sph_address) {
        return static_cast<std::uint8_t>(sp_ >> 8);
    }
    return address < data_.size() ? data_[address] : 0;
}

void AvrCpu::poke(std::uint16_t address, std::uint8_t value)
{
    if (address == sreg_address) {
        sreg_ = value;
    } else if (address == spl_address) {
        set_sp(static_cast<std::uint16_t>((sp_ & 0xFF00) | value));
    } else if (address == sph_address) {
        set_sp(static_cast<std::uint16_t>((sp_ & 0x00FF) | (value << 8)));
    } else if (address < data_.size()) {
        data_[address] = value;
    }
}

std::uint8_t AvrCpu::read_data(std::uint16_t address)
{
    if (!watchpoints_.empty()) {
        check_watch(address, false);
    }
    if (address < 0x20) {
        return data_[address];
    }
    if (address <= config_.io_end) {
        if (address == sreg_address || address == spl_address || address == sph_address) {
            return peek(address);
        }
        if (read_io) {
            return read_io(address);
        }
        return data_[address];
    }
    if (address < data_.size()) {
        return data_[address];
    }
    if (diagnostic) {
        char text[96];
        std::snprintf(text, sizeof text, "Read of 0x%04X, outside the data space, at PC 0x%04X", address,
            static_cast<unsigned>(pc_ * 2));
        diagnostic(text);
    }
    return 0;
}

void AvrCpu::write_data(std::uint16_t address, std::uint8_t value)
{
    if (!watchpoints_.empty()) {
        check_watch(address, true);
    }
    if (address < 0x20) {
        data_[address] = value;
        return;
    }
    if (address <= config_.io_end) {
        if (address == sreg_address || address == spl_address || address == sph_address) {
            poke(address, value);
        } else if (write_io) {
            write_io(address, value);
        } else {
            data_[address] = value;
        }
        return;
    }
    if (address < data_.size()) {
        data_[address] = value;
        return;
    }
    if (diagnostic) {
        char text[96];
        std::snprintf(text, sizeof text, "Write of 0x%04X, outside the data space, at PC 0x%04X", address,
            static_cast<unsigned>(pc_ * 2));
        diagnostic(text);
    }
}

void AvrCpu::push(std::uint8_t value)
{
    write_data(sp_, value);
    set_sp(static_cast<std::uint16_t>(sp_ - 1));
}

std::uint8_t AvrCpu::pop()
{
    set_sp(static_cast<std::uint16_t>(sp_ + 1));
    return read_data(sp_);
}

// The return address goes on the stack low byte first (at the higher
// address), then the high byte.
void AvrCpu::push_pc(std::uint32_t pc)
{
    push(static_cast<std::uint8_t>(pc));
    push(static_cast<std::uint8_t>(pc >> 8));
}

std::uint32_t AvrCpu::pop_pc()
{
    const std::uint32_t high = pop();
    const std::uint32_t low = pop();
    return ((high << 8) | low) & flash_mask_;
}

const char* AvrCpu::op_name(std::uint8_t op) { return op < OP_COUNT ? op_names[op] : "?"; }

int AvrCpu::op_count() { return OP_COUNT; }

void AvrCpu::count_ops(bool enable)
{
    count_ops_ = enable;
    if (enable) {
        op_hits_.assign(OP_COUNT, 0);
    }
}

AvrInsn AvrCpu::decode(std::uint16_t w, std::uint16_t next) const
{
    AvrInsn in;
    in.raw = w;
    in.op = OP_UNDEF;
    const auto d5 = static_cast<std::uint8_t>((w >> 4) & 0x1F);
    const auto r5 = static_cast<std::uint8_t>((w & 0x0F) | ((w >> 5) & 0x10));
    const auto d4 = static_cast<std::uint8_t>(16 + ((w >> 4) & 0x0F));
    const auto k8 = static_cast<std::uint16_t>((w & 0x0F) | ((w >> 4) & 0xF0));
    auto set = [&](AvrOp op, std::uint8_t d, std::uint8_t r, std::uint16_t k = 0) {
        in.op = op;
        in.d = d;
        in.r = r;
        in.k = k;
    };
    switch (w >> 12) {
    case 0x0:
        if (w == 0) {
            set(OP_NOP, 0, 0);
        } else if ((w & 0xFF00) == 0x0100) {
            set(OP_MOVW, static_cast<std::uint8_t>(((w >> 4) & 0x0F) * 2), static_cast<std::uint8_t>((w & 0x0F) * 2));
        } else if ((w & 0xFF00) == 0x0200) {
            set(OP_MULS, d4, static_cast<std::uint8_t>(16 + (w & 0x0F)));
        } else if ((w & 0xFF00) == 0x0300) {
            const auto d3 = static_cast<std::uint8_t>(16 + ((w >> 4) & 7));
            const auto r3 = static_cast<std::uint8_t>(16 + (w & 7));
            static const AvrOp ops[4] = {OP_MULSU, OP_FMUL, OP_FMULS, OP_FMULSU};
            set(ops[((w >> 6) & 2) | ((w >> 3) & 1)], d3, r3);
        } else {
            static const AvrOp ops[4] = {OP_NONE, OP_CPC, OP_SBC, OP_ADD};
            set(ops[(w >> 10) & 3], d5, r5);
        }
        break;
    case 0x1: {
        static const AvrOp ops[4] = {OP_CPSE, OP_CP, OP_SUB, OP_ADC};
        set(ops[(w >> 10) & 3], d5, r5);
        break;
    }
    case 0x2: {
        static const AvrOp ops[4] = {OP_AND, OP_EOR, OP_OR, OP_MOV};
        set(ops[(w >> 10) & 3], d5, r5);
        break;
    }
    case 0x3: set(OP_CPI, d4, 0, k8); break;
    case 0x4: set(OP_SBCI, d4, 0, k8); break;
    case 0x5: set(OP_SUBI, d4, 0, k8); break;
    case 0x6: set(OP_ORI, d4, 0, k8); break;
    case 0x7: set(OP_ANDI, d4, 0, k8); break;
    case 0x8:
    case 0xA: {
        const auto q = static_cast<std::uint16_t>((w & 7) | ((w >> 7) & 0x18) | ((w >> 8) & 0x20));
        const bool store = (w & 0x0200) != 0;
        const bool y = (w & 0x0008) != 0;
        set(store ? (y ? OP_STD_Y : OP_STD_Z) : (y ? OP_LDD_Y : OP_LDD_Z), d5, 0, q);
        break;
    }
    case 0x9:
        if ((w & 0x0C00) == 0x0000) { // 1001 00xd: loads and stores
            const bool store = (w & 0x0200) != 0;
            switch (w & 0x0F) {
            case 0x0: set(store ? OP_STS : OP_LDS, d5, 0, next); in.size = 2; break;
            case 0x1: set(store ? OP_ST_ZP : OP_LD_ZP, d5, 0); break;
            case 0x2: set(store ? OP_ST_MZ : OP_LD_MZ, d5, 0); break;
            case 0x4: if (!store) { set(OP_LPM_Z, d5, 0); } break;
            case 0x5: if (!store) { set(OP_LPM_ZP, d5, 0); } break;
            case 0x9: set(store ? OP_ST_YP : OP_LD_YP, d5, 0); break;
            case 0xA: set(store ? OP_ST_MY : OP_LD_MY, d5, 0); break;
            case 0xC: set(store ? OP_ST_X : OP_LD_X, d5, 0); break;
            case 0xD: set(store ? OP_ST_XP : OP_LD_XP, d5, 0); break;
            case 0xE: set(store ? OP_ST_MX : OP_LD_MX, d5, 0); break;
            case 0xF: set(store ? OP_PUSH : OP_POP, d5, 0); break;
            default: break; // ELPM, XCH, LAS, LAC, LAT: not on these chips
            }
        } else if ((w & 0x0E00) == 0x0400) { // 1001 010x: one-operand, flags, flow
            switch (w & 0x0F) {
            case 0x0: set(OP_COM, d5, 0); break;
            case 0x1: set(OP_NEG, d5, 0); break;
            case 0x2: set(OP_SWAP, d5, 0); break;
            case 0x3: set(OP_INC, d5, 0); break;
            case 0x5: set(OP_ASR, d5, 0); break;
            case 0x6: set(OP_LSR, d5, 0); break;
            case 0x7: set(OP_ROR, d5, 0); break;
            case 0xA: set(OP_DEC, d5, 0); break;
            case 0x8:
                if ((w & 0x0100) == 0) { // 1001 0100 Bsss 1000: BSET, BCLR
                    set((w & 0x0080) ? OP_BCLR : OP_BSET, static_cast<std::uint8_t>((w >> 4) & 7), 0);
                } else {
                    switch (w) {
                    case 0x9508: set(OP_RET, 0, 0); break;
                    case 0x9518: set(OP_RETI, 0, 0); break;
                    case 0x9588: set(OP_SLEEP, 0, 0); break;
                    case 0x9598:
                        if (config_.break_instruction) {
                            set(OP_BREAK, 0, 0);
                        }
                        break;
                    case 0x95A8: set(OP_WDR, 0, 0); break;
                    case 0x95C8: set(OP_LPM, 0, 0); break;
                    case 0x95E8: set(OP_SPM, 0, 0); break;
                    default: break; // ELPM, SPM Z+: not on these chips
                    }
                }
                break;
            case 0x9:
                if (w == 0x9409) {
                    set(OP_IJMP, 0, 0);
                } else if (w == 0x9509) {
                    set(OP_ICALL, 0, 0);
                }
                break; // EIJMP, EICALL: not on these chips
            case 0xC:
            case 0xD:
            case 0xE:
            case 0xF: {
                const auto k = (static_cast<std::uint32_t>(((w >> 3) & 0x3E) | (w & 1)) << 16) | next;
                if (config_.jmp_call) { // the address wraps at the flash size
                    set((w & 0x2) ? OP_CALL : OP_JMP, 0, 0, static_cast<std::uint16_t>(k & (config_.flash_words - 1)));
                }
                break;
            }
            default: break; // DES and reserved codes
            }
        } else if ((w & 0x0F00) == 0x0600) {
            set(OP_ADIW, static_cast<std::uint8_t>(24 + ((w >> 4) & 3) * 2), 0,
                static_cast<std::uint16_t>((w & 0x0F) | ((w >> 2) & 0x30)));
        } else if ((w & 0x0F00) == 0x0700) {
            set(OP_SBIW, static_cast<std::uint8_t>(24 + ((w >> 4) & 3) * 2), 0,
                static_cast<std::uint16_t>((w & 0x0F) | ((w >> 2) & 0x30)));
        } else if ((w & 0x0C00) == 0x0800) {
            static const AvrOp ops[4] = {OP_CBI, OP_SBIC, OP_SBI, OP_SBIS};
            set(ops[(w >> 8) & 3], static_cast<std::uint8_t>(w & 7), static_cast<std::uint8_t>((w >> 3) & 0x1F));
        } else if ((w & 0x0C00) == 0x0C00) {
            set(OP_MUL, d5, r5);
        }
        break;
    case 0xB: {
        const auto a = static_cast<std::uint8_t>((w & 0x0F) | ((w >> 5) & 0x30));
        set((w & 0x0800) ? OP_OUT : OP_IN, d5, a);
        break;
    }
    case 0xC: set(OP_RJMP, 0, 0, static_cast<std::uint16_t>(w & 0x0FFF)); break;
    case 0xD: set(OP_RCALL, 0, 0, static_cast<std::uint16_t>(w & 0x0FFF)); break;
    case 0xE: set(OP_LDI, d4, 0, k8); break;
    case 0xF:
        if ((w & 0x0800) == 0) {
            set((w & 0x0400) ? OP_BRBC : OP_BRBS, static_cast<std::uint8_t>(w & 7), 0,
                static_cast<std::uint16_t>((w >> 3) & 0x7F));
        } else if ((w & 0x0008) == 0) {
            static const AvrOp ops[4] = {OP_BLD, OP_BST, OP_SBRC, OP_SBRS};
            set(ops[(w >> 9) & 3], d5, static_cast<std::uint8_t>(w & 7));
        }
        break;
    default: break;
    }
    if (in.op == OP_NONE) {
        in.op = OP_UNDEF;
    }
    // Two-word encodings keep their size even where this chip lacks them, so
    // that a skip over them moves two words, as the instruction length says.
    if ((w & 0xFC0F) == 0x9000) {
        in.size = 2;
    }
    if ((w & 0xFE0C) == 0x940C) {
        in.size = 2;
    }
    return in;
}

const AvrInsn& AvrCpu::fetch()
{
    AvrInsn& in = decoded_[pc_];
    if (in.op == OP_NONE) {
        in = decode(flash_[pc_], flash_[(pc_ + 1) & flash_mask_]);
    }
    return in;
}

void AvrCpu::skip_next()
{
    const AvrInsn& next = fetch();
    pc_ = (pc_ + next.size) & flash_mask_;
    cycles_ += next.size; // one cycle per skipped word
}

void AvrCpu::undefined(const AvrInsn& in)
{
    if (diagnostic) {
        char text[120];
        std::snprintf(text, sizeof text, "Opcode 0x%04X at 0x%04X is not an instruction of this chip; it runs as a NOP",
            in.raw, static_cast<unsigned>(pc_ * 2));
        diagnostic(text);
    }
}

bool AvrCpu::take_interrupt()
{
    if (!interrupt_pending_ || !flag(I) || !pending_vector) {
        return false;
    }
    const int vector = pending_vector();
    if (vector <= 0) {
        return false;
    }
    if (state_ == AvrState::Sleeping) {
        state_ = AvrState::Running;
        cycles_ += 4; // the response from sleep takes four cycles more
    }
    push_pc(pc_);
    set_flag(I, false);
    pc_ = (vector_base_ + static_cast<std::uint32_t>(vector) * config_.vector_words) & flash_mask_;
    cycles_ += 4;
    if (interrupt_taken) {
        interrupt_taken(vector);
    }
    return true;
}

AvrState AvrCpu::run(std::uint64_t limit)
{
    stop_requested_ = false;
    while (cycles_ < limit && !stop_requested_) {
        if (state_ == AvrState::Halted || debug_halted_) {
            break;
        }
        if (inhibit_interrupt_) {
            inhibit_interrupt_ = false;
        } else if (interrupt_pending_ && take_interrupt()) {
            continue;
        }
        if (state_ == AvrState::Sleeping) {
            cycles_ = limit; // nothing to do until an interrupt: time moves on
            break;
        }
        if (!breakpoints_.empty()) {
            if (skip_breakpoint_) {
                skip_breakpoint_ = false;
            } else if (std::find(breakpoints_.begin(), breakpoints_.end(), pc_) != breakpoints_.end()) {
                debug_halted_ = true;
                break;
            }
        }
        const AvrInsn& in = fetch();
        execute(in);
    }
    return state_;
}

void AvrCpu::step_instruction()
{
    if (state_ == AvrState::Halted) {
        return;
    }
    if (inhibit_interrupt_) {
        inhibit_interrupt_ = false;
    } else if (interrupt_pending_ && take_interrupt()) {
        return;
    }
    if (state_ == AvrState::Sleeping) {
        state_ = AvrState::Running; // a debugger's step wakes the core
    }
    execute(fetch());
}

void AvrCpu::set_debugger_attached(bool attached)
{
    debugger_ = attached;
    if (!attached) {
        breakpoints_.clear();
        watchpoints_.clear();
        debug_halted_ = false;
        skip_breakpoint_ = false;
        watch_hit_ = {};
    }
}

void AvrCpu::set_breakpoints(std::vector<std::uint32_t> words) { breakpoints_ = std::move(words); }

void AvrCpu::set_watchpoints(std::vector<Watchpoint> watchpoints) { watchpoints_ = std::move(watchpoints); }

void AvrCpu::resume_from_halt()
{
    debug_halted_ = false;
    watch_hit_ = {};
    skip_breakpoint_ = true;
}

void AvrCpu::check_watch(std::uint16_t address, bool write)
{
    for (const auto& w : watchpoints_) {
        const bool kind = w.kind == WatchKind::Access || (w.kind == WatchKind::Write) == write;
        if (kind && address >= w.address && address < w.address + w.length) {
            watch_hit_ = {true, w.address, w.kind};
            debug_halted_ = true; // after this instruction
            stop_requested_ = true;
            return;
        }
    }
}

void AvrCpu::execute(const AvrInsn& in)
{
    ++instructions_;
    if (count_ops_) {
        ++op_hits_[in.op];
    }
    std::uint8_t* const r = data_.data();
    const std::uint32_t next_pc = (pc_ + in.size) & flash_mask_;
    std::uint32_t pc = next_pc;
    std::uint64_t cycles = 1;

    // Flag helpers.
    auto nz = [&](std::uint8_t result) {
        set_flag(N, (result & 0x80) != 0);
        set_flag(Z, result == 0);
    };
    auto sign = [&] { set_flag(S, flag(N) != flag(V)); };
    auto add_flags = [&](std::uint8_t a, std::uint8_t b, std::uint8_t res) {
        const unsigned carries = (a & b) | (b & ~res) | (~res & a);
        set_flag(H, (carries & 0x08) != 0);
        set_flag(C, (carries & 0x80) != 0);
        set_flag(V, (((a & b & ~res) | (~a & ~b & res)) & 0x80) != 0);
        nz(res);
        sign();
    };
    // Subtraction a - b = res; `keep_z`: SBC/SBCI/CPC leave Z set only if it
    // was set and the result is zero.
    auto sub_flags = [&](std::uint8_t a, std::uint8_t b, std::uint8_t res, bool keep_z) {
        const unsigned borrows = (~a & b) | (b & res) | (res & ~a);
        set_flag(H, (borrows & 0x08) != 0);
        set_flag(C, (borrows & 0x80) != 0);
        set_flag(V, (((a & ~b & ~res) | (~a & b & res)) & 0x80) != 0);
        const bool z = keep_z ? (res == 0 && flag(Z)) : res == 0;
        set_flag(N, (res & 0x80) != 0);
        set_flag(Z, z);
        sign();
    };
    auto logic_flags = [&](std::uint8_t res) {
        set_flag(V, false);
        nz(res);
        sign();
    };
    auto load = [&](std::uint16_t address) { return read_data(address); };

    switch (in.op) {
    case OP_NOP: break;
    case OP_MOVW:
        r[in.d] = r[in.r];
        r[in.d + 1] = r[in.r + 1];
        break;
    case OP_MUL:
    case OP_MULS:
    case OP_MULSU:
    case OP_FMUL:
    case OP_FMULS:
    case OP_FMULSU: {
        const bool sd = in.op == OP_MULS || in.op == OP_MULSU || in.op == OP_FMULS || in.op == OP_FMULSU;
        const bool sr = in.op == OP_MULS || in.op == OP_FMULS;
        const int a = sd ? static_cast<std::int8_t>(r[in.d]) : r[in.d];
        const int b = sr ? static_cast<std::int8_t>(r[in.r]) : r[in.r];
        auto product = static_cast<std::uint16_t>(a * b);
        const bool fractional = in.op == OP_FMUL || in.op == OP_FMULS || in.op == OP_FMULSU;
        set_flag(C, (product & 0x8000) != 0);
        if (fractional) {
            product = static_cast<std::uint16_t>(product << 1);
        }
        set_flag(Z, product == 0);
        set_pair(0, product);
        cycles = 2;
        break;
    }
    case OP_ADD:
    case OP_ADC: {
        const std::uint8_t a = r[in.d];
        const std::uint8_t b = r[in.r];
        const auto res = static_cast<std::uint8_t>(a + b + (in.op == OP_ADC && flag(C) ? 1 : 0));
        r[in.d] = res;
        add_flags(a, b, res);
        break;
    }
    case OP_SUB:
    case OP_SBC:
    case OP_CP:
    case OP_CPC: {
        const std::uint8_t a = r[in.d];
        const std::uint8_t b = r[in.r];
        const bool with_carry = in.op == OP_SBC || in.op == OP_CPC;
        const auto res = static_cast<std::uint8_t>(a - b - (with_carry && flag(C) ? 1 : 0));
        if (in.op == OP_SUB || in.op == OP_SBC) {
            r[in.d] = res;
        }
        sub_flags(a, b, res, with_carry);
        break;
    }
    case OP_SUBI:
    case OP_SBCI:
    case OP_CPI: {
        const std::uint8_t a = r[in.d];
        const auto b = static_cast<std::uint8_t>(in.k);
        const bool with_carry = in.op == OP_SBCI;
        const auto res = static_cast<std::uint8_t>(a - b - (with_carry && flag(C) ? 1 : 0));
        if (in.op != OP_CPI) {
            r[in.d] = res;
        }
        sub_flags(a, b, res, with_carry);
        break;
    }
    case OP_CPSE:
        if (r[in.d] == r[in.r]) {
            pc_ = next_pc;
            skip_next();
            pc = pc_;
        }
        break;
    case OP_AND:
        r[in.d] = static_cast<std::uint8_t>(r[in.d] & r[in.r]);
        logic_flags(r[in.d]);
        break;
    case OP_EOR:
        r[in.d] = static_cast<std::uint8_t>(r[in.d] ^ r[in.r]);
        logic_flags(r[in.d]);
        break;
    case OP_OR:
        r[in.d] = static_cast<std::uint8_t>(r[in.d] | r[in.r]);
        logic_flags(r[in.d]);
        break;
    case OP_ANDI:
        r[in.d] = static_cast<std::uint8_t>(r[in.d] & in.k);
        logic_flags(r[in.d]);
        break;
    case OP_ORI:
        r[in.d] = static_cast<std::uint8_t>(r[in.d] | in.k);
        logic_flags(r[in.d]);
        break;
    case OP_MOV: r[in.d] = r[in.r]; break;
    case OP_LDI: r[in.d] = static_cast<std::uint8_t>(in.k); break;
    case OP_LDD_Y: r[in.d] = load(static_cast<std::uint16_t>(pair(28) + in.k)); cycles = 2; break;
    case OP_LDD_Z: r[in.d] = load(static_cast<std::uint16_t>(pair(30) + in.k)); cycles = 2; break;
    case OP_STD_Y: write_data(static_cast<std::uint16_t>(pair(28) + in.k), r[in.d]); cycles = 2; break;
    case OP_STD_Z: write_data(static_cast<std::uint16_t>(pair(30) + in.k), r[in.d]); cycles = 2; break;
    case OP_LDS: r[in.d] = load(in.k); cycles = 2; break;
    case OP_STS: write_data(in.k, r[in.d]); cycles = 2; break;
    case OP_LD_X:
    case OP_LD_XP:
    case OP_LD_MX:
    case OP_LD_YP:
    case OP_LD_MY:
    case OP_LD_ZP:
    case OP_LD_MZ: {
        const int p = (in.op == OP_LD_X || in.op == OP_LD_XP || in.op == OP_LD_MX) ? 26
                      : (in.op == OP_LD_YP || in.op == OP_LD_MY)                  ? 28
                                                                                  : 30;
        const bool pre = in.op == OP_LD_MX || in.op == OP_LD_MY || in.op == OP_LD_MZ;
        const bool post = in.op == OP_LD_XP || in.op == OP_LD_YP || in.op == OP_LD_ZP;
        std::uint16_t address = pair(p);
        if (pre) {
            --address;
            set_pair(p, address);
        }
        const std::uint8_t value = load(address);
        if (post) {
            set_pair(p, static_cast<std::uint16_t>(address + 1));
        }
        r[in.d] = value; // with Rd in the pointer, the loaded value wins (undefined on the chip)
        cycles = 2;
        break;
    }
    case OP_ST_X:
    case OP_ST_XP:
    case OP_ST_MX:
    case OP_ST_YP:
    case OP_ST_MY:
    case OP_ST_ZP:
    case OP_ST_MZ: {
        const int p = (in.op == OP_ST_X || in.op == OP_ST_XP || in.op == OP_ST_MX) ? 26
                      : (in.op == OP_ST_YP || in.op == OP_ST_MY)                  ? 28
                                                                                  : 30;
        const bool pre = in.op == OP_ST_MX || in.op == OP_ST_MY || in.op == OP_ST_MZ;
        const bool post = in.op == OP_ST_XP || in.op == OP_ST_YP || in.op == OP_ST_ZP;
        const std::uint8_t value = r[in.d]; // read before the pointer changes
        std::uint16_t address = pair(p);
        if (pre) {
            --address;
            set_pair(p, address);
        }
        write_data(address, value);
        if (post) {
            set_pair(p, static_cast<std::uint16_t>(address + 1));
        }
        cycles = 2;
        break;
    }
    case OP_LPM:
    case OP_LPM_Z:
    case OP_LPM_ZP: {
        const std::uint16_t z = pair(30);
        std::uint8_t value = 0;
        if (!lpm_read || !lpm_read(z, value)) {
            value = flash_byte(z);
        }
        r[in.op == OP_LPM ? 0 : in.d] = value;
        if (in.op == OP_LPM_ZP) {
            set_pair(30, static_cast<std::uint16_t>(z + 1));
        }
        cycles = 3;
        break;
    }
    case OP_SPM: {
        const unsigned extra = on_spm ? on_spm(pair(30), pair(0)) : 0;
        cycles = 1 + extra;
        break;
    }
    case OP_PUSH: push(r[in.d]); cycles = 2; break;
    case OP_POP: r[in.d] = pop(); cycles = 2; break;
    case OP_COM: {
        const auto res = static_cast<std::uint8_t>(~r[in.d]);
        r[in.d] = res;
        set_flag(C, true);
        logic_flags(res);
        break;
    }
    case OP_NEG: {
        const std::uint8_t a = r[in.d];
        const auto res = static_cast<std::uint8_t>(0 - a);
        r[in.d] = res;
        set_flag(H, ((res | a) & 0x08) != 0);
        set_flag(V, res == 0x80);
        set_flag(C, res != 0);
        nz(res);
        sign();
        break;
    }
    case OP_SWAP: r[in.d] = static_cast<std::uint8_t>((r[in.d] << 4) | (r[in.d] >> 4)); break;
    case OP_INC: {
        const auto res = static_cast<std::uint8_t>(r[in.d] + 1);
        r[in.d] = res;
        set_flag(V, res == 0x80);
        nz(res);
        sign();
        break;
    }
    case OP_DEC: {
        const auto res = static_cast<std::uint8_t>(r[in.d] - 1);
        r[in.d] = res;
        set_flag(V, res == 0x7F);
        nz(res);
        sign();
        break;
    }
    case OP_ASR:
    case OP_LSR:
    case OP_ROR: {
        const std::uint8_t a = r[in.d];
        std::uint8_t res = static_cast<std::uint8_t>(a >> 1);
        if (in.op == OP_ASR) {
            res = static_cast<std::uint8_t>(res | (a & 0x80));
        } else if (in.op == OP_ROR && flag(C)) {
            res = static_cast<std::uint8_t>(res | 0x80);
        }
        r[in.d] = res;
        set_flag(C, (a & 1) != 0);
        nz(res);
        set_flag(V, flag(N) != flag(C));
        sign();
        break;
    }
    case OP_JMP: pc = in.k & flash_mask_; cycles = 3; break;
    case OP_CALL:
        push_pc(next_pc);
        pc = in.k & flash_mask_;
        cycles = 4;
        break;
    case OP_RJMP: pc = (pc_ + 1 + static_cast<std::uint32_t>(signed_field(in.k, 12))) & flash_mask_; cycles = 2; break;
    case OP_RCALL:
        push_pc(next_pc);
        pc = (pc_ + 1 + static_cast<std::uint32_t>(signed_field(in.k, 12))) & flash_mask_;
        cycles = 3;
        break;
    case OP_IJMP: pc = pair(30) & flash_mask_; cycles = 2; break;
    case OP_ICALL:
        push_pc(next_pc);
        pc = pair(30) & flash_mask_;
        cycles = 3;
        break;
    case OP_RET: pc = pop_pc(); cycles = 4; break;
    case OP_RETI:
        pc = pop_pc();
        set_flag(I, true);
        inhibit_interrupt_ = true; // one instruction runs before the next interrupt
        cycles = 4;
        break;
    case OP_BSET:
        if (in.d == I && !flag(I)) {
            inhibit_interrupt_ = true; // the instruction after SEI runs first
        }
        set_flag(static_cast<Flag>(in.d), true);
        break;
    case OP_BCLR: set_flag(static_cast<Flag>(in.d), false); break;
    case OP_BRBS:
    case OP_BRBC:
        if (flag(static_cast<Flag>(in.d)) == (in.op == OP_BRBS)) {
            pc = (pc_ + 1 + static_cast<std::uint32_t>(signed_field(in.k, 7))) & flash_mask_;
            cycles = 2;
        }
        break;
    case OP_SBRC:
    case OP_SBRS:
        if (bit(r[in.d], in.r) == (in.op == OP_SBRS ? 1 : 0)) {
            pc_ = next_pc;
            skip_next();
            pc = pc_;
        }
        break;
    case OP_SBIC:
    case OP_SBIS:
        if (bit(read_data(static_cast<std::uint16_t>(0x20 + in.r)), in.d) == (in.op == OP_SBIS ? 1 : 0)) {
            pc_ = next_pc;
            skip_next();
            pc = pc_;
        }
        break;
    case OP_CBI:
    case OP_SBI: {
        const auto address = static_cast<std::uint16_t>(0x20 + in.r);
        const std::uint8_t value = read_data(address);
        write_data(address, static_cast<std::uint8_t>(in.op == OP_SBI ? (value | (1u << in.d)) : (value & ~(1u << in.d))));
        cycles = 2;
        break;
    }
    case OP_BST: set_flag(T, bit(r[in.d], in.r) != 0); break;
    case OP_BLD:
        r[in.d] = static_cast<std::uint8_t>(flag(T) ? (r[in.d] | (1u << in.r)) : (r[in.d] & ~(1u << in.r)));
        break;
    case OP_IN: r[in.d] = read_data(static_cast<std::uint16_t>(0x20 + in.r)); break;
    case OP_OUT: write_data(static_cast<std::uint16_t>(0x20 + in.r), r[in.d]); break;
    case OP_ADIW:
    case OP_SBIW: {
        const std::uint16_t a = pair(in.d);
        const auto res = static_cast<std::uint16_t>(in.op == OP_ADIW ? a + in.k : a - in.k);
        set_pair(in.d, res);
        const bool a15 = (a & 0x8000) != 0;
        const bool r15 = (res & 0x8000) != 0;
        if (in.op == OP_ADIW) {
            set_flag(V, !a15 && r15);
            set_flag(C, !r15 && a15);
        } else {
            set_flag(V, a15 && !r15);
            set_flag(C, r15 && !a15);
        }
        set_flag(N, r15);
        set_flag(Z, res == 0);
        sign();
        cycles = 2;
        break;
    }
    case OP_SLEEP:
        if (on_sleep && on_sleep()) {
            state_ = AvrState::Sleeping;
        }
        break;
    case OP_WDR:
        if (on_wdr) {
            on_wdr();
        }
        break;
    case OP_BREAK:
        if (debugger_) {
            debug_halted_ = true; // after the BREAK, so that continuing goes on
            stop_requested_ = true;
        } else if (on_break) {
            on_break();
        }
        break;
    case OP_UNDEF:
    default: undefined(in); break;
    }
    pc_ = pc;
    cycles_ += cycles;
}

} // namespace simnodus::emu
