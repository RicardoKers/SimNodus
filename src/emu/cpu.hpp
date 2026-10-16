// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Cortex-M3 (ARMv7-M) core of the SimNodus STM32 emulator: Thumb-2
// interpreter, exception model (NVIC priorities, stacking, tail-chaining by
// re-evaluation, faults, lockup) and the system control space (NVIC, SCB,
// SysTick). Written from the ARMv7-M Architecture Reference Manual
// (DDI 0403E) and the Cortex-M3 Technical Reference Manual (r1p1, the core of
// the STM32F103). See docs/architecture/MCU_EMULATOR.md, section 4.
#pragma once

#include "bus.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace simnodus::emu {

// One decoded instruction. Decoding never depends on processor state (the
// IT condition and "set flags outside an IT block" are resolved at execution).
struct Insn {
    std::uint8_t op = 0;    // Op (cpu.cpp); 0 = not decoded yet
    std::uint8_t size = 2;  // bytes
    std::uint8_t alu = 0;   // data-processing operation, or condition for branches
    std::uint8_t s = 0;     // flags: 0 no, 1 yes, 2 only outside an IT block
    std::uint8_t rd = 0;
    std::uint8_t rn = 0;
    std::uint8_t rm = 0;
    std::uint8_t ra = 0;    // accumulator, Rt2, RdHi or shift register
    std::uint8_t shift_t = 0; // 0 LSL, 1 LSR, 2 ASR, 3 ROR, 4 RRX
    std::uint8_t shift_n = 0;
    std::int8_t carry = -1; // immediate's carry-out: -1 unchanged, 0 or 1
    std::uint8_t mode = 0;  // memory: size and flags; others: variant
    std::uint32_t imm = 0;
};

enum class CoreState { Running, Sleeping, Lockup };

class Cpu {
public:
    static constexpr int num_irqs = 68;               // STM32F103 uses 0..42; NVIC sized for 68
    static constexpr int num_exceptions = 16 + num_irqs;

    explicit Cpu(Bus& bus);
    Cpu(const Cpu&) = delete;
    Cpu& operator=(const Cpu&) = delete;

    // Power-on or system reset: NVIC/SCB/SysTick to reset values, SP and PC from
    // the vector table at address 0.
    void reset();

    // Executes instructions until `cycles()` reaches `limit`, the core sleeps or
    // locks up, or `request_stop()` was called. Returns the state afterwards.
    CoreState run(std::uint64_t limit);
    void request_stop() { attention_ = true; stop_requested_ = true; }

    std::uint64_t cycles() const { return cycles_; }
    // Moves time forward while the core sleeps (no instructions execute).
    void advance_idle(std::uint64_t to);
    CoreState state() const { return state_; }

    // Interrupt line from a peripheral (level sensitive, as on the chip).
    void set_irq_line(int irq, bool level);
    // Pending bit of exception `number` (16 + IRQ for interrupts).
    void set_pending(int number);

    // Registers for tests, diagnostics and the debugger.
    std::uint32_t reg(int n) const;
    void set_reg(int n, std::uint32_t value);
    std::uint32_t pc() const { return pc_; }
    std::uint32_t xpsr() const;
    std::uint32_t msp() const;
    std::uint32_t psp() const;
    std::uint32_t primask() const { return primask_; }
    std::uint32_t control() const;
    int active_exception() const { return ipsr_; }
    std::uint64_t instructions() const { return instructions_; }

    // SysTick: called by the machine when its next event is due.
    std::uint64_t systick_next_event() const;
    void systick_update(std::uint64_t now);

    // Fault or lockup description for diagnostics (empty if none).
    const std::string& last_fault() const { return last_fault_; }

    // Debugger (GDB server, SN-180). With breakpoints set, run() stops before
    // executing an instruction at one of them (debug_halted() is then true).
    // With a debugger attached, BKPT halts the core
    // instead of raising a HardFault.
    void set_debugger_attached(bool attached);
    void set_breakpoints(std::vector<std::uint32_t> addresses);
    bool debug_halted() const { return debug_halted_; }
    // Resumes after a halt: the next run() executes the instruction at the PC
    // even if it has a breakpoint (otherwise it would halt again at once).
    void resume_from_halt();
    // Executes one instruction (or the entry into a pending exception).
    void step_instruction();
    void set_xpsr(std::uint32_t value); // the APSR flags, EPSR.T and IT bits (IPSR is read-only)
    // Code in memory changed outside the CPU's stores (debugger, loader).
    void invalidate_code_cache();

    // Called when the firmware requests a system reset (AIRCR.SYSRESETREQ).
    std::function<void()> on_system_reset;
    // Called when the core enters a fault handler (HardFault, MemManage, BusFault,
    // UsageFault), before the PC moves to the handler; the text says why.
    std::function<void(int number, const std::string& why)> on_fault;
    // Called when the next time event may have changed (SysTick reprogrammed).
    std::function<void()> on_schedule_changed;

    // Decoding, exposed for tests and the decoder coverage counters.
    static Insn decode(std::uint32_t hw1, std::uint32_t hw2);
    static const char* op_name(std::uint8_t op);
    static int op_count();
    const std::vector<std::uint64_t>& op_hits() const { return op_hits_; }
    void count_ops(bool enable) { count_ops_ = enable; if (enable) op_hits_.assign(op_count(), 0); }

    // System control space (0xE000E000, 4 KiB) as a bus device.
    Device& scs() { return scs_; }

private:
    friend class Scs;
    class Scs : public Device {
    public:
        explicit Scs(Cpu& cpu) : cpu_(cpu) {}
        const char* name() const override { return "SCS (NVIC, SysTick, SCB)"; }
        bool read(std::uint32_t offset, unsigned size, std::uint32_t& value) override;
        bool write(std::uint32_t offset, unsigned size, std::uint32_t value) override;
    private:
        Cpu& cpu_;
    };

    // Execution.
    void step();
    bool fast(const Insn& insn); // inline forms of the run loop; false: use step()
    void execute(const Insn& insn);
    const Insn& fetch_decode();
    bool condition_passed(unsigned cond) const;
    bool in_it_block() const { return (itstate_ & 0xF) != 0; }
    bool last_in_it_block() const { return (itstate_ & 0xF) == 0x8; }
    void it_advance();

    std::uint32_t read_reg(unsigned n) const; // R15 reads as the instruction address + 4
    void write_reg(unsigned n, std::uint32_t value);
    void branch_write_pc(std::uint32_t address);
    void bx_write_pc(std::uint32_t address); // interworking and exception return
    void set_sp(std::uint32_t value);
    void select_sp(bool process);

    bool mem_read(std::uint32_t address, unsigned size, std::uint32_t& value);
    bool mem_write(std::uint32_t address, unsigned size, std::uint32_t value);

    // Exceptions.
    void raise_usage_fault(std::uint32_t cfsr_bit, const char* what);
    void raise_bus_fault(std::uint32_t bfsr_bit, std::uint32_t address, bool address_valid, const char* what);
    void fault(int number, const std::string& what); // escalates as the architecture says
    void lockup(const std::string& what);
    int group_priority(int number) const;
    int exception_priority(int number) const;
    int execution_priority() const;
    int pending_exception() const; // highest pending that may preempt, or 0
    void take_exception(int number, std::uint32_t return_address);
    void exception_return(std::uint32_t exc_return);
    void check_exceptions();
    std::uint32_t vector_table() const { return vtor_; }

    // SysTick.
    std::uint32_t systick_value(std::uint64_t now) const;
    void systick_restart(std::uint64_t now, std::uint32_t value);

    // SCS register access.
    bool scs_read(std::uint32_t offset, std::uint32_t& value);
    bool scs_write(std::uint32_t offset, std::uint32_t value);

    Bus& bus_;
    Scs scs_{*this};

    // Core registers.
    std::array<std::uint32_t, 16> r_{};
    std::uint32_t pc_ = 0;      // address of the current instruction
    std::uint32_t next_pc_ = 0; // set by step(); branches overwrite it
    std::uint32_t sp_other_ = 0; // the stack pointer not selected now
    bool sp_process_ = false;    // R13 is PSP
    bool n_ = false, z_ = false, c_ = false, v_ = false, q_ = false;
    bool thumb_ = true;
    std::uint32_t itstate_ = 0;
    std::uint32_t ipsr_ = 0;
    std::uint32_t primask_ = 0, faultmask_ = 0, basepri_ = 0;
    bool npriv_ = false, spsel_ = false;

    // Exception state.
    std::array<bool, num_exceptions> pending_{};
    std::array<bool, num_exceptions> active_{};
    std::array<bool, num_irqs> irq_enabled_{};
    std::array<bool, num_irqs> irq_line_{};
    std::array<std::uint8_t, num_exceptions> priority_{}; // programmable priorities
    std::uint32_t vtor_ = 0, aircr_prigroup_ = 0, scr_ = 0, ccr_ = 0, shcsr_ = 0;
    std::uint32_t cfsr_ = 0, hfsr_ = 0, mmfar_ = 0, bfar_ = 0;
    bool monitor_ = false;
    bool event_register_ = false;

    // SysTick.
    std::uint32_t st_ctrl_ = 0, st_reload_ = 0;
    std::uint64_t st_base_ = 0;      // cycle at which the counter held st_start_
    std::uint32_t st_start_ = 0;
    bool st_countflag_ = false;
    std::uint64_t st_next_ = 0;      // cached systick_next_event()
    void systick_reschedule() { st_next_ = systick_next_event(); }

    // Debugger.
    bool is_breakpoint(std::uint32_t address) const;
    bool debugger_ = false;
    std::vector<std::uint32_t> breakpoints_;
    bool debug_halted_ = false;
    bool skip_breakpoint_ = false; // the instruction at the PC runs once despite a breakpoint

    // Run state.
    std::uint64_t cycles_ = 0;
    std::uint64_t instructions_ = 0;
    CoreState state_ = CoreState::Running;
    bool attention_ = true;
    bool stop_requested_ = false;
    bool faulted_ = false; // set by a fault inside the current instruction
    bool in_it_ = false;   // the current instruction executes inside an IT block
    std::string last_fault_;

    // Decoded instruction cache for code in the flash and its alias.
    std::vector<Insn> flash_cache_; // per halfword of 0x08000000..+cache size
    Insn scratch_;
    bool count_ops_ = false;
    std::vector<std::uint64_t> op_hits_;
};

} // namespace simnodus::emu
