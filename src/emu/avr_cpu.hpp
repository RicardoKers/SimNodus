// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// AVRe+ core of the SimNodus emulator (experiment E-10: the ATmega8, then the
// ATmega32 and ATmega328/328P): interpreter with predecoded instructions and
// the cycle counts of the AVR Instruction Set Manual (DS40002198C, the AVRe
// column, which the ATmega8A datasheet DS40001974B repeats). The 32
// registers, the I/O registers and the SRAM share one data space; SREG and
// the stack pointer live in the core, every other I/O register in the
// machine (read_io/write_io). Interrupts come from the machine: it says when
// one is pending and enabled, the core takes the lowest vector after the
// instruction in progress. See docs/experiments/E-10-plan.md.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace simnodus::emu {

// The parts of the core that differ between AVR chips.
struct AvrConfig {
    std::uint32_t flash_words = 4096; // ATmega8: 8 KB; a power of two
    std::uint16_t data_size = 0x460;  // registers, I/O and SRAM (ATmega8: 0x0000-0x045F)
    std::uint16_t io_end = 0x5F;      // last I/O address (0xFF with extended I/O)
    std::uint16_t sp_reset = 0;       // the ATmega8 resets SP to 0; later chips to RAMEND
    bool jmp_call = false;            // JMP and CALL (chips above 8 KB of flash)
    bool break_instruction = false;   // BREAK (chips with on-chip debugging)
    unsigned vector_words = 1;        // 1 (RJMP vectors) or 2 (JMP vectors)
};

// One decoded instruction.
struct AvrInsn {
    std::uint8_t op = 0;   // AvrOp (avr_cpu.cpp); 0 = not decoded yet
    std::uint8_t d = 0;    // destination register, or the bit/flag number
    std::uint8_t r = 0;    // source register, or the I/O address / bit
    std::uint8_t size = 1; // words
    std::uint16_t k = 0;   // constant, address or displacement
    std::uint16_t raw = 0;
};

enum class AvrState { Running, Sleeping, Halted };

class AvrCpu {
public:
    // SREG bits.
    enum Flag : std::uint8_t { C = 0, Z = 1, N = 2, V = 3, S = 4, H = 5, T = 6, I = 7 };
    static constexpr std::uint16_t sreg_address = 0x5F;
    static constexpr std::uint16_t spl_address = 0x5D;
    static constexpr std::uint16_t sph_address = 0x5E;

    explicit AvrCpu(const AvrConfig& config = {});
    AvrCpu(const AvrCpu&) = delete;
    AvrCpu& operator=(const AvrCpu&) = delete;

    const AvrConfig& config() const { return config_; }

    // Reset: PC to the reset vector (0, or the boot section's start that the
    // machine sets with set_reset_pc), SREG 0, SP to its reset value. The
    // registers and the SRAM keep their contents, as on the chip.
    void reset();
    void set_reset_pc(std::uint32_t word) { reset_pc_ = word; }

    // Executes instructions until cycles() reaches `limit`, the core sleeps or
    // halts, or request_stop() was called. Sleeping moves time to `limit`;
    // the machine sets `limit` to its next event.
    AvrState run(std::uint64_t limit);
    void request_stop() { stop_requested_ = true; }
    // Executes one instruction, or takes one pending interrupt.
    void step_instruction();

    std::uint64_t cycles() const { return cycles_; }
    void set_cycles(std::uint64_t cycles) { cycles_ = cycles; }
    std::uint64_t instructions() const { return instructions_; }
    AvrState state() const { return state_; }
    void wake() { state_ = state_ == AvrState::Sleeping ? AvrState::Running : state_; }
    void halt(const std::string& reason);
    const std::string& halt_reason() const { return halt_reason_; }

    // Program memory, in words. Writes drop the decoded instructions there.
    void load_flash(const std::vector<std::uint16_t>& words, std::uint32_t at = 0);
    std::uint16_t flash_word(std::uint32_t word) const { return flash_[word & flash_mask_]; }
    void write_flash_word(std::uint32_t word, std::uint16_t value);
    std::uint8_t flash_byte(std::uint32_t byte) const
    {
        const std::uint16_t w = flash_word(byte >> 1);
        return static_cast<std::uint8_t>((byte & 1u) ? w >> 8 : w);
    }

    // The data space as the debugger and tests see it (no side effects:
    // I/O registers read and write the core's copy).
    std::uint8_t reg(int n) const { return data_[static_cast<std::size_t>(n)]; }
    void set_reg(int n, std::uint8_t value) { data_[static_cast<std::size_t>(n)] = value; }
    std::uint8_t sreg() const { return sreg_; }
    void set_sreg(std::uint8_t value) { sreg_ = value; }
    std::uint16_t sp() const { return sp_; }
    void set_sp(std::uint16_t value) { sp_ = static_cast<std::uint16_t>(value & sp_mask_); }
    std::uint32_t pc() const { return pc_; }
    void set_pc(std::uint32_t word) { pc_ = word & flash_mask_; }
    std::uint8_t peek(std::uint16_t address) const;
    void poke(std::uint16_t address, std::uint8_t value);

    // The data space as instructions see it: registers, SREG and SP in the
    // core, other I/O through the machine, then SRAM.
    std::uint8_t read_data(std::uint16_t address);
    void write_data(std::uint16_t address, std::uint8_t value);

    // ---- The machine's side ----
    // I/O registers other than SREG and SP (data addresses 0x20..io_end).
    // Without them, I/O addresses are plain storage.
    std::function<std::uint8_t(std::uint16_t address)> read_io;
    std::function<void(std::uint16_t address, std::uint8_t value)> write_io;
    // Interrupts: the machine calls set_interrupt_pending(true) while some
    // interrupt is flagged and enabled; the core then asks for the vector
    // (the lowest pending one) and tells which one it took, so the machine
    // clears the flag that the hardware clears.
    void set_interrupt_pending(bool pending) { interrupt_pending_ = pending; }
    std::function<int()> pending_vector;
    std::function<void(int vector)> interrupt_taken;
    // Vector table start in words (IVSEL moves it to the boot section).
    void set_vector_base(std::uint32_t word) { vector_base_ = word; }
    // SLEEP: returns true when the sleep enable bit is set (the core then
    // sleeps until the machine's interrupt wakes it).
    std::function<bool()> on_sleep;
    std::function<void()> on_wdr;
    // SPM with Z and r1:r0; returns the cycles it takes beyond the first.
    std::function<unsigned(std::uint16_t z, std::uint16_t r1r0)> on_spm;
    // LPM reads that the machine answers itself (fuse, lock and signature
    // bytes after BLBSET or SIGRD); return false to read the flash.
    std::function<bool(std::uint16_t z, std::uint8_t& value)> lpm_read;
    std::function<void()> on_break;

    // Debugger (GDB server, SN-262), as on the other cores. A breakpoint
    // (a word address) halts the core before its instruction; a data access
    // that overlaps a watchpoint (data addresses) halts it after its
    // instruction; with a debugger attached, BREAK halts it after the BREAK
    // instead of calling on_break. debug_halted() is then true and run()
    // returns until resume_from_halt().
    enum class WatchKind { Write = 2, Read = 3, Access = 4 }; // GDB's Z2, Z3, Z4
    struct Watchpoint {
        std::uint32_t address = 0;
        std::uint32_t length = 1;
        WatchKind kind = WatchKind::Write;
    };
    struct WatchHit {
        bool hit = false;
        std::uint32_t address = 0;
        WatchKind kind = WatchKind::Write;
    };
    void set_debugger_attached(bool attached);
    void set_breakpoints(std::vector<std::uint32_t> words);
    void set_watchpoints(std::vector<Watchpoint> watchpoints);
    const WatchHit& last_watch() const { return watch_hit_; }
    bool debug_halted() const { return debug_halted_; }
    // From a halt: the instruction at the PC runs even if it has a breakpoint.
    void resume_from_halt();
    // Diagnostics: an opcode this chip does not have, an access outside the
    // data space.
    std::function<void(const std::string& text)> diagnostic;

    // Decoding, exposed for tests and the decoder coverage counters.
    AvrInsn decode(std::uint16_t word, std::uint16_t next) const;
    static const char* op_name(std::uint8_t op);
    static int op_count();
    const std::vector<std::uint64_t>& op_hits() const { return op_hits_; }
    void count_ops(bool enable);

private:
    void execute(const AvrInsn& in);
    bool take_interrupt();
    const AvrInsn& fetch();
    void push(std::uint8_t value);
    std::uint8_t pop();
    void push_pc(std::uint32_t pc);
    std::uint32_t pop_pc();
    void skip_next();
    void set_flag(Flag f, bool on)
    {
        sreg_ = static_cast<std::uint8_t>(on ? (sreg_ | (1u << f)) : (sreg_ & ~(1u << f)));
    }
    bool flag(Flag f) const { return (sreg_ >> f) & 1u; }
    std::uint16_t pair(int low) const
    {
        return static_cast<std::uint16_t>(data_[static_cast<std::size_t>(low)] | (data_[static_cast<std::size_t>(low) + 1] << 8));
    }
    void set_pair(int low, std::uint16_t value)
    {
        data_[static_cast<std::size_t>(low)] = static_cast<std::uint8_t>(value);
        data_[static_cast<std::size_t>(low) + 1] = static_cast<std::uint8_t>(value >> 8);
    }
    void undefined(const AvrInsn& in);
    void check_watch(std::uint16_t address, bool write);

    AvrConfig config_;
    std::uint32_t flash_mask_;
    std::uint16_t sp_mask_;
    std::vector<std::uint16_t> flash_;
    std::vector<AvrInsn> decoded_;
    std::vector<std::uint8_t> data_;
    std::uint8_t sreg_ = 0;
    std::uint16_t sp_ = 0;
    std::uint32_t pc_ = 0;
    std::uint32_t reset_pc_ = 0;
    std::uint32_t vector_base_ = 0;
    std::uint64_t cycles_ = 0;
    std::uint64_t instructions_ = 0;
    AvrState state_ = AvrState::Running;
    std::string halt_reason_;
    bool stop_requested_ = false;
    bool interrupt_pending_ = false;
    bool inhibit_interrupt_ = false; // one instruction after SEI and RETI
    bool count_ops_ = false;
    std::vector<std::uint64_t> op_hits_;
    bool debugger_ = false;
    bool debug_halted_ = false;
    bool skip_breakpoint_ = false; // the first instruction after a resume
    std::vector<std::uint32_t> breakpoints_;
    std::vector<Watchpoint> watchpoints_;
    WatchHit watch_hit_;
};

} // namespace simnodus::emu
