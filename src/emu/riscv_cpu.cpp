// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "riscv_cpu.hpp"

#include <algorithm>
#include <cstdio>
#include <limits>

namespace simnodus::emu {

namespace {

// Operations. Compressed instructions decode to these too.
enum RvOp : std::uint8_t {
    OP_NONE,
    OP_ILLEGAL,
    OP_LUI, OP_AUIPC, OP_JAL, OP_JALR,
    OP_BEQ, OP_BNE, OP_BLT, OP_BGE, OP_BLTU, OP_BGEU,
    OP_LB, OP_LH, OP_LW, OP_LBU, OP_LHU, OP_SB, OP_SH, OP_SW,
    OP_ADDI, OP_SLTI, OP_SLTIU, OP_XORI, OP_ORI, OP_ANDI, OP_SLLI, OP_SRLI, OP_SRAI,
    OP_ADD, OP_SUB, OP_SLL, OP_SLT, OP_SLTU, OP_XOR, OP_SRL, OP_SRA, OP_OR, OP_AND,
    OP_MUL, OP_MULH, OP_MULHSU, OP_MULHU, OP_DIV, OP_DIVU, OP_REM, OP_REMU,
    OP_FENCE, OP_FENCE_I, OP_ECALL, OP_EBREAK, OP_MRET, OP_WFI,
    OP_CSRRW, OP_CSRRS, OP_CSRRC, OP_CSRRWI, OP_CSRRSI, OP_CSRRCI,
    OP_COUNT
};

// Decoder entries: the operations above for 32-bit instructions, then one per
// compressed instruction.
enum RvEntry : std::uint8_t {
    E_C_ADDI4SPN = OP_COUNT, E_C_LW, E_C_SW, E_C_ADDI, E_C_JAL, E_C_LI, E_C_ADDI16SP, E_C_LUI,
    E_C_SRLI, E_C_SRAI, E_C_ANDI, E_C_SUB, E_C_XOR, E_C_OR, E_C_AND, E_C_J, E_C_BEQZ, E_C_BNEZ,
    E_C_SLLI, E_C_LWSP, E_C_JR, E_C_MV, E_C_EBREAK, E_C_JALR, E_C_ADD, E_C_SWSP,
    E_COUNT
};

const char* const entry_names[E_COUNT] = {
    "none", "illegal",
    "lui", "auipc", "jal", "jalr",
    "beq", "bne", "blt", "bge", "bltu", "bgeu",
    "lb", "lh", "lw", "lbu", "lhu", "sb", "sh", "sw",
    "addi", "slti", "sltiu", "xori", "ori", "andi", "slli", "srli", "srai",
    "add", "sub", "sll", "slt", "sltu", "xor", "srl", "sra", "or", "and",
    "mul", "mulh", "mulhsu", "mulhu", "div", "divu", "rem", "remu",
    "fence", "fence.i", "ecall", "ebreak", "mret", "wfi",
    "csrrw", "csrrs", "csrrc", "csrrwi", "csrrsi", "csrrci",
    "c.addi4spn", "c.lw", "c.sw", "c.addi", "c.jal", "c.li", "c.addi16sp", "c.lui",
    "c.srli", "c.srai", "c.andi", "c.sub", "c.xor", "c.or", "c.and", "c.j", "c.beqz", "c.bnez",
    "c.slli", "c.lwsp", "c.jr", "c.mv", "c.ebreak", "c.jalr", "c.add", "c.swsp",
};

constexpr std::uint8_t x0_sink = 32;

constexpr std::uint32_t bits(std::uint32_t value, unsigned hi, unsigned lo)
{
    return (value >> lo) & ((1u << (hi - lo + 1)) - 1);
}

constexpr std::int32_t sign_extend(std::uint32_t value, unsigned width)
{
    const std::uint32_t m = 1u << (width - 1);
    return static_cast<std::int32_t>((value ^ m) - m);
}

constexpr std::uint8_t dest(std::uint32_t r) { return r == 0 ? x0_sink : static_cast<std::uint8_t>(r); }

RvInsn make(std::uint8_t op, std::uint8_t entry, std::uint32_t raw, std::uint8_t size)
{
    RvInsn in;
    in.op = op;
    in.entry = entry;
    in.raw = raw;
    in.size = size;
    return in;
}

RvInsn illegal(std::uint32_t raw, std::uint8_t size)
{
    return make(OP_ILLEGAL, OP_ILLEGAL, raw, size);
}

RvInsn decode32(std::uint32_t w)
{
    RvInsn in = make(OP_ILLEGAL, OP_ILLEGAL, w, 4);
    const std::uint32_t rd = bits(w, 11, 7);
    const std::uint32_t f3 = bits(w, 14, 12);
    const std::uint32_t rs1 = bits(w, 19, 15);
    const std::uint32_t rs2 = bits(w, 24, 20);
    const std::uint32_t f7 = bits(w, 31, 25);
    const std::int32_t imm_i = sign_extend(bits(w, 31, 20), 12);
    const std::int32_t imm_s = sign_extend((bits(w, 31, 25) << 5) | bits(w, 11, 7), 12);
    const std::int32_t imm_b = sign_extend((bits(w, 31, 31) << 12) | (bits(w, 7, 7) << 11)
            | (bits(w, 30, 25) << 5) | (bits(w, 11, 8) << 1), 13);
    const std::int32_t imm_j = sign_extend((bits(w, 31, 31) << 20) | (bits(w, 19, 12) << 12)
            | (bits(w, 20, 20) << 11) | (bits(w, 30, 21) << 1), 21);
    auto set = [&](std::uint8_t op, std::int32_t imm) {
        in.op = op;
        in.entry = op;
        in.rd = dest(rd);
        in.rs1 = static_cast<std::uint8_t>(rs1);
        in.rs2 = static_cast<std::uint8_t>(rs2);
        in.imm = imm;
    };
    switch (bits(w, 6, 0)) {
    case 0x37: set(OP_LUI, static_cast<std::int32_t>(w & 0xFFFFF000u)); break;
    case 0x17: set(OP_AUIPC, static_cast<std::int32_t>(w & 0xFFFFF000u)); break;
    case 0x6F: set(OP_JAL, imm_j); break;
    case 0x67:
        if (f3 == 0) {
            set(OP_JALR, imm_i);
        }
        break;
    case 0x63: {
        static constexpr std::uint8_t ops[8] = {OP_BEQ, OP_BNE, OP_ILLEGAL, OP_ILLEGAL, OP_BLT, OP_BGE, OP_BLTU, OP_BGEU};
        if (ops[f3] != OP_ILLEGAL) {
            set(ops[f3], imm_b);
            in.rd = x0_sink;
        }
        break;
    }
    case 0x03: {
        static constexpr std::uint8_t ops[8] = {OP_LB, OP_LH, OP_LW, OP_ILLEGAL, OP_LBU, OP_LHU, OP_ILLEGAL, OP_ILLEGAL};
        if (ops[f3] != OP_ILLEGAL) {
            set(ops[f3], imm_i);
        }
        break;
    }
    case 0x23: {
        static constexpr std::uint8_t ops[8] = {OP_SB, OP_SH, OP_SW, OP_ILLEGAL, OP_ILLEGAL, OP_ILLEGAL, OP_ILLEGAL, OP_ILLEGAL};
        if (ops[f3] != OP_ILLEGAL) {
            set(ops[f3], imm_s);
            in.rd = x0_sink;
        }
        break;
    }
    case 0x13:
        switch (f3) {
        case 0: set(OP_ADDI, imm_i); break;
        case 2: set(OP_SLTI, imm_i); break;
        case 3: set(OP_SLTIU, imm_i); break;
        case 4: set(OP_XORI, imm_i); break;
        case 6: set(OP_ORI, imm_i); break;
        case 7: set(OP_ANDI, imm_i); break;
        case 1:
            if (f7 == 0) {
                set(OP_SLLI, static_cast<std::int32_t>(rs2));
            }
            break;
        case 5:
            if (f7 == 0) {
                set(OP_SRLI, static_cast<std::int32_t>(rs2));
            } else if (f7 == 0x20) {
                set(OP_SRAI, static_cast<std::int32_t>(rs2));
            }
            break;
        default: break;
        }
        break;
    case 0x33: {
        static constexpr std::uint8_t base[8] = {OP_ADD, OP_SLL, OP_SLT, OP_SLTU, OP_XOR, OP_SRL, OP_OR, OP_AND};
        static constexpr std::uint8_t mul[8] = {OP_MUL, OP_MULH, OP_MULHSU, OP_MULHU, OP_DIV, OP_DIVU, OP_REM, OP_REMU};
        if (f7 == 0) {
            set(base[f3], 0);
        } else if (f7 == 1) {
            set(mul[f3], 0);
        } else if (f7 == 0x20 && f3 == 0) {
            set(OP_SUB, 0);
        } else if (f7 == 0x20 && f3 == 5) {
            set(OP_SRA, 0);
        }
        break;
    }
    case 0x0F:
        if (f3 == 0) {
            set(OP_FENCE, 0); // FENCE, FENCE.TSO and PAUSE: one hart, nothing to order
            in.rd = x0_sink;
        } else if (f3 == 1) {
            set(OP_FENCE_I, 0);
            in.rd = x0_sink;
        }
        break;
    case 0x73:
        if (f3 == 0) {
            if (rd == 0 && rs1 == 0) {
                switch (bits(w, 31, 20)) {
                case 0x000: set(OP_ECALL, 0); break;
                case 0x001: set(OP_EBREAK, 0); break;
                case 0x302: set(OP_MRET, 0); break;
                case 0x105: set(OP_WFI, 0); break;
                default: break; // SRET, URET and others: no supervisor mode
                }
            }
        } else if (f3 != 4) {
            static constexpr std::uint8_t ops[8] = {OP_ILLEGAL, OP_CSRRW, OP_CSRRS, OP_CSRRC, OP_ILLEGAL, OP_CSRRWI, OP_CSRRSI, OP_CSRRCI};
            set(ops[f3], static_cast<std::int32_t>(rs1)); // the uimm of the immediate forms
            in.csr = static_cast<std::uint16_t>(bits(w, 31, 20));
        }
        break;
    default: break;
    }
    return in;
}

RvInsn decode16(std::uint32_t h)
{
    RvInsn in = illegal(h, 2);
    const std::uint32_t f3 = bits(h, 15, 13);
    const std::uint32_t r_full = bits(h, 11, 7);
    const std::uint32_t r2_full = bits(h, 6, 2);
    const std::uint8_t r_short = static_cast<std::uint8_t>(bits(h, 9, 7) + 8);
    const std::uint8_t r2_short = static_cast<std::uint8_t>(bits(h, 4, 2) + 8);
    const std::int32_t imm6 = sign_extend((bits(h, 12, 12) << 5) | bits(h, 6, 2), 6);
    const std::uint32_t lw_off = (bits(h, 12, 10) << 3) | (bits(h, 6, 6) << 2) | (bits(h, 5, 5) << 6);
    const std::int32_t j_off = sign_extend((bits(h, 12, 12) << 11) | (bits(h, 11, 11) << 4) | (bits(h, 10, 9) << 8)
            | (bits(h, 8, 8) << 10) | (bits(h, 7, 7) << 6) | (bits(h, 6, 6) << 7) | (bits(h, 5, 3) << 1)
            | (bits(h, 2, 2) << 5), 12);
    const std::int32_t b_off = sign_extend((bits(h, 12, 12) << 8) | (bits(h, 11, 10) << 3) | (bits(h, 6, 5) << 6)
            | (bits(h, 4, 3) << 1) | (bits(h, 2, 2) << 5), 9);
    auto set = [&](std::uint8_t op, std::uint8_t entry, std::uint8_t rd, std::uint8_t rs1, std::uint8_t rs2,
                   std::int32_t imm) {
        in.op = op;
        in.entry = entry;
        in.rd = dest(rd);
        in.rs1 = rs1;
        in.rs2 = rs2;
        in.imm = imm;
    };
    switch (bits(h, 1, 0)) {
    case 0:
        if (f3 == 0) {
            const std::uint32_t nzuimm = (bits(h, 12, 11) << 4) | (bits(h, 10, 7) << 6) | (bits(h, 6, 6) << 2)
                | (bits(h, 5, 5) << 3);
            if (nzuimm != 0) { // also makes the all-zero halfword illegal
                set(OP_ADDI, E_C_ADDI4SPN, r2_short, 2, 0, static_cast<std::int32_t>(nzuimm));
            }
        } else if (f3 == 2) {
            set(OP_LW, E_C_LW, r2_short, r_short, 0, static_cast<std::int32_t>(lw_off));
        } else if (f3 == 6) {
            set(OP_SW, E_C_SW, 0, r_short, r2_short, static_cast<std::int32_t>(lw_off));
        }
        break; // C.FLD, C.FLW, C.FSD, C.FSW and the reserved 100: no F, D or Zcb
    case 1:
        switch (f3) {
        case 0: set(OP_ADDI, E_C_ADDI, static_cast<std::uint8_t>(r_full), static_cast<std::uint8_t>(r_full), 0, imm6); break;
        case 1: set(OP_JAL, E_C_JAL, 1, 0, 0, j_off); break;
        case 2: set(OP_ADDI, E_C_LI, static_cast<std::uint8_t>(r_full), 0, 0, imm6); break;
        case 3:
            if (r_full == 2) {
                const std::int32_t nzimm = sign_extend((bits(h, 12, 12) << 9) | (bits(h, 6, 6) << 4)
                        | (bits(h, 5, 5) << 6) | (bits(h, 4, 3) << 7) | (bits(h, 2, 2) << 5), 10);
                if (nzimm != 0) {
                    set(OP_ADDI, E_C_ADDI16SP, 2, 2, 0, nzimm);
                }
            } else if (imm6 != 0) {
                set(OP_LUI, E_C_LUI, static_cast<std::uint8_t>(r_full), 0, 0, static_cast<std::int32_t>(static_cast<std::uint32_t>(imm6) << 12));
            }
            break;
        case 4:
            switch (bits(h, 11, 10)) {
            case 0:
                if (bits(h, 12, 12) == 0) { // shamt[5] = 1 is reserved on RV32
                    set(OP_SRLI, E_C_SRLI, r_short, r_short, 0, static_cast<std::int32_t>(r2_full));
                }
                break;
            case 1:
                if (bits(h, 12, 12) == 0) {
                    set(OP_SRAI, E_C_SRAI, r_short, r_short, 0, static_cast<std::int32_t>(r2_full));
                }
                break;
            case 2: set(OP_ANDI, E_C_ANDI, r_short, r_short, 0, imm6); break;
            case 3:
                if (bits(h, 12, 12) == 0) { // C.SUBW and C.ADDW are RV64 only
                    static constexpr std::uint8_t ops[4] = {OP_SUB, OP_XOR, OP_OR, OP_AND};
                    static constexpr std::uint8_t entries[4] = {E_C_SUB, E_C_XOR, E_C_OR, E_C_AND};
                    const std::uint32_t f = bits(h, 6, 5);
                    set(ops[f], entries[f], r_short, r_short, r2_short, 0);
                }
                break;
            default: break;
            }
            break;
        case 5: set(OP_JAL, E_C_J, 0, 0, 0, j_off); break;
        case 6: set(OP_BEQ, E_C_BEQZ, 0, r_short, 0, b_off); break;
        case 7: set(OP_BNE, E_C_BNEZ, 0, r_short, 0, b_off); break;
        default: break;
        }
        break;
    case 2:
        switch (f3) {
        case 0:
            if (bits(h, 12, 12) == 0) {
                set(OP_SLLI, E_C_SLLI, static_cast<std::uint8_t>(r_full), static_cast<std::uint8_t>(r_full), 0,
                    static_cast<std::int32_t>(r2_full));
            }
            break;
        case 2:
            if (r_full != 0) {
                const std::uint32_t off = (bits(h, 12, 12) << 5) | (bits(h, 6, 4) << 2) | (bits(h, 3, 2) << 6);
                set(OP_LW, E_C_LWSP, static_cast<std::uint8_t>(r_full), 2, 0, static_cast<std::int32_t>(off));
            }
            break;
        case 4:
            if (bits(h, 12, 12) == 0) {
                if (r2_full == 0) {
                    if (r_full != 0) {
                        set(OP_JALR, E_C_JR, 0, static_cast<std::uint8_t>(r_full), 0, 0);
                    }
                } else {
                    set(OP_ADD, E_C_MV, static_cast<std::uint8_t>(r_full), 0, static_cast<std::uint8_t>(r2_full), 0);
                }
            } else if (r2_full == 0) {
                if (r_full == 0) {
                    set(OP_EBREAK, E_C_EBREAK, 0, 0, 0, 0);
                } else {
                    set(OP_JALR, E_C_JALR, 1, static_cast<std::uint8_t>(r_full), 0, 0);
                }
            } else {
                set(OP_ADD, E_C_ADD, static_cast<std::uint8_t>(r_full), static_cast<std::uint8_t>(r_full),
                    static_cast<std::uint8_t>(r2_full), 0);
            }
            break;
        case 6: {
            const std::uint32_t off = (bits(h, 12, 9) << 2) | (bits(h, 8, 7) << 6);
            set(OP_SW, E_C_SWSP, 0, 2, static_cast<std::uint8_t>(r2_full), static_cast<std::int32_t>(off));
            break;
        }
        default: break; // C.FLDSP, C.FLWSP, C.FSDSP, C.FSWSP: no F or D
        }
        break;
    default: break;
    }
    return in;
}

// CSR numbers.
constexpr std::uint16_t CSR_MSTATUS = 0x300, CSR_MISA = 0x301, CSR_MIE = 0x304, CSR_MTVEC = 0x305,
    CSR_MCOUNTEREN = 0x306, CSR_MSCRATCH = 0x340, CSR_MEPC = 0x341, CSR_MCAUSE = 0x342, CSR_MTVAL = 0x343,
    CSR_MIP = 0x344, CSR_PMPCFG0 = 0x3A0, CSR_PMPADDR0 = 0x3B0, CSR_MCYCLE = 0xB00, CSR_MINSTRET = 0xB02,
    CSR_MCYCLEH = 0xB80, CSR_MINSTRETH = 0xB82, CSR_CYCLE = 0xC00, CSR_INSTRET = 0xC02,
    CSR_CYCLEH = 0xC80, CSR_INSTRETH = 0xC82, CSR_MVENDORID = 0xF11, CSR_MARCHID = 0xF12, CSR_MIMPID = 0xF13,
    CSR_MHARTID = 0xF14;

constexpr std::uint32_t MSTATUS_MIE = 1u << 3, MSTATUS_MPIE = 1u << 7, MSTATUS_MPP_SHIFT = 11,
    MSTATUS_MPRV = 1u << 17, MSTATUS_TW = 1u << 21;

constexpr std::uint8_t PMP_R = 0x01, PMP_W = 0x02, PMP_X = 0x04, PMP_A = 0x18, PMP_L = 0x80;
constexpr std::uint8_t PMP_TOR = 0x08, PMP_NA4 = 0x10, PMP_NAPOT = 0x18;

} // namespace

RiscvCpu::RiscvCpu(Bus& bus) : RiscvCpu(bus, Config{}) {}

RiscvCpu::RiscvCpu(Bus& bus, const Config& config) : bus_(bus), config_(config)
{
    reset();
}

RvInsn RiscvCpu::decode(std::uint32_t bits_)
{
    return (bits_ & 3) == 3 ? decode32(bits_) : decode16(bits_ & 0xFFFF);
}

const char* RiscvCpu::entry_name(std::uint8_t entry)
{
    return entry < E_COUNT ? entry_names[entry] : "?";
}

int RiscvCpu::entry_count()
{
    return E_COUNT;
}

void RiscvCpu::reset()
{
    x_.fill(0);
    pc_ = config_.reset_pc;
    priv_ = Privilege::Machine;
    mie_bit_ = mpie_ = mprv_ = tw_ = false;
    mpp_ = Privilege::Machine;
    mie_ = 0;
    mtvec_ = config_.vectored_only ? 1 : 0;
    mscratch_ = mepc_ = mcause_ = mtval_ = 0;
    mcounteren_ = 0;
    cycle_offset_ = instret_offset_ = 0;
    pmpcfg_.fill(0);
    pmpaddr_.fill(0);
    pmp_changed();
    state_ = RvState::Running;
    halt_reason_.clear();
    attention_ = true;
    invalidate_code_cache();
}

void RiscvCpu::set_reg(int n, std::uint32_t value)
{
    if (n > 0 && n < 32) {
        x_[static_cast<std::size_t>(n)] = value;
    }
}

void RiscvCpu::set_pc(std::uint32_t value)
{
    pc_ = value & ~1u;
    if (state_ == RvState::Halted) {
        state_ = RvState::Running;
        halt_reason_.clear();
    }
    attention_ = true;
}

void RiscvCpu::advance_idle(std::uint64_t to)
{
    if (to > cycles_) {
        cycles_ = to;
    }
}

void RiscvCpu::set_interrupt_lines(std::uint32_t lines)
{
    if (lines != mip_) {
        if (lines & ~mip_) {
            interrupt_wait_ = config_.interrupt_latency;
        }
        mip_ = lines;
        attention_ = true;
    }
}

// ---------------------------------------------------------------------------
// Code cache.

void RiscvCpu::add_code_region(std::uint32_t base, std::uint32_t size)
{
    CodeRegion region;
    region.base = base;
    region.size = size;
    region.bytes = bus_.memory_at(base, size);
    if (!region.bytes || size < 4) {
        return; // not plain memory: decoded on every fetch
    }
    region.pages.resize((size + page_bytes - 1) / page_bytes);
    regions_.push_back(std::move(region));
    last_region_ = nullptr;
    last_page_base_ = 1;
}

void RiscvCpu::add_code_alias(std::uint32_t alias_base, std::uint32_t code_base, std::uint32_t size)
{
    code_aliases_.push_back({alias_base, code_base, size});
}

void RiscvCpu::invalidate_code_cache()
{
    for (CodeRegion& region : regions_) {
        for (auto& page : region.pages) {
            page.reset();
        }
        last_page_base_ = 1;
    }
}

void RiscvCpu::invalidate_code(std::uint32_t address, unsigned size)
{
    for (CodeRegion& region : regions_) {
        // A 32-bit instruction starting one halfword earlier also changes.
        const std::uint64_t first = address >= 2 ? address - 2u : address;
        const std::uint64_t last = std::uint64_t{address} + size - 1;
        const std::uint64_t base = region.base;
        if (last < base || first >= base + region.size) {
            continue;
        }
        const std::uint64_t lo = std::max(first, base) - base;
        const std::uint64_t hi = std::min(last - base, std::uint64_t{region.size} - 1);
        for (std::uint64_t i = lo & ~1ull; i <= hi; i += 2) {
            if (region.pages[i / page_bytes]) {
                region.at(static_cast<std::uint32_t>(i)) = RvInsn{};
            }
        }
    }
}

inline const RvInsn& RiscvCpu::fetch()
{
    if (pc_ - last_page_base_ < page_bytes - 2) { // room for a 32-bit instruction in the page's memory
        RvInsn& in = last_page_[(pc_ - last_page_base_) >> 1];
        if (in.op != OP_NONE) {
            return in;
        }
    }
    CodeRegion* region = last_region_;
    std::uint32_t offset = region ? pc_ - region->base : 0;
    if (!region || offset >= region->size - 2) { // room for a 32-bit instruction
        region = nullptr;
        for (CodeRegion& r : regions_) {
            if (pc_ - r.base < r.size - 2) {
                region = &r;
                offset = pc_ - r.base;
                break;
            }
        }
        last_region_ = region;
    }
    if (region) {
        RvInsn& in = region->at(offset);
        last_page_base_ = pc_ - (offset % page_bytes);
        last_page_ = region->pages[offset / page_bytes].get();
        if (in.op == OP_NONE) {
            std::uint32_t w = 0;
            std::memcpy(&w, region->bytes + offset, (region->bytes[offset] & 3) == 3 ? 4 : 2);
            in = decode(w);
        }
        return in;
    }
    // Outside the cached code: read through the bus. A fetch that fails is an
    // instruction access fault, reported as an op the run loop handles.
    std::uint32_t lo = 0;
    if (!bus_.read(pc_, 2, lo)) {
        scratch_ = make(OP_NONE, OP_NONE, 0, 0);
        return scratch_;
    }
    if ((lo & 3) != 3) {
        scratch_ = decode16(lo);
        return scratch_;
    }
    std::uint32_t hi = 0;
    if (!bus_.read(pc_ + 2, 2, hi)) {
        scratch_ = make(OP_NONE, OP_NONE, 0, 2); // the second halfword faults
        return scratch_;
    }
    scratch_ = decode32(lo | (hi << 16));
    return scratch_;
}

// ---------------------------------------------------------------------------
// Memory.

RiscvCpu::Privilege RiscvCpu::data_privilege() const
{
    return mprv_ ? mpp_ : priv_;
}

bool RiscvCpu::load(std::uint32_t address, unsigned size, std::uint32_t& value)
{
    if (address & (size - 1)) {
        if (config_.misaligned_access) {
            return load_misaligned(address, size, value);
        }
        trap(LoadMisaligned, address);
        return false;
    }
    if (pmp_check_ && !pmp_allows(address, size, PmpRead, data_privilege())) {
        trap(LoadAccessFault, address);
        return false;
    }
    if (!bus_.read(address, size, value)) {
        trap(LoadAccessFault, address);
        return false;
    }
    return true;
}

bool RiscvCpu::store(std::uint32_t address, unsigned size, std::uint32_t value)
{
    if (address & (size - 1)) {
        if (config_.misaligned_access) {
            return store_misaligned(address, size, value);
        }
        trap(StoreMisaligned, address);
        return false;
    }
    if (pmp_check_ && !pmp_allows(address, size, PmpWrite, data_privilege())) {
        trap(StoreAccessFault, address);
        return false;
    }
    if (!bus_.write(address, size, value)) {
        trap(StoreAccessFault, address);
        return false;
    }
    if (!regions_.empty()) {
        invalidate_code(address, size);
        for (const CodeAlias& alias : code_aliases_) {
            if (address - alias.alias_base < alias.size) {
                invalidate_code(alias.code_base + (address - alias.alias_base), size);
            }
        }
    }
    return true;
}

// Misaligned accesses where the core does them: one byte at a time, little
// endian; a byte that faults makes the whole access fault at its address.
bool RiscvCpu::load_misaligned(std::uint32_t address, unsigned size, std::uint32_t& value)
{
    if (pmp_check_ && !pmp_allows(address, size, PmpRead, data_privilege())) {
        trap(LoadAccessFault, address);
        return false;
    }
    std::uint32_t result = 0;
    for (unsigned i = 0; i < size; ++i) {
        std::uint32_t byte = 0;
        if (!bus_.read(address + i, 1, byte)) {
            trap(LoadAccessFault, address);
            return false;
        }
        result |= (byte & 0xFF) << (8 * i);
    }
    value = result;
    return true;
}

bool RiscvCpu::store_misaligned(std::uint32_t address, unsigned size, std::uint32_t value)
{
    if (pmp_check_ && !pmp_allows(address, size, PmpWrite, data_privilege())) {
        trap(StoreAccessFault, address);
        return false;
    }
    for (unsigned i = 0; i < size; ++i) {
        if (!bus_.write(address + i, 1, (value >> (8 * i)) & 0xFF)) {
            trap(StoreAccessFault, address);
            return false;
        }
    }
    if (!regions_.empty()) {
        invalidate_code(address, size);
        for (const CodeAlias& alias : code_aliases_) {
            if (address - alias.alias_base < alias.size) {
                invalidate_code(alias.code_base + (address - alias.alias_base), size);
            }
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Physical memory protection (privileged spec, section 3.7).

bool RiscvCpu::pmp_allows(std::uint32_t address, unsigned size, PmpAccess access, Privilege priv) const
{
    const std::uint64_t lo = address;
    const std::uint64_t hi = lo + size; // exclusive
    for (std::size_t i = 0; i < 16; ++i) {
        const std::uint8_t cfg = pmpcfg_[i];
        const std::uint8_t mode = cfg & PMP_A;
        if (mode == 0) {
            continue;
        }
        std::uint64_t start = 0;
        std::uint64_t end = 0;
        if (mode == PMP_TOR) {
            start = i == 0 ? 0 : static_cast<std::uint64_t>(pmpaddr_[i - 1]) << 2;
            end = static_cast<std::uint64_t>(pmpaddr_[i]) << 2;
        } else if (mode == PMP_NA4) {
            start = static_cast<std::uint64_t>(pmpaddr_[i]) << 2;
            end = start + 4;
        } else {
            const std::uint64_t a = pmpaddr_[i];
            unsigned ones = 0;
            while (ones < 32 && ((a >> ones) & 1)) {
                ++ones;
            }
            const std::uint64_t bytes = std::uint64_t{1} << (ones + 3);
            start = (a & ~((std::uint64_t{1} << ones) - 1)) << 2;
            start &= ~(bytes - 1);
            end = start + bytes;
        }
        const bool any = lo < end && hi > start;
        if (!any) {
            continue;
        }
        if (lo < start || hi > end) {
            return false; // partly inside the first matching entry
        }
        if (priv == Privilege::Machine && !(cfg & PMP_L)) {
            return true;
        }
        return (cfg & access) != 0;
    }
    return priv == Privilege::Machine;
}

bool RiscvCpu::fetch_allowed(std::uint32_t address, unsigned size)
{
    if (address >= exec_lo_ && address + size <= exec_hi_ && address + size > address) {
        return true;
    }
    if (!pmp_allows(address, size, PmpExec, priv_)) {
        return false;
    }
    // Remember a window around the address that the same answer covers: the
    // 4 KiB page, if every byte of it is allowed (checked at both ends and,
    // since entries are contiguous ranges, by asking for the whole page).
    const std::uint32_t page = address & ~0xFFFu;
    if (pmp_allows(page, 0x1000 - 4, PmpExec, priv_) && pmp_allows(page + 0x1000 - 4, 4, PmpExec, priv_)) {
        exec_lo_ = page;
        exec_hi_ = page + 0x1000;
    } else {
        exec_lo_ = address;
        exec_hi_ = address + size;
    }
    return true;
}

void RiscvCpu::pmp_changed()
{
    bool locked = false;
    for (std::uint8_t cfg : pmpcfg_) {
        if ((cfg & PMP_L) && (cfg & PMP_A)) {
            locked = true;
        }
    }
    pmp_check_ = locked || priv_ != Privilege::Machine || (mprv_ && mpp_ != Privilege::Machine);
    exec_lo_ = exec_hi_ = 0;
    if (!locked && priv_ == Privilege::Machine) {
        exec_lo_ = 0;
        exec_hi_ = 0xFFFFFFFFu; // machine mode without locked entries fetches anywhere
    }
    attention_ = true;
}

// ---------------------------------------------------------------------------
// Traps and interrupts.

void RiscvCpu::trap(std::uint32_t cause, std::uint32_t tval)
{
    trapped_ = true;
    if (on_trap) {
        on_trap(cause, pc_, tval);
    }
    mepc_ = pc_;
    mcause_ = cause;
    mtval_ = tval;
    mpie_ = mie_bit_;
    mie_bit_ = false;
    mpp_ = priv_;
    priv_ = Privilege::Machine;
    next_pc_ = mtvec_ & ~3u;
    pmp_changed();
}

void RiscvCpu::take_interrupt(int line)
{
    const std::uint32_t cause = 0x80000000u | static_cast<std::uint32_t>(line);
    if (on_trap) {
        on_trap(cause, pc_, 0);
    }
    mepc_ = pc_;
    mcause_ = cause;
    mtval_ = 0;
    mpie_ = mie_bit_;
    mie_bit_ = false;
    mpp_ = priv_;
    priv_ = Privilege::Machine;
    pc_ = (mtvec_ & ~3u) + ((mtvec_ & 3) == 1 ? 4u * static_cast<std::uint32_t>(line) : 0u);
    took_interrupt_ = true;
    pmp_changed();
}

int RiscvCpu::pending_interrupt() const
{
    const std::uint32_t ready = mip_ & (config_.mie_gates ? mie_ : 0xFFFFFFFFu);
    if (ready == 0 || (priv_ == Privilege::Machine && !mie_bit_)) {
        return -1;
    }
    if (choose_interrupt) {
        return choose_interrupt(ready);
    }
    for (int line : {11, 3, 7}) {
        if (ready & (1u << line)) {
            return line;
        }
    }
    for (int line = 0; line < 32; ++line) {
        if (ready & (1u << line)) {
            return line;
        }
    }
    return -1;
}

void RiscvCpu::mret()
{
    priv_ = mpp_;
    mie_bit_ = mpie_;
    mpie_ = true;
    if (mpp_ != Privilege::Machine) {
        mprv_ = false;
    }
    mpp_ = Privilege::User; // the least privileged mode
    next_pc_ = mepc_;
    pmp_changed();
}

// ---------------------------------------------------------------------------
// CSRs.

bool RiscvCpu::csr_read(std::uint16_t csr, std::uint32_t& value) const
{
    const std::uint64_t cycle = cycles_ + cycle_offset_;
    const std::uint64_t instret = instructions_ + instret_offset_;
    if (csr >= CSR_PMPCFG0 && csr < CSR_PMPCFG0 + 4) {
        const std::size_t i = static_cast<std::size_t>(csr - CSR_PMPCFG0) * 4;
        value = pmpcfg_[i] | (pmpcfg_[i + 1] << 8) | (pmpcfg_[i + 2] << 16) | (static_cast<std::uint32_t>(pmpcfg_[i + 3]) << 24);
        return true;
    }
    if (csr >= CSR_PMPADDR0 && csr < CSR_PMPADDR0 + 16) {
        value = pmpaddr_[static_cast<std::size_t>(csr - CSR_PMPADDR0)];
        return true;
    }
    switch (csr) {
    case CSR_MSTATUS:
        value = (mie_bit_ ? MSTATUS_MIE : 0) | (mpie_ ? MSTATUS_MPIE : 0)
            | (static_cast<std::uint32_t>(mpp_) << MSTATUS_MPP_SHIFT) | (mprv_ ? MSTATUS_MPRV : 0) | (tw_ ? MSTATUS_TW : 0);
        return true;
    case CSR_MISA:
        value = (1u << 30) | (1u << ('I' - 'A')) | (1u << ('M' - 'A')) | (1u << ('C' - 'A')) | (1u << ('U' - 'A'));
        return true;
    case CSR_MIE: value = mie_; return true;
    case CSR_MIP: value = mip_; return true;
    case CSR_MTVEC: value = mtvec_; return true;
    case CSR_MSCRATCH: value = mscratch_; return true;
    case CSR_MEPC: value = mepc_; return true;
    case CSR_MCAUSE: value = mcause_; return true;
    case CSR_MTVAL: value = mtval_; return true;
    case CSR_MVENDORID: value = config_.mvendorid; return true;
    case CSR_MARCHID: value = config_.marchid; return true;
    case CSR_MIMPID: value = config_.mimpid; return true;
    case CSR_MHARTID: value = 0; return true;
    default: break;
    }
    if (config_.counters) {
        switch (csr) {
        case CSR_MCOUNTEREN: value = mcounteren_; return true;
        case CSR_MCYCLE: case CSR_CYCLE: value = static_cast<std::uint32_t>(cycle); return true;
        case CSR_MCYCLEH: case CSR_CYCLEH: value = static_cast<std::uint32_t>(cycle >> 32); return true;
        case CSR_MINSTRET: case CSR_INSTRET: value = static_cast<std::uint32_t>(instret); return true;
        case CSR_MINSTRETH: case CSR_INSTRETH: value = static_cast<std::uint32_t>(instret >> 32); return true;
        default: break;
        }
    }
    if (read_custom_csr) { // no time CSR: firmware reads the machine's timer
        return read_custom_csr(csr, value);
    }
    return false;
}

bool RiscvCpu::csr_write(std::uint16_t csr, std::uint32_t value)
{
    if (csr >= CSR_PMPCFG0 && csr < CSR_PMPCFG0 + 4) {
        const std::size_t base = static_cast<std::size_t>(csr - CSR_PMPCFG0) * 4;
        for (std::size_t k = 0; k < 4; ++k) {
            if (!(pmpcfg_[base + k] & PMP_L)) {
                std::uint8_t cfg = static_cast<std::uint8_t>(value >> (8 * k)) & (PMP_R | PMP_W | PMP_X | PMP_A | PMP_L);
                if ((cfg & (PMP_R | PMP_W)) == PMP_W) {
                    cfg &= static_cast<std::uint8_t>(~PMP_W); // R=0 W=1 is reserved
                }
                pmpcfg_[base + k] = cfg;
            }
        }
        pmp_changed();
        return true;
    }
    if (csr >= CSR_PMPADDR0 && csr < CSR_PMPADDR0 + 16) {
        const std::size_t i = static_cast<std::size_t>(csr - CSR_PMPADDR0);
        const bool locked = (pmpcfg_[i] & PMP_L) != 0;
        const bool next_tor_locked = i + 1 < 16 && (pmpcfg_[i + 1] & PMP_L) && (pmpcfg_[i + 1] & PMP_A) == PMP_TOR;
        if (!locked && !next_tor_locked) {
            pmpaddr_[i] = value;
            pmp_changed();
        }
        return true;
    }
    switch (csr) {
    case CSR_MSTATUS: {
        mie_bit_ = (value & MSTATUS_MIE) != 0;
        mpie_ = (value & MSTATUS_MPIE) != 0;
        const std::uint32_t mpp = (value >> MSTATUS_MPP_SHIFT) & 3;
        mpp_ = mpp == 3 ? Privilege::Machine : Privilege::User; // only M and U exist
        mprv_ = (value & MSTATUS_MPRV) != 0;
        tw_ = (value & MSTATUS_TW) != 0;
        pmp_changed();
        return true;
    }
    case CSR_MISA: return true; // fixed
    case CSR_MIE:
        mie_ = value & config_.interrupt_lines;
        attention_ = true;
        return true;
    case CSR_MIP: return true; // every line is driven by the machine
    case CSR_MTVEC:
        mtvec_ = config_.vectored_only ? (value & ~3u) | 1 : (value & ~3u) | (value & 1);
        return true;
    case CSR_MSCRATCH: mscratch_ = value; return true;
    case CSR_MEPC: mepc_ = value & ~1u; return true;
    case CSR_MCAUSE: mcause_ = value; return true;
    case CSR_MTVAL: mtval_ = value; return true;
    default: break;
    }
    if (config_.counters) {
        const std::uint64_t cycle = cycles_ + cycle_offset_;
        const std::uint64_t instret = instructions_ + instret_offset_;
        switch (csr) {
        case CSR_MCOUNTEREN: mcounteren_ = value & 0x5; return true; // CY and IR
        case CSR_MCYCLE:
            cycle_offset_ = ((cycle & ~0xFFFFFFFFull) | value) - cycles_;
            return true;
        case CSR_MCYCLEH:
            cycle_offset_ = ((static_cast<std::uint64_t>(value) << 32) | (cycle & 0xFFFFFFFFu)) - cycles_;
            return true;
        case CSR_MINSTRET:
            // The writing instruction retires afterwards and must not count.
            instret_offset_ = ((instret & ~0xFFFFFFFFull) | value) - instructions_ - 1;
            return true;
        case CSR_MINSTRETH:
            instret_offset_ = ((static_cast<std::uint64_t>(value) << 32) | (instret & 0xFFFFFFFFu)) - instructions_ - 1;
            return true;
        default: break;
        }
    }
    if (write_custom_csr) {
        return write_custom_csr(csr, value);
    }
    return false;
}

bool RiscvCpu::read_csr(std::uint16_t csr, std::uint32_t& value)
{
    return csr_read(csr, value);
}

bool RiscvCpu::write_csr(std::uint16_t csr, std::uint32_t value)
{
    return csr_write(csr, value);
}

void RiscvCpu::csr_instruction(const RvInsn& in)
{
    const std::uint16_t csr = in.csr;
    const bool read_only = (csr >> 10) == 3;
    const unsigned level = (csr >> 8) & 3;
    if (level > static_cast<unsigned>(priv_)) {
        trap(IllegalInstruction, in.raw);
        return;
    }
    if (priv_ == Privilege::User && config_.counters
        && (csr == CSR_CYCLE || csr == CSR_CYCLEH || csr == CSR_INSTRET || csr == CSR_INSTRETH)) {
        const unsigned bit = (csr & 0x7F) == 0 ? 0 : 2;
        if (!(mcounteren_ & (1u << bit))) {
            trap(IllegalInstruction, in.raw);
            return;
        }
    }
    const bool immediate = in.op >= OP_CSRRWI;
    const std::uint32_t operand = immediate ? static_cast<std::uint32_t>(in.imm) : x_[in.rs1];
    const bool writes = in.op == OP_CSRRW || in.op == OP_CSRRWI || in.imm != 0 || (!immediate && in.rs1 != 0);
    // CSRRW with rd = x0 does not read; the others always read.
    std::uint32_t old = 0;
    const bool reads = !((in.op == OP_CSRRW || in.op == OP_CSRRWI) && in.rd == x0_sink);
    if (writes && read_only) {
        trap(IllegalInstruction, in.raw);
        return;
    }
    if (reads || !writes) {
        if (!csr_read(csr, old)) {
            trap(IllegalInstruction, in.raw);
            return;
        }
    } else {
        std::uint32_t probe = 0;
        if (!csr_read(csr, probe)) { // the CSR must exist even when not read
            trap(IllegalInstruction, in.raw);
            return;
        }
    }
    if (writes) {
        std::uint32_t value = operand;
        if (in.op == OP_CSRRS || in.op == OP_CSRRSI) {
            value = old | operand;
        } else if (in.op == OP_CSRRC || in.op == OP_CSRRCI) {
            value = old & ~operand;
        }
        if (!csr_write(csr, value)) {
            trap(IllegalInstruction, in.raw);
            return;
        }
    }
    x_[in.rd] = old;
}

// ---------------------------------------------------------------------------
// Execution.

bool RiscvCpu::check_attention()
{
    attention_ = false;
    if (stop_requested_ || state_ == RvState::Halted) {
        attention_ = true;
        return false;
    }
    if (count_entries_) {
        attention_ = true; // stay on the slow path to count
    }
    const int line = pending_interrupt();
    if (state_ == RvState::Sleeping) {
        if ((mip_ & (config_.mie_gates ? mie_ : 0xFFFFFFFFu)) == 0) {
            attention_ = true;
            return false; // still asleep: the run loop moves time on
        }
        state_ = RvState::Running; // WFI wakes on any enabled pending line, even with MIE off
    }
    if (line >= 0 && interrupt_wait_ > 0) {
        --interrupt_wait_; // one more instruction before the interrupt
        attention_ = true;
    } else if (line >= 0) {
        take_interrupt(line);
        attention_ = true; // the handler may have more to take
    }
    return true;
}

RvState RiscvCpu::run(std::uint64_t limit)
{
    stop_requested_ = false;
    attention_ = true;
    while (cycles_ < limit) {
        if (attention_) {
            if (!check_attention()) {
                if (!stop_requested_ && state_ != RvState::Running) {
                    cycles_ = limit; // asleep or halted: time goes on to the next event
                }
                break;
            }
            if (cycles_ >= limit) {
                break;
            }
        }
        const RvInsn& in = fetch();
        execute(in);
    }
    return state_;
}

void RiscvCpu::step_instruction()
{
    attention_ = true;
    took_interrupt_ = false;
    if (check_attention() && state_ == RvState::Running && !took_interrupt_) {
        const RvInsn& in = fetch();
        execute(in);
    }
    attention_ = true;
}

void RiscvCpu::execute(const RvInsn& in)
{
    ++cycles_;
    if (in.op == OP_NONE) { // the fetch failed
        const std::uint32_t address = pc_ + in.size;
        if ((mtvec_ & ~3u) == pc_ && mcause_ == InstructionAccessFault) {
            // The trap handler itself cannot be fetched: nothing can run.
            state_ = RvState::Halted;
            char text[96];
            std::snprintf(text, sizeof text, "No code at the trap vector 0x%08X: the core stopped", static_cast<unsigned>(pc_));
            halt_reason_ = text;
            attention_ = true;
            return;
        }
        next_pc_ = pc_;
        trap(InstructionAccessFault, address);
        pc_ = next_pc_;
        trapped_ = false;
        return;
    }
    if (exec_hi_ != 0xFFFFFFFFu && !fetch_allowed(pc_, in.size)) {
        next_pc_ = pc_;
        trap(InstructionAccessFault, pc_);
        pc_ = next_pc_;
        trapped_ = false;
        return;
    }
    if (count_entries_) {
        ++entry_hits_[in.entry];
    }
    const std::uint32_t pc = pc_;
    next_pc_ = pc + in.size;
    std::array<std::uint32_t, 33>& x = x_;
    const std::uint32_t a = x[in.rs1];
    const std::uint32_t b = x[in.rs2];
    const std::uint32_t imm = static_cast<std::uint32_t>(in.imm);
    switch (in.op) {
    case OP_LUI: x[in.rd] = imm; break;
    case OP_AUIPC: x[in.rd] = pc + imm; break;
    case OP_JAL:
        x[in.rd] = next_pc_;
        next_pc_ = pc + imm;
        break;
    case OP_JALR: {
        const std::uint32_t target = (a + imm) & ~1u; // read rs1 before writing rd
        x[in.rd] = next_pc_;
        next_pc_ = target;
        break;
    }
    case OP_BEQ: if (a == b) next_pc_ = pc + imm; break;
    case OP_BNE: if (a != b) next_pc_ = pc + imm; break;
    case OP_BLT: if (static_cast<std::int32_t>(a) < static_cast<std::int32_t>(b)) next_pc_ = pc + imm; break;
    case OP_BGE: if (static_cast<std::int32_t>(a) >= static_cast<std::int32_t>(b)) next_pc_ = pc + imm; break;
    case OP_BLTU: if (a < b) next_pc_ = pc + imm; break;
    case OP_BGEU: if (a >= b) next_pc_ = pc + imm; break;
    case OP_LB: case OP_LH: case OP_LW: case OP_LBU: case OP_LHU: {
        static constexpr unsigned sizes[] = {1, 2, 4, 1, 2};
        const unsigned size = sizes[in.op - OP_LB];
        std::uint32_t value = 0;
        if (!load(a + imm, size, value)) {
            break;
        }
        if (in.op == OP_LB) {
            value = static_cast<std::uint32_t>(sign_extend(value, 8));
        } else if (in.op == OP_LH) {
            value = static_cast<std::uint32_t>(sign_extend(value, 16));
        }
        x[in.rd] = value;
        break;
    }
    case OP_SB: store(a + imm, 1, b & 0xFF); break;
    case OP_SH: store(a + imm, 2, b & 0xFFFF); break;
    case OP_SW: store(a + imm, 4, b); break;
    case OP_ADDI: x[in.rd] = a + imm; break;
    case OP_SLTI: x[in.rd] = static_cast<std::int32_t>(a) < in.imm ? 1 : 0; break;
    case OP_SLTIU: x[in.rd] = a < imm ? 1 : 0; break;
    case OP_XORI: x[in.rd] = a ^ imm; break;
    case OP_ORI: x[in.rd] = a | imm; break;
    case OP_ANDI: x[in.rd] = a & imm; break;
    case OP_SLLI: x[in.rd] = a << imm; break;
    case OP_SRLI: x[in.rd] = a >> imm; break;
    case OP_SRAI: x[in.rd] = static_cast<std::uint32_t>(static_cast<std::int32_t>(a) >> imm); break;
    case OP_ADD: x[in.rd] = a + b; break;
    case OP_SUB: x[in.rd] = a - b; break;
    case OP_SLL: x[in.rd] = a << (b & 31); break;
    case OP_SLT: x[in.rd] = static_cast<std::int32_t>(a) < static_cast<std::int32_t>(b) ? 1 : 0; break;
    case OP_SLTU: x[in.rd] = a < b ? 1 : 0; break;
    case OP_XOR: x[in.rd] = a ^ b; break;
    case OP_SRL: x[in.rd] = a >> (b & 31); break;
    case OP_SRA: x[in.rd] = static_cast<std::uint32_t>(static_cast<std::int32_t>(a) >> (b & 31)); break;
    case OP_OR: x[in.rd] = a | b; break;
    case OP_AND: x[in.rd] = a & b; break;
    case OP_MUL: x[in.rd] = a * b; break;
    case OP_MULH:
        x[in.rd] = static_cast<std::uint32_t>(static_cast<std::uint64_t>(
            static_cast<std::int64_t>(static_cast<std::int32_t>(a)) * static_cast<std::int32_t>(b)) >> 32);
        break;
    case OP_MULHSU:
        x[in.rd] = static_cast<std::uint32_t>(static_cast<std::uint64_t>(
            static_cast<std::int64_t>(static_cast<std::int32_t>(a)) * static_cast<std::int64_t>(b)) >> 32);
        break;
    case OP_MULHU: x[in.rd] = static_cast<std::uint32_t>((static_cast<std::uint64_t>(a) * b) >> 32); break;
    case OP_DIV: {
        const std::int32_t sa = static_cast<std::int32_t>(a);
        const std::int32_t sb = static_cast<std::int32_t>(b);
        if (sb == 0) {
            x[in.rd] = 0xFFFFFFFFu;
        } else if (sa == std::numeric_limits<std::int32_t>::min() && sb == -1) {
            x[in.rd] = a;
        } else {
            x[in.rd] = static_cast<std::uint32_t>(sa / sb);
        }
        break;
    }
    case OP_DIVU: x[in.rd] = b == 0 ? 0xFFFFFFFFu : a / b; break;
    case OP_REM: {
        const std::int32_t sa = static_cast<std::int32_t>(a);
        const std::int32_t sb = static_cast<std::int32_t>(b);
        if (sb == 0) {
            x[in.rd] = a;
        } else if (sa == std::numeric_limits<std::int32_t>::min() && sb == -1) {
            x[in.rd] = 0;
        } else {
            x[in.rd] = static_cast<std::uint32_t>(sa % sb);
        }
        break;
    }
    case OP_REMU: x[in.rd] = b == 0 ? a : a % b; break;
    case OP_FENCE: break;
    case OP_FENCE_I:
        invalidate_code_cache();
        break;
    case OP_ECALL: trap(priv_ == Privilege::User ? EcallFromU : EcallFromM, 0); break;
    case OP_EBREAK: trap(Breakpoint, pc); break;
    case OP_MRET:
        if (priv_ != Privilege::Machine) {
            trap(IllegalInstruction, in.raw);
        } else {
            mret();
        }
        break;
    case OP_WFI:
        if (priv_ != Privilege::Machine) {
            trap(IllegalInstruction, in.raw); // no supervisor mode: WFI is machine-only here
        } else if ((mip_ & (config_.mie_gates ? mie_ : 0xFFFFFFFFu)) == 0) {
            state_ = RvState::Sleeping;
            attention_ = true;
        }
        break;
    case OP_CSRRW: case OP_CSRRS: case OP_CSRRC: case OP_CSRRWI: case OP_CSRRSI: case OP_CSRRCI:
        csr_instruction(in);
        break;
    default: // OP_ILLEGAL
        trap(IllegalInstruction, in.raw);
        break;
    }
    if (trapped_) {
        trapped_ = false; // the instruction did not retire
    } else {
        ++instructions_;
    }
    pc_ = next_pc_;
}

} // namespace simnodus::emu
