# Plan: SimNodus STM32 emulator

Revision 2 — 2026-10-15 (revision 1 reviewed the same day; the review's
findings are folded in, see "Review" at the end). Design:
[MCU_EMULATOR.md](../architecture/MCU_EMULATOR.md). Decision record:
[ADR 0089](../decisions/0089-own-stm32-emulator.md) (proposed until gate G0b).
Tasks: SN-170..SN-186 in the [backlog](BACKLOG.md).

## Principles

1. **Releases keep their dates.** 0.2 (11-13), 0.3 (11-27), 0.4 (12-11) and the
   freeze (12-15) ship on whichever engine has passed its gates. Renode stays
   the shipped engine until the emulator passes G1, and stays in the package
   as a fallback at least until decision D4.
2. **Gates with predeclared, measured criteria.** Each gate is written before
   its work starts and measured on something the model under test does not
   produce itself (an independent reference, the analog node, the real
   board). A failed gate leads to a recorded decision, never to quietly
   lowering the criteria.
3. **New MCU features are built once.** After decision D2 they are built only
   on the emulator; before it, the Renode path only gets fixes. D2 is a
   separate decision because it removes those features from the Renode
   fallback (see D3).
4. **The owner tests every milestone hands-on**, as with the releases.
5. **Every milestone updates** MICROCONTROLLER.md, the CHANGELOG, CURRENT.md and
   the backlog, and adds an experiment report or ADR when it decides something.
6. **Independent references.** The core is written and reviewed by agents; its
   correctness rests on references we did not write: the ARMv7-M pseudocode,
   Renode, QEMU as a test-only process, and the owner's real Blue Pill.

## Milestones

### N0 — Prototype, experiment E-07 (SN-170, SN-186)

Dates: 2026-10-16 → 10-30, with two gate reviews: **G0a on 10-24** and
**G0b on 10-30**. Not integrated in the GUI.

Work packages, in order:

| # | Package | Done when |
|---|---|---|
| 0.1 | `src/emu` library and `apps/simnodus-emu` CLI in CMake (Windows and Linux CI); ELF loader; `McuBackend`-style time in nanoseconds from the start | Loads every example ELF; CLI prints entry point and segments |
| 0.2 | Core state, 16-bit Thumb instructions, flags | Instruction tests pass |
| 0.3 | 32-bit Thumb-2 instructions, IT blocks, exclusive monitor; decoder hit counters (per decoder entry) | Instruction tests for every encoding group pass |
| 0.4 | Differential harness for random SRAM blocks against Renode (batched dumps: registers, flags, memory window), QEMU `mps2-an385` as a second test-only oracle | Runs in ctest as `emu-differential` when the oracles are installed |
| 0.5 | Predecoded instruction cache, portable dispatch (handler pointers in the decoded records; no computed goto, so MSVC is covered), benchmark firmware | Numbers recorded → **G0a** |
| 0.6 | Exceptions, NVIC, SysTick, SCB, WFI/WFE, bit-band, faults, lockup, resets | A test firmware takes SysTick, SVC, PendSV, a forced fault and each reset kind |
| 0.7 | Tier-1 devices: RCC (behavior of SimNodusRcc.cs), FLASH, PWR, GPIOA–D, AFIO, EXTI, USART1–3 at byte level | Every example ELF runs in the CLI |
| 0.8 | Whole-firmware trace comparison against Renode; FreeRTOS demo firmware | Results recorded → **G0b** |
| 0.9 | **Analog edge spike E-07b (SN-186), independent of the emulator:** a synthetic edge stream (115200-baud frames, 20 kHz PWM) into today's interactive session; compare ngspice breakpoints (`ngSpice_SetBkpt`, which adds a timestep breakpoint through `CKTsetBreak`), edges with a finite rise time by GPIO speed, and injecting edges only on nets with analog loads (logic-only nets go to the logic lanes) | Speed and edge-time error per option recorded; the G2 speed criterion is fixed from this data at G0b, before N2 starts |
| 0.10 | Report `docs/experiments/E-07-results.md` | Gates reviewed with the owner |

**Gate G0a — core (10-24):**

| ID | Criterion |
|---|---|
| G0a.1 | Instruction tests written from the ARMv7-M pseudocode pass for every encoding group (list in the report) |
| G0a.2 | Random differential: every decoder entry hit at least 1 000 times (from the hit counters), with operands that include PC and SP where allowed, shifts by 0, 32 and over 32, and branches in the last IT slot; zero unexplained differences against Renode. A difference counts as a Renode deviation only when a third reference (pseudocode vector, QEMU or the real board) agrees with our result; deviations are listed per encoding, at most 3 encodings may rest on QEMU alone |
| G0a.3 | At least 150 MIPS on a busy loop and 100 MIPS on the benchmark mix, Release build, owner's laptop, on Windows (MSVC) and Linux (GCC); target 2x real time at 72 MHz because lab PCs are unknown (Q-103) |
| G0a.4 | Decoding sampled 32-bit patterns and every 16-bit pattern never crashes the host |

**Gate G0b — firmware (10-30):**

| ID | Criterion |
|---|---|
| G0b.1 | Every example and Renode test firmware (blink, button_irq with the same injected inputs, uart_echo, lesson1_gpio, lesson2_button, hal_clock) gives the same sequence of GPIO and UART events as Renode; timestamps agree within Renode's 100 µs reporting step, and with the instruction count expected at one instruction per cycle within 1 µs |
| G0b.2 | The owner's STM32CubeMX/CubeIDE project: PC13 toggle times in the CLI trace match the period programmed in CubeIDE within 0.1 % |
| G0b.3 | FreeRTOS demo: the sequence of task switches and GPIO events matches Renode |
| G0b.4 | Faults: a firmware that reads an unmapped address and one that ends in lockup produce a diagnostic with the PC and function name; the host never hangs |
| G0b.5 | Resets: NRST, SYSRESETREQ (and later IWDG) keep SRAM, set the RCC_CSR reset flags as on the chip, and restart at the reset vector |
| G0b.6 | Loading and resetting an ELF takes under 50 ms; builds without warnings on Windows and Linux CI, and `emu-differential` runs on both |
| G0b.7 | Two runs of the same ELF with the same scripted inputs give identical traces (hash) |

Outcomes (decision **D0**): G0a and G0b pass → ADR 0089 accepted, start N1.
Only the speed criterion fails → one more week for performance (block
chaining, a clang-cl build on Windows), then re-review. Correctness criteria
fail without a clear path → keep Renode, record the reasons, ADR 0089 rejected
or deferred.

### N1 — Integration and parity (SN-171..SN-174)

Dates: 11-02 → 11-13 (release 0.2 still ships on Renode on 11-13).

- SN-171: extract `McuBackend` from today's `McuRunner`, **in nanoseconds**,
  with Renode-neutral pin names ("PA9", not "gpioPortA"); the current class
  becomes `RenodeMcuRunner` and rounds to its microseconds. The interface
  includes a way to read circuit voltages for the ADC (`analog_at(pin, t_ns)`,
  answered from the circuit's history) so N3 does not change it again.
- SN-172: `NativeMcuRunner` on `src/emu`; engine selection per board profile
  (`"engine": "native" | "renode"`) with a Simulation Settings override for
  tests and for falling back; diagnostics in the Messages panel.
- SN-173: the MCU test cases of the session, GUI and engine suites run on both
  engines (parameterized), plus native-only assertions with tight timing
  (no 100 µs or two-quanta tolerances); the deterministic coupling rule
  (inputs apply at the next window boundary, design section 7) and a measured
  window `L`. CI time is watched: the Renode variants may move to a nightly job.
- SN-174: packaging, MICROCONTROLLER.md per engine (one instruction per cycle
  named as an approximation until N5), CHANGELOG; owner test build.

**Gate G1:** every MCU test passes on both engines and the native-only timing
assertions pass; every example runs at ≥ 1.0x real time with the native engine
on the owner's laptop; startup of a run with a board under 1 s; two coupled
runs of example 08 with scripted button presses give identical pin and UART
traces (hash); a one-hour interactive run of example 07 keeps memory flat
(growth under 10 MB); the GUI smoke test with the native engine passes on
Linux CI; the owner's hands-on test of lessons 1–2 and his CubeIDE project
passes. → decision **D1**: native becomes the default engine (test build
first, release 0.3 if the owner agrees).

**Rollback trigger D1b:** an MCU regression found by the owner in a release
with the native default that is not fixed within three working days makes
Renode the default again for the next release.

### N2 — Pin-level signals (SN-175, SN-176)

Dates: 11-16 → 11-20.

- SN-175: nanosecond times through the session (the session's MCU times move
  from µs to ns); USART frames on TX and sampling of RX at the configured baud
  rate; logic lanes show the bits.
- SN-176: the edge technique chosen in E-07b, with finite rise times.

**Gate G2** (measured on the analog node, not on the emulator's pin log):
at 115200 baud, the threshold crossings of V(PA9) in example 09 give bit
times within 1 % of 8.68 µs; interactive speed with continuous serial output
at least the value fixed at G0b from E-07b; earlier tests unchanged. If
SimNodus runs two boards in one circuit by then, two Blue Pills exchange text
through wires (otherwise this check moves to N6).

### N3 — Phase 3 content on the emulator (SN-177..SN-179)

Dates: 11-16 → 11-27 (release 0.3), only if decision **D2** puts it on the
emulator; it then replaces SN-140/SN-141.

- SN-177: TIM1–4: time base, PWM, output compare, input capture, one-pulse,
  interrupts and DMA requests; timer clocks ×2 when the APB prescaler is not 1;
  PWM on the pins.
- SN-178: ADC1–2: single, continuous and scan conversions, sample times;
  samples read from the circuit's history at their sample instants; one
  synchronization per coupling window instead of one per sample, and a
  labeled approximation (latest committed value) when the sample rate is too
  high for real time.
- SN-179: DMA1, only what HAL ADC+DMA and UART+DMA need (circular and normal
  mode, transfer-complete and half-transfer interrupts).

Deferred until after the freeze: IWDG, WWDG, CRC, RTC/BKP (SN-187).

**Gate G3** (measured against independent values): lesson 3 (potentiometer →
ADC → UART report, PWM → RC filter) passes: ADC codes within ±2 LSB of the
code computed from the node voltage that ngspice reports at the sample time;
PWM frequency and duty within 0.1 % from the analog node's threshold
crossings; a HAL project with continuous ADC and DMA at the maximum rate
runs at ≥ 0.9x real time or shows the labeled approximation.

If D2 keeps phase 3 on Renode, phase 3 follows the original plan (SN-140/141)
and N3 moves after the freeze.

### N4 — Debugging (SN-180)

Dates: 11-30 → 12-11 (phase 4), replacing the Renode plan of SN-150 if D2 put
features on the emulator. **If N3 slips, N4 moves after the freeze** and
release 0.4 ships without debugging (the roadmap already names debugging as
the first candidate to postpone).

GDB remote serial protocol on loopback: target description, registers,
memory, breakpoints (FPB and software), watchpoints (DWT), step, continue,
`monitor reset halt`. Flash download is a non-goal: the documented CubeIDE
launch configuration skips `load` (the ELF is already loaded by SimNodus) —
or, if CubeIDE cannot skip it, `load` writes the ELF image into the model.
Pause semantics: the firmware stops at time t_f; the circuit runs on to t_f;
then both are paused, and the GUI shows that time.

**Gate G4:** in STM32CubeIDE, a breakpoint in an EXTI handler stops circuit and
firmware at the same time; step, variable view, continue and reset work; ten
repeated sessions without a hang.

### Freeze (12-15) — decision D3

| Option | When |
|---|---|
| Native engine with its listed limits | G1 passed, D1 taken, no open D1b rollback, owner's tests clean |
| Renode **without** the phase 3/4 features built only on the emulator | otherwise |

Debugging ships only if G4 passed.

### N5 — Cycle model and board validation (SN-181), after the freeze

Cortex-M3 cycle table and flash wait states (design section 6), measured on
the owner's Blue Pill with DWT CYCCNT for a set of loops; a setting chooses
"one instruction per cycle" or "Cortex-M3 timing". Gate G5: the test loops
agree with the board within 5 %.

### N6 — Chip and board separation (SN-182)

A chip component (STM32F103C8, LQFP48) and board profiles that reference it;
several boards in one circuit; full subcircuit editing waits for PRJ-06 (SN-161).

### N7 — Renode decision (SN-183) — decision D4

After two releases on the native engine without MCU regressions, decide in an
ADR whether Renode leaves the package (about 100 MB less) or stays optional.

### N8 — Next chips (SN-184, SN-185), after the classroom release

- SN-184: Cortex-M4F core variant (DSP extension, FPv4-SP FPU, lazy stacking),
  validated with the same differential method.
- SN-185: STM32F407 peripherals (GPIO v2, RCC F4, ADC F4, DMA streams; USART
  and TIM shared) and a board (STM32F4-Discovery). The STM32F411 "Black Pill"
  shares most of this and could be the first M4 board — decision **D5**.

## Calendar

| Dates | Releases (unchanged) | Emulator track |
|---|---|---|
| 10-16 → 10-24 | 0.2 polishing, owner tests | N0 core, gate **G0a** (10-24) |
| 10-27 → 10-30 | — | N0 devices, firmware traces, E-07b; gate **G0b** (10-30) → D0 |
| 11-02 → 11-13 | **0.2** on Renode (11-13) | N1 integration; gate **G1** → D1, D2 |
| 11-16 → 11-27 | **0.3** (native if D1) | N2 serial frames and edges (G2); N3 TIM/ADC/DMA (G3) if D2 |
| 11-30 → 12-11 | **0.4** | N4 debugging (G4), unless N3 slipped |
| 12-15 | Freeze, 0.5 RC | **D3**: classroom engine |
| 12-15 → 01-31 | Stabilization, 1.0 | Fixes; N5 measurements if time allows |
| After classroom | — | SN-187 (watchdogs, RTC, CRC), N5, N6, N7 (D4), N8 (D5) |

Where it will most likely slip: N0 (a whole Thumb-2 core in two weeks) and
N2+N3 (five areas in two weeks). The scope protection rule of the roadmap
applies: a milestone more than three working days late has its gate reviewed,
and the release ships on Renode for that feature.

## Decision points

| ID | When | Question | Default if not decided |
|---|---|---|---|
| D0 | 10-30 (G0a, G0b) | Continue with the emulator? | Stop; keep Renode |
| D1 | 11-13 (G1) | Native as the default engine? | Renode stays default |
| D1b | any release with native default | Roll back to Renode after an unfixed regression (3 working days) | Roll back |
| D2 | 11-13 | Build phase 3/4 features only on the emulator? (removes them from the Renode fallback) | On Renode, as planned |
| D3 | 12-15 | Engine of the classroom release (options above) | Renode |
| D4 | after two native releases | Remove Renode from the package? | Keep it |
| D5 | after the classroom | First M4 chip: F407 or F411 | F407 |

## Risks

[RISKS.md](RISKS.md) R-110..R-117: core correctness, interpreter speed on lab
PCs, peripheral scope, analog cost of edges, the classroom release depending
on a young engine, agent-only review of agent-written code, CI time doubling
with two engines, and the Renode fallback losing features after D2.

## Reporting

- Each work package lands as a pull request with tests; its description states
  which gate criteria it advances.
- `CURRENT.md` carries an "Emulator track" line with the milestone and the
  last numbers (MIPS, decoder coverage, differential blocks, open deviations).
- E-07-results.md reports G0a, G0b and E-07b; later gates add a results
  section there or an ADR when a decision is taken.

## Review

Revision 1 was reviewed on 2026-10-15 by an independent agent review. Changes
in revision 2: N0 split into G0a/G0b with the analog edge spike E-07b moved
into N0; `ngSpice_SetBkpt` checked in the ngspice source (it adds a timestep
breakpoint through `CKTsetBreak`; it does not halt the run) with its speed
cost still to be measured; ADC synchronization per window with an
`analog_at` interface in SN-171; a deterministic coupling rule and trace-hash
criteria; D2 separated from D1, D1b rollback, D3 options stated honestly;
watchdogs/RTC/CRC deferred and DMA reduced; gate criteria measured against
independent references (pseudocode, QEMU, analog node, real board) with bounded
exclusions and decoder coverage; FreeRTOS, fault, reset, Linux, memory and
determinism criteria added; GDB download and pause semantics specified; risks
for agent-only review, CI time and lab PC speed added.
