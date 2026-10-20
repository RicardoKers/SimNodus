# SimNodus STM32 emulator (design)

Revision 2 — 2026-10-15 (review findings folded in). Status: **accepted** ([ADR 0089](../decisions/0089-own-stm32-emulator.md), 2026-10-15);
the prototype is experiment E-07 (SN-170). The delivery plan, gates and dates
are in [MCU_EMULATOR_PLAN.md](../planning/MCU_EMULATOR_PLAN.md).

Update 2026-10-18: the emulator is the default engine since
[ADR 0090](../decisions/0090-own-emulator-default.md) and the only one since
[ADR 0094](../decisions/0094-remove-renode.md), which removed Renode. The
comparisons with Renode below (sections 1 and 9) are the record of how it was
chosen and validated.

SimNodus ran microcontroller firmware in Renode 1.16.1 when this was written
(ADR 0082–0088). This document designs a replacement written for SimNodus: an in-process
emulator of the STM32F103, built so that further STM32 chips (STM32F407 first)
reuse its core and peripheral models.

## 1. Why an own emulator

Measured limits of the Renode path, each traced to an ADR:

| Limit today | Evidence | With an own emulator |
|---|---|---|
| Renode starts in 7–9 s on every run | ADR 0082, package checks | Load and reset of an ELF in milliseconds |
| Firmware and circuit exchange data in quanta (1 ms default); timestamps are rounded to Renode's 100 µs time step; events arrive only during `RunFor` and sometimes one call late | ADR 0083, 0085, PR #116 | Every pin change carries its exact cycle time |
| Peripherals move whole bytes; the TX pin carries no waveform; ADC, timer and PWM models need owned extensions or audits | ADR 0086, E-04, SN-140/141 | Peripherals are written against the pins: serial frames, PWM edges and ADC sampling instants are part of the model |
| One instruction per cycle; no cycle model | ADR 0088 | Instruction counting first, a Cortex-M3 cycle table later |
| 137 MB package (Renode with .NET); a child process per run with platform-specific lifetime handling | ADR 0082, SN-019 | A C++ library inside the engine; package size drops by about 100 MB once Renode is removed |
| Debugging needs a patched, cancellable Renode build | ADR 0014, SN-150 | A GDB server inside the emulator, pausing the whole simulation at a known time |

The costs are real too: the Thumb-2 instruction set, exceptions and every
peripheral must be written and validated by us. Section 9 describes the
validation; the plan kept Renode as the shipped engine until the emulator had
passed its gates (ADR 0090), and ADR 0094 then removed it.

## 2. Goals and non-goals

Goals:

1. Run unmodified STM32F103 ELF files: bare metal, STM32CubeIDE HAL and LL,
   libopencm3, FreeRTOS, newlib (including soft-float and `printf`).
2. Pin-level peripherals: every pin change has a virtual timestamp in CPU
   cycles, converted to nanoseconds.
3. Deterministic: the same ELF, inputs and settings give the same trace on
   every run and platform (no host time inside the emulator).
4. Fast: at least real time at 72 MHz on a 2026 laptop with headroom for the
   analog engine; idle firmware (`__WFI()`) costs almost nothing.
5. Clear diagnostics for students: faults and accesses to unsimulated
   features are reported with the instruction address and, when the ELF has
   symbols, the function name.
6. Extensible: chips are assembled from a core variant, a memory map and
   reusable STM32 peripheral blocks.
7. A GDB remote server for STM32CubeIDE debugging (MCU-08); `src/emu/gdb_server.*` since SN-180.

Non-goals, at least until after the classroom release:

- Cycle-exact timing (bus contention, prefetch buffer details, DMA arbitration).
- Electrical internals of peripherals beyond the pin model (the circuit side
  stays in ngspice).
- USB, CAN, Ethernet, SDIO, FSMC; flash programming (in-application
  programming), option bytes, read protection.
- A just-in-time compiler. An interpreter with a predecoded instruction cache
  is the design; a JIT is considered only if the performance gate fails.

## 3. Place in the architecture

```text
apps/simnodus (Qt)                      unchanged: talks to SimulationSession only
src/sim  SimulationSession ─────────────┐
                                         │ McuBackend interface (extracted from McuRunner)
          ┌──────────────────────────────┴───────────────────────────┐
src/mcu   (RenodeMcuRunner, removed by ADR 0094)    NativeMcuRunner (new)
                                                      │ owns one emu::Machine per board
                                                      │
src/emu   (new, Qt-free C++20, no third-party code)  ▼
          core/armv7m   registers, decoder, executor, exceptions, NVIC, SysTick,
                        SCB, DWT (CYCCNT), FPB (breakpoints)
          bus           address map, memories, MMIO dispatch, bit-band, faults
          sched         cycle counter, event queue, cycle <-> ns timebase
          devices/stm32 rcc_f1, flash_f1, pwr, gpio_f1, afio_f1, exti, usart,
                        tim (basic/general/advanced), adc_f1, dma_f1, iwdg, wwdg,
                        rtc_f1/bkp, crc, dbgmcu, uid
          chips         stm32f103 (xB/x8 variants): assembly of core + devices
          pins          chip pin states, timestamped change log, inputs
          elf           ELF32 loader, symbols
          gdb           remote serial protocol server (loopback only)
apps/     simnodus-emu  command-line runner: ELF + time -> pin/UART trace (tests, E-07)
```

Rules (same as the rest of `src/`): C++20, no Qt, builds and runs on Windows
and Linux, MIT, no code copied from QEMU, Unicorn (GPL), tlib (LGPL) or other
emulators; written from the ARMv7-M Architecture Reference Manual (DDI 0403E),
the Cortex-M3 Technical Reference Manual, the STM32F10xxx reference manual
(RM0008), the programming manual (PM0056) and the STM32F103 datasheet.

## 4. Core (ARMv7-M, Cortex-M3)

**State.** R0–R12, MSP/PSP with SP selection by CONTROL.SPSEL and mode, LR,
PC, xPSR (APSR flags N Z C V Q, IPSR, EPSR with T bit and ITSTATE), PRIMASK,
BASEPRI, FAULTMASK, CONTROL.

**Instruction set.** All ARMv7-M Thumb encodings except coprocessor and
floating-point instructions (the M3 has none): 16-bit Thumb, 32-bit Thumb-2
data processing (immediate, modified immediate, shifted register), loads and
stores of every addressing mode (including exclusive LDREX/STREX with a local
monitor), LDM/STM/PUSH/POP, branches (B, BL, BX, BLX, CBZ/CBNZ, TBB/TBH),
IT blocks, multiply and divide (MUL, MLA, MLS, UMULL, SMULL, UMLAL, SMLAL,
UDIV, SDIV with the DIV_0_TRP behavior), bitfield (BFI, BFC, UBFX, SBFX),
CLZ, RBIT, REV*, extend (SXTB, UXTH…), SSAT/USAT, barriers (DMB, DSB, ISB),
hints (NOP, YIELD, WFE, WFI, SEV), SVC, BKPT, MRS/MSR, CPS. Undefined
encodings raise UsageFault (or HardFault when it is disabled), as on the chip.

**Decoder and executor.** Instructions are decoded once into a compact record
(handler, operands, length) kept in a predecode cache indexed by halfword
address. Flash code is cached for the whole run; code executed from SRAM is
cached per page and the page is invalidated on any write to it. The executor
runs `run_until(cycle_limit)`: execute instructions until the limit or the next
scheduled event, checking a single "exception pending" flag after each one.
Dispatch goes through handler pointers stored in the decoded records, which
works on MSVC, GCC and Clang (computed goto is not available on MSVC).
Rules for the cache: a decoded record never contains the IT condition (the
condition comes from ITSTATE at execution); a 32-bit instruction that
straddles a cache page belongs to both pages; every write path invalidates
code pages: CPU stores, bit-band writes, DMA and the debugger.

**Exceptions.** Reset, NMI, HardFault (with escalation), MemManage (no MPU on
the F103, so only execute-never regions), BusFault, UsageFault (configurable
traps via CCR and SHCSR), SVCall, DebugMonitor (reserved), PendSV, SysTick and
the chip's external interrupts. Priority grouping (AIRCR.PRIGROUP), BASEPRI,
PRIMASK and FAULTMASK masking, 8-word stacking with CCR.STKALIGN,
EXC_RETURN handling, tail-chaining; late arrival is modeled when an
interrupt becomes pending during stacking. CFSR/HFSR/MMFAR/BFAR are set as
on the chip, so fault handlers and debuggers see real information. A fault
during a HardFault handler (or NMI) enters **lockup**, as on the chip: the
core stops fetching, the run reports it with the PC and function, and only a
reset leaves it. The exclusive monitor is cleared on exception entry and
return. Details of revision r1p1 (the STM32F103 core): CCR.STKALIGN resets to
0; LDRD/STRD and LDM/STM fault on unaligned addresses.

**System peripherals.** NVIC (ISER/ICER/ISPR/ICPR/IABR/IPR, STIR), SysTick
(clocked by HCLK or HCLK/8 with CLKSOURCE, exact RVR + 1 period, COUNTFLAG,
STK_CALIB as on the F103),
SCB (CPUID r1p1, VTOR, AIRCR with SYSRESETREQ, SCR sleep bits, CCR, SHPR,
SHCSR, fault registers), DWT (CYCCNT, used by some delay libraries), FPB
(breakpoints for GDB), ITM stimulus port 0 (optional, for `printf` over SWO).

**Sleep.** WFI and WFE with no pending event advance time directly to the next
scheduled event or input; the host thread does no work while the firmware
sleeps. WFI wakes on a pending interrupt even when PRIMASK masks it (the
handler then does not run); SCR.SEVONPEND makes pending interrupts wake WFE.

## 5. Bus and memory

- Address map of regions (flash with its 0x00000000 alias selected by the BOOT
  pins, SRAM, system memory stub, option bytes, peripherals, private peripheral
  bus). Accesses are 8, 16 or 32 bits with the chip's alignment rules
  (unaligned word access allowed for normal loads and stores, faulting for
  LDM/STM and exclusive accesses, or always when CCR.UNALIGN_TRP is set).
- Bit-band aliases for SRAM (0x22000000) and peripherals (0x42000000).
- An access to an address without a device raises a precise BusFault, as on
  the chip, and produces a diagnostic.
- Peripheral registers are described per device with reset values, access
  types (read-only, write-1-to-clear, read-clear) and callbacks.

## 6. Time

- The emulator's clock is a 64-bit count of CPU cycles. Peripherals schedule
  events in an ordered queue keyed by `(cycle, sequence)`: timer updates,
  UART bit boundaries, ADC end of conversion, SysTick wrap.
- Conversion to time: the RCC model defines HCLK and the bus clocks. The
  timebase is piecewise: when HCLK changes at cycle `c0` and time `t0`, later
  times are `t0 + (c - c0) / f`. Times are reported in nanoseconds (64-bit),
  matching the 1 ns `SimTime` of [SIMULATION_TIME.md](SIMULATION_TIME.md).
- Cycle model, in two steps:
  1. **One instruction per cycle** (as Renode, ADR 0088). Needed for the
     differential tests against Renode in E-07.
  2. **Cortex-M3 cycle table** (TRM): taken branches +1..3, loads 2, LDM/STM
     n + 1, multiply-long 3–5, divide 2–12, exception entry and return 12,
     plus flash wait states from FLASH_ACR with a simple prefetch
     approximation. Labeled approximate and validated against the owner's
     Blue Pill with DWT CYCCNT (plan N5).
- Determinism: no host time enters the emulator. Inputs carry the cycle (or
  nanosecond) at which they apply.
- Clock changes (RCC prescalers, PLL switch) reschedule the events already in
  the queue that were computed from the old frequency (timer updates, UART
  bit boundaries). Timer kernels run at 2x the APB clock when its prescaler is
  not 1, as on the chip.

## 7. Pins and coupling with the circuit

**Chip pins.** Each GPIO pin exposes its configuration (input floating, pull-up,
pull-down, analog, output push-pull or open-drain, alternate function push-pull
or open-drain, speed), its driven level and its source: the ODR bit or a
peripheral signal (USART TX, TIM channel, MCO). Every change is appended to a
per-pin log with its time in nanoseconds; the runner turns the log into the
`PinState`/`PinDrive` history the session already uses (ADR 0084).

**Inputs.** The session keeps converting circuit voltages to logic levels with
the Schmitt thresholds (MCU-04) and passes level changes with their circuit
times. Analog voltages for ADC channels are provided on request (below).

**Windows and determinism.** The firmware advances in windows of length `L`
granted by the circuit: it may run window k (from k·L to (k+1)·L) only after
the circuit has reached k·L. An input change the circuit produces at time
`tc` applies to the firmware at the next window boundary at or after `tc`.
Both rules depend only on simulated times, never on thread scheduling, so a
coupled run with the same inputs repeats exactly (today's rule, apply at the
circuit time if the firmware has not passed it and otherwise at once, depends
on scheduling). The cost is an input delay of up to `L`, labeled approximate;
without inter-process calls `L` can be much smaller than today's two 1 ms
quanta, and it is chosen by measurement in plan N1 (EQ-3).

**Edges in the analog engine.** Since SN-176 (ADR 0091) each circuit step
gets the pin's drive averaged over the step, so PWM averages are exact at the
100 µs step; an edge is spread over its step. Breakpoints at every edge were
measured and not taken (lesson 1 fell to 0.85x real time; numbers in ADR
0091). Serial frames (8.68 µs bits at 115200 baud) need more than averaging.
Candidate techniques for them, compared in the spike E-07b (SN-186):

- `ngSpice_SetBkpt(time)` for each upcoming edge: in the shared library it
  adds a timestep breakpoint (`CKTsetBreak`; checked in the ngspice source),
  so the solver steps on the edge without halting. Each breakpoint forces
  small steps around it, and S1 measured 0.6x real time at a 1 µs maximum
  step, so continuous 115200-baud traffic (up to about 11 500 edges per
  second) may cost too much.
- Edges with a finite rise and fall time taken from the datasheet for the
  pin's speed setting (2, 10, 50 MHz) instead of ideal steps, which also
  helps convergence.
- Injecting edges into the analog engine only for nets with analog loads;
  nets that only connect logic inputs are shown in the logic lanes.

Whatever is chosen, an ideal zero-time step is not injected. The behavior
across segment restarts (ADR 0080) is part of the spike.

**ADC sampling.** Implemented in SN-178 as ADR 0092 describes: the session
sends the wired pins' voltages at every accepted point (`set_analog`), and a
sample in window k reads the voltage at (k+1)L minus the lead and the maximum
step, a time the circuit has always passed, so no wait and no deadlock. The
original plan below (an `analog_at` call that waits) was dropped because the
wait can deadlock with ngspice's trial steps. Original text: A conversion
samples its channel at the end of the sampling time. The runner asks the
session for the node voltage at that instant (`analog_at(pin, t_ns)` in the
backend interface). The circuit's history is
kept for the window, so samples inside a window are read from it
(interpolated between accepted points) and the firmware waits at most once per
window, never once per sample, which would stop the lead at high sample rates.
The wait cannot deadlock: the circuit only waits for the firmware to reach the
circuit's own time. When the sample rate is too high for real time (continuous
or scan conversions near 1 MSPS), the model may use the latest committed value
and the run is labeled approximate. The code is quantized once, as in E-04.

**Serial frames.** The USART model drives TX bit by bit at the baud rate set
by BRR and the bus clock (start bit, 8 or 9 data bits, parity, stop bits) and
samples RX at the bit centers (16x or 8x oversampling reduced to the center
sample, with framing and parity flags). The terminal keeps receiving bytes;
the scope and logic lanes see the waveform. Two boards can talk through wires.

As built (SN-192): input changes carry the time the pin voltage crossed the
threshold (the step's start when it began at a firmware edge breakpoint,
otherwise interpolated between accepted points) besides the accepted point
that applies them. The USART keeps its RX pin's edges with those times and
decides a frame `uart_rx_lag_ns` after its stop bit's center (the ADC's lag
plus two windows), so every edge of the frame has arrived: the result does
not depend on thread timing. Bits shorter than four circuit steps are not
decoded without exact edges (a diagnostic says so). A RX pin on the same net
as another board's TX keeps the byte link and is not decoded twice.

## 8. Peripheral fidelity policy

- Every device lists, in its header and in MICROCONTROLLER.md, what it
  implements and what it does not.
- Registers have the documented reset values and access types.
- Using an unsimulated feature (for example TIM2 encoder mode before it exists)
  produces one diagnostic per feature and run, shown in the Messages panel:
  "Firmware enabled TIM2 encoder mode, which SimNodus does not simulate yet
  (PC 0x08000a3c, MX_TIM2_Init)".
- Diagnostics for real student mistakes the emulator would otherwise hide:
  FLASH_ACR latency too low for the configured SYSCLK (the chip would fail),
  a peripheral used with its clock disabled, a GPIO pin used by a peripheral
  but left in input mode.
- Known chip errata are reproduced where firmware sees them, for example
  DBGMCU_IDCODE reading 0 when no debugger is attached (to be confirmed on the
  owner's board).
- Clock gating is modeled where it is cheap and teaches something: a
  peripheral whose RCC enable bit is off ignores writes and reads 0, and a
  diagnostic says why (a common student mistake). This differs from Renode
  and is checked against the owner's board.
- F103 devices by tier:

| Tier | Devices | Needed for |
|---|---|---|
| 1 (parity with Renode) | RCC, FLASH interface (ACR), PWR, GPIOA–D, AFIO (EXTICR, remaps), EXTI, USART1–3, DBGMCU IDCODE, flash size and unique ID registers | E-07, N1 |
| 2 (lessons) | TIM1–4 (PWM, output compare, input capture, one-pulse; encoder later; done in SN-177, `src/emu/stm32f1_timer.*`), ADC1–2 (single, continuous, scan, sample times, injected; done in SN-178, `src/emu/stm32f1_adc.*`), DMA1 as HAL needs it (done in SN-179, `src/emu/stm32f1_dma.*`) | N3 |
| 2b (after the freeze) | IWDG, WWDG, RTC/BKP, CRC (done in SN-187, `src/emu/stm32f1_system.*`) | SN-187 |
| 3 (later) | SPI1–2 (master, done in SN-204, `src/emu/stm32f1_spi.*`), I2C1–2 (with digital parts in the library, SN-205), CAN, USB | after the classroom release |

## 9. Validation

1. **Instruction tests:** vectors written from the ARMv7-M pseudocode for every
   encoding, including flags, shifts by 0/32/over 32, saturation, IT blocks,
   unaligned and exclusive accesses, exception entry and return.
2. **Differential tests against Renode (oracle, both at one instruction per
   cycle):**
   - random instruction blocks loaded into SRAM, compared register by
     register, flag by flag and memory after each block;
   - whole-firmware traces (GPIO and UART events in order, timestamps within
     Renode's 100 µs reporting step) for every example and test firmware.
   (Done in E-07, results in [E-07-results.md](../experiments/E-07-results.md);
   the tests `emu-differential` and `emu-firmware` were removed with Renode by
   ADR 0094.)
   Renode is used only as a separate test oracle; differences are analyzed,
   not assumed to be our bugs (Renode has known deviations, ADR 0083/0085).
   A difference is accepted as a Renode deviation only when a third
   reference agrees with us: a pseudocode vector, QEMU (`mps2-an385`,
   Cortex-M3) run as a test-only process (no linking, so its GPL does not
   reach SimNodus), or the same block run on the owner's Blue Pill through
   ST-Link/OpenOCD. Coverage is measured with per-entry decoder hit counters,
   not by the number of blocks. Dumps are batched (one monitor script per
   batch of blocks) because each Renode monitor read costs about 0.1 ms.
3. **Firmware corpus:** the examples, STM32CubeIDE HAL and LL projects (the
   owner's projects included), a libopencm3 blink, a FreeRTOS demo (PSP,
   SVC, PendSV), newlib `printf` and soft-float, built with -O0, -O2 and -Os.
4. **Real board:** the owner's Blue Pill with an ST-Link gives the reference for
   peripheral behavior questions and cycle timing (DWT CYCCNT), recorded in
   experiment reports.
5. **Robustness:** decoding every 16-bit and every 32-bit pattern (sampled)
   never crashes the host; invalid ELF files are rejected with a message.
6. **Performance:** a benchmark firmware (integer loop mix, memory copy,
   interrupts) reports MIPS in the test log; regressions over 20 % fail.

## 10. Other chips

- STM32F103 variants (x6/x8/xB/xC/xE): memory sizes, packages and peripheral
  counts are data in the chip description.
- **STM32F407 (and F411 "Black Pill"):** a Cortex-M4F core variant adds the DSP
  extension (SIMD, saturating and packing instructions), the FPv4-SP floating
  point unit (VFP instructions, FPSCR, lazy stacking, CONTROL.FPCA) and
  different peripheral versions (GPIO with MODER/AFR, RCC with the F4 PLL,
  ADC of the F4, DMA streams). USART and TIM are close to the F1 versions and
  are shared with variant flags.
- Boards are profiles that map header pins to chip pins and declare on-board
  parts (LED, crystal frequencies, reset button, regulator). Separating the
  chip component from the board needs subcircuits (PRJ-06, SN-161); until then
  a board profile references its chip by name.

## 11. Open questions

| ID | Question | Resolved by |
|---|---|---|
| EQ-1 | Interpreter speed on the owner's laptop and on lab PCs (Q-103) | E-07 benchmark; lab specs |
| EQ-2 | Cost of each edge technique (breakpoints, rise times, logic-only nets); interaction with segment restarts | Spike E-07b in N0 |
| EQ-3 | Smallest useful lead `L` without slowing the circuit | **Resolved 2026-10-15 (SN-173):** at maximum speed with an EXTI-driven firmware, windows of 1 ms, 100 µs and 20 µs ran at 2.48x, 2.40x and 2.32x real time; the own emulator uses 100 µs (inputs late by less than 0.1 ms, button-to-LED response 100 µs instead of 1.1 ms with Renode) |
| EQ-4 | Can ST's SVD files (register descriptions) be used to generate register tables? License to be checked; otherwise tables are written from RM0008 | before N3 |
| EQ-5 | Which clock-gating behaviors to model, checked on the real board | N1 |
| EQ-6 | Keep Renode as an optional engine after the switch, or remove it | **Resolved 2026-10-18:** removed (decision D4, [ADR 0094](../decisions/0094-remove-renode.md)) |
