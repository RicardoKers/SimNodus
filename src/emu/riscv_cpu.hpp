// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// RV32IMC core of the SimNodus emulator (experiment E-08, the ESP32-C3's
// core): interpreter with predecoded instructions, Zicsr, Zifencei, machine
// and user modes, physical memory protection and traps. Written from The
// RISC-V Instruction Set Manual, volume I (unprivileged, 20191213) and
// volume II (privileged, 20211203). Interrupt lines come from the machine
// (the ESP32-C3's interrupt controller, or the test machine's timer); one
// instruction takes one cycle, an approximation. See
// docs/experiments/E-08-plan.md.
#pragma once

#include "bus.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace simnodus::emu {

// One decoded instruction. Compressed instructions decode to the operation of
// their 32-bit expansion; `entry` keeps the decoder entry for coverage counts.
struct RvInsn {
    std::uint8_t op = 0;    // RvOp (riscv_cpu.cpp); 0 = not decoded yet
    std::uint8_t entry = 0; // decoder entry (RvEntry), for the coverage counters
    std::uint8_t size = 4;  // bytes
    std::uint8_t rd = 0;    // 32 stands for x0 as a destination (writes are dropped)
    std::uint8_t rs1 = 0;
    std::uint8_t rs2 = 0;
    std::uint16_t csr = 0;
    std::int32_t imm = 0;
    std::uint32_t raw = 0;  // the instruction bits (16 for compressed), for mtval
};

enum class RvState { Running, Sleeping, Halted };

class RiscvCpu {
public:
    // Trap causes (mcause without the interrupt bit).
    enum Cause : std::uint32_t {
        InstructionMisaligned = 0,
        InstructionAccessFault = 1,
        IllegalInstruction = 2,
        Breakpoint = 3,
        LoadMisaligned = 4,
        LoadAccessFault = 5,
        StoreMisaligned = 6,
        StoreAccessFault = 7,
        EcallFromU = 8,
        EcallFromM = 11,
    };
    enum class Privilege { User = 0, Machine = 3 };

    struct Config {
        std::uint32_t reset_pc = 0;
        std::uint32_t mvendorid = 0;
        std::uint32_t marchid = 0;
        std::uint32_t mimpid = 0;
        // Interrupt lines (bit n = mcause n) whose mie bits are writable.
        std::uint32_t interrupt_lines = (1u << 3) | (1u << 7) | (1u << 11);
        bool counters = true;      // mcycle, minstret, cycle, instret and mcounteren
        bool vectored_only = false; // mtvec.MODE reads as 1 whatever is written
    };

    explicit RiscvCpu(Bus& bus);
    RiscvCpu(Bus& bus, const Config& config);
    RiscvCpu(const RiscvCpu&) = delete;
    RiscvCpu& operator=(const RiscvCpu&) = delete;

    // Reset: machine mode, PC at reset_pc, MIE and the PMP cleared.
    void reset();

    // Executes instructions until `cycles()` reaches `limit`, the core sleeps
    // (WFI with no enabled interrupt pending) or halts, or request_stop() was
    // called. Sleeping moves time to `limit`; the machine sets `limit` to its
    // next event.
    RvState run(std::uint64_t limit);
    void request_stop()
    {
        attention_ = true;
        stop_requested_ = true;
    }
    // Executes one instruction (or takes one pending interrupt).
    void step_instruction();

    std::uint64_t cycles() const { return cycles_; }
    std::uint64_t instructions() const { return instructions_; }
    void advance_idle(std::uint64_t to);
    RvState state() const { return state_; }

    // Code in plain memory that the core caches decoded. Stores by the core
    // keep the cache right; the machine calls invalidate_code_cache() after
    // other writes (loader, DMA, a remapped cache).
    void add_code_region(std::uint32_t base, std::uint32_t size);
    void invalidate_code_cache();

    // Levels of the interrupt lines (bit n = mcause n); mip reads them.
    void set_interrupt_lines(std::uint32_t lines);
    std::uint32_t interrupt_lines() const { return mip_; }
    // Chooses which pending and enabled line to take (the ESP32-C3's
    // priorities and threshold); returns -1 for none. Without it, the
    // architecture's order: 11, 3, 7, then the lowest number.
    std::function<int(std::uint32_t pending_enabled)> choose_interrupt;
    // CSRs the architecture does not define (vendor CSRs); return false for
    // a CSR that does not exist (an illegal instruction).
    std::function<bool(std::uint16_t csr, std::uint32_t& value)> read_custom_csr;
    std::function<bool(std::uint16_t csr, std::uint32_t value)> write_custom_csr;
    // Called on every trap before the PC moves to the handler.
    std::function<void(std::uint32_t mcause, std::uint32_t mepc, std::uint32_t mtval)> on_trap;

    // Registers for tests, diagnostics and the debugger.
    std::uint32_t reg(int n) const { return n == 0 ? 0 : x_[static_cast<std::size_t>(n)]; }
    void set_reg(int n, std::uint32_t value);
    std::uint32_t pc() const { return pc_; }
    void set_pc(std::uint32_t value);
    Privilege privilege() const { return priv_; }
    // CSR access as the debugger sees it (no privilege checks, no side effects
    // on the run state); false for a CSR that does not exist.
    bool read_csr(std::uint16_t csr, std::uint32_t& value);
    bool write_csr(std::uint16_t csr, std::uint32_t value);
    // The reason the core halted (empty while it runs).
    const std::string& halt_reason() const { return halt_reason_; }

    // Decoding, exposed for tests and the decoder coverage counters.
    static RvInsn decode(std::uint32_t bits);
    static const char* entry_name(std::uint8_t entry);
    static int entry_count();
    const std::vector<std::uint64_t>& entry_hits() const { return entry_hits_; }
    void count_entries(bool enable)
    {
        count_entries_ = enable;
        if (enable) {
            entry_hits_.assign(static_cast<std::size_t>(entry_count()), 0);
        }
        attention_ = true;
    }

private:
    struct CodeRegion {
        std::uint32_t base = 0;
        std::uint32_t size = 0;
        const std::uint8_t* bytes = nullptr;
        std::vector<RvInsn> insns; // per halfword
    };

    const RvInsn& fetch();
    void execute(const RvInsn& in);
    bool check_attention(); // false: stop the run loop
    void trap(std::uint32_t cause, std::uint32_t tval);
    void take_interrupt(int line);
    int pending_interrupt() const; // enabled line that may be taken now, or -1
    void mret();
    void csr_instruction(const RvInsn& in);
    bool csr_read(std::uint16_t csr, std::uint32_t& value) const;
    bool csr_write(std::uint16_t csr, std::uint32_t value);

    bool load(std::uint32_t address, unsigned size, std::uint32_t& value);
    bool store(std::uint32_t address, unsigned size, std::uint32_t value);
    void invalidate_code(std::uint32_t address, unsigned size);

    // Physical memory protection.
    enum PmpAccess : std::uint8_t { PmpRead = 1, PmpWrite = 2, PmpExec = 4 };
    bool pmp_allows(std::uint32_t address, unsigned size, PmpAccess access, Privilege priv) const;
    bool fetch_allowed(std::uint32_t address, unsigned size);
    void pmp_changed();
    Privilege data_privilege() const; // MPRV makes loads and stores use MPP

    Bus& bus_;
    Config config_;

    std::array<std::uint32_t, 33> x_{}; // x_[32] takes writes to x0
    std::uint32_t pc_ = 0;
    std::uint32_t next_pc_ = 0;
    Privilege priv_ = Privilege::Machine;

    // CSRs.
    bool mie_bit_ = false, mpie_ = false, mprv_ = false, tw_ = false;
    Privilege mpp_ = Privilege::Machine;
    std::uint32_t mie_ = 0, mip_ = 0, mtvec_ = 0, mscratch_ = 0, mepc_ = 0, mcause_ = 0, mtval_ = 0;
    std::uint32_t mcounteren_ = 0;
    std::uint64_t cycle_offset_ = 0, instret_offset_ = 0;
    std::array<std::uint8_t, 16> pmpcfg_{};
    std::array<std::uint32_t, 16> pmpaddr_{};
    bool pmp_check_ = false;  // data accesses need checks (user mode, MPRV or a locked entry)
    std::uint32_t exec_lo_ = 0, exec_hi_ = 0; // fetches already allowed: [lo, hi)

    // Run state.
    std::uint64_t cycles_ = 0;
    std::uint64_t instructions_ = 0;
    RvState state_ = RvState::Running;
    bool attention_ = true;
    bool stop_requested_ = false;
    bool trapped_ = false; // set by a trap inside the current instruction
    bool took_interrupt_ = false;
    std::string halt_reason_;

    std::vector<CodeRegion> regions_;
    CodeRegion* last_region_ = nullptr; // where the previous fetch was
    RvInsn scratch_;
    bool count_entries_ = false;
    std::vector<std::uint64_t> entry_hits_;
};

} // namespace simnodus::emu
