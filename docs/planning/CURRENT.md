# Current state

Updated: 2026-10-16. Keep this file short: present state, last results, next
steps. History of the first development line (2026-08-31 → 2026-10-06) is in
[archive/2026-10-legacy/CURRENT.md](../archive/2026-10-legacy/CURRENT.md).

## Where we are

- **Release 0.1.0 published on 2026-10-14** (due 2026-10-30): tag `v0.1.0`,
  GitHub release with `SimNodus-0.1.0-windows-x64.zip`. Phase 1 (SN-110..SN-123)
  is done; the owner's retest of the build passed.
- Everything is merged in `main`: phase 0 (PR #97), phase 1 (#98–#100),
  fixes from the owner's first test (#101: open-switch leakage, crash when
  leaving a value field) and 0.1 polish (#102: crash recovery, net names on
  hover, differential probes, approximate-mode notes in exports), then
  Help > Keyboard Shortcuts (F1, read from the menus).
- What works: place/wire/edit circuits with component set A (20 types),
  undo/redo, `.snx` files with backup, examples; continuous interactive
  simulation (segments per ADR 0080) with switches, buttons,
  potentiometers, glowing LEDs, meter readouts; transient mode; Signal
  Analyzer with probes, roll/trigger, cursors, measurements, CSV/PNG export.
  See the [quick start](../user/QUICK_START.md).
- **Phase 2 (release 0.2, due 2026-11-13): SN-130..SN-137 done.** The
  Blue Pill component runs real firmware (ELF) in Renode 1.16.1 during an
  interactive run: Renode starts first, the firmware leads the circuit by at
  most two coupling quanta (default 1 ms, ADR 0083/0085), pins show their
  state, the PC13 LED lights, RESET
  restarts the firmware, Browse/Reload choose the ELF. GPIO outputs drive the
  circuit through the pin model (ADR 0084); example 07 blinks an external
  LED on PA0. Inputs read the circuit with Schmitt thresholds and fire EXTI
  interrupts (ADR 0085); example 08: a button toggles an LED from EXTI1.
  The analyzer's UART tab is a serial terminal (ADR 0086); example 09.
  The Logic tab shows pin and probe lanes on the scope's time base.
  Lessons 1 and 2 ship with firmware and notes (`examples/lessons/`);
  push buttons can bounce; interactive runs use Gear integration (ADR 0087).
  Probes can be dragged to another wire or pin.
  Renode ships in the Windows zip (ADR 0082).
  User page [MICROCONTROLLER.md](../user/MICROCONTROLLER.md) (MCU-09);
  timing fixes (#116); application icon (#117).
- **SN-137 clock model (ADR 0088):** `resources/renode/SimNodusRcc.cs`
  replaces the RCC store: ready flags, SWS following SW, SYSCLK/HCLK from
  the PLL settings; CPU MIPS and SysTick follow HCLK; bit-band aliases
  mapped; `FLASH_ACR`/`PWR` stores. STM32CubeIDE HAL projects run at 72 MHz.
  Example 12 (`hal_clock`) is built from the STM32CubeF1 1.8.7 HAL in
  `~/STM32Cube/Repository` (`cube.json`, `stm32f103c8_cube.ld`).
- USART TX pins rest at 3.3 V while the transmitter is on (PR #120, owner
  question); the bits themselves need the own emulator.
- **Emulator track:** SimNodus's own STM32 emulator (ADR 0089; design
  [MCU_EMULATOR.md](../architecture/MCU_EMULATOR.md), plan
  [MCU_EMULATOR_PLAN.md](MCU_EMULATOR_PLAN.md)). E-07 passed (D0), N1 done and
  gate G1 met ([E-07-results.md](../experiments/E-07-results.md)).
  **D1 and D2 taken 2026-10-16 (ADR 0090, PR #130): the emulator is the
  default engine; Renode is frozen** (legacy menu option, kept until D4).
  Deterministic 100 µs window coupling; examples 07–12 at 1.00x real time.
- **N3 on the emulator: SN-177 timers done (2026-10-16).** TIM1–TIM4 in
  `src/emu/stm32f1_timer.*`: time base, output compare, PWM 1/2, TIM1
  complementary outputs and MOE, input capture, interrupts, AFIO remaps,
  APB timer clocks; edges exact to the timer clock (the core stops at the
  next timer event). Example 13 (`timer_pwm`, 100 Hz PWM into RC: 0.823 V
  vs 0.825 V ideal, input capture, TIM3 interrupt). **Owner's test
  (2026-10-16, dist zip):** example 13 and the owner's own CubeMX/CubeIDE
  project (TIM1 PWM at 100 Hz, switching 25 %/75 % every 2 s) worked
  correctly.
- **SN-176 done (ADR 0091):** each circuit step gets the pin's drive averaged
  over the step, so a 1 kHz PWM averages exactly (0.8251 V vs 0.825 V at the
  default 100 µs step; it was 0.66–0.99 V) at unchanged speed (examples 10
  and 13 at about 2x real time). Breakpoints at every edge were measured
  (exact but lesson 1 at 0.85x real time) and not taken. Example 13 is back
  at 1 kHz.
- **SN-178 ADC done (ADR 0092):** ADC1/ADC2 in `src/emu/stm32f1_adc.*`; a
  sample reads the circuit 0.1–0.2 ms back (a time the circuit has always
  passed: deterministic, no deadlock). Example 14: potentiometer → ADC → PWM,
  codes exactly those of the voltages ngspice computed. **Bug found and fixed
  on the way:** interactive potentiometers/closed switches were offset by
  their initial value since 0.1.0 (ngspice adds DC to external sources).
- **SN-179 DMA done:** DMA1 for ADC1 and USART1–3; example 15 is a
  CubeMX-style HAL project (ADC scan + circular DMA, TIM PWM, UART DMA) and
  runs as on the chip (23 808 scans/s, VREFINT 1489). N3 is complete; gate G3
  needs the owner's CubeMX check and lesson 3.
- **Exact edges option (ADR 0093, owner's request 2026-10-16):** after the
  owner saw deformed 1 kHz waveforms at 100 µs (and 0.25x at 10 µs), the
  SN-176 breakpoint code returned as the setting "Exact microcontroller
  edges" (off by default): square waves at the 100 µs step, 0.8x–2.9x real
  time (lesson 1: 1.2x). Speeds with the CPU free, default settings:
  examples 07 2.8x, 10 2.9x, 12 1.2x, 13 2.7x, 14 1.6x, 15 1.0x.
- **Owner's test 2026-10-16 (dist zip):** everything worked; a 10 kHz PWM
  with a 10 µs step and exact edges showed proper waveforms at 0.2x real
  time (acceptable to the owner). A CubeMX ADC+DMA project is not tested yet.
- **Lesson 3 (SN-143):** example 16 and `examples/lessons/lesson3_adc_uart.md`
  (potentiometer → ADC → UART, PWM → RC → ADC).

## Verified on 2026-10-15 (SN-137, merged in PR #118)

- **Owner's test:** a project generated with STM32CubeMX and built in
  STM32CubeIDE (HAL, LED blinking on PC13) runs in SimNodus; the period
  seen in the analyzer matches the one programmed in CubeIDE.
- CI: `Build (Linux)` and `Build (Windows)` passed on PR #118.
- Renode: blink firmware after a register switch to PLL x9 toggles PC13
  every 55.55 ms, after SYSRESETREQ every 500.0 ms again; HAL example
  prints SYSCLK 72 MHz, HCLK 72, PCLK1 36, PCLK2 72 MHz, PC13 every
  500.0 ms, PA1 pulse of 72000 instructions = 1000 µs. Standalone 1.6 s in
  1.3 s. GUI: example 12 shows the report in the UART tab, running at
  0.84x real time (busy-wait at 72 MIPS). ctest 9/9 (216 s), build without
  warnings, package checks pass, repository checker passed.

## Verified on 2026-10-15 (timing fixes, PR #116)

- Lesson 1 test under load (4 parallel runs, 3 rounds): 12/12 pass, 20
  rising edges, ripple 0.79–0.80 V. Cause of the CI failures: after Stop,
  ngspice computed up to 2.5 ms with frozen pin levels; those points are now
  dropped. Diagnostics showed the runner's event stream complete in every
  failing run. ctest 9/9.

## Verified on 2026-10-15 (SN-136)

- Bounce: 9 edges in 3.21 ms with bounce, 1 without. Lesson 1: 20 rising
  edges in 20 ms, RC ripple 0.79–0.80 V (theory 0.805 V). Lesson 2: 9
  falling edges per press and release without the filter, 1 with it (input
  changes are queued and replayed with their spacing, so bounces shorter
  than the quantum reach the firmware); the node stays within 0..3.3 V
  (it rang to −2.4 V with trapezoidal integration). ctest 9/9.

## Verified on 2026-10-15 (SN-135)

- Runner pin log (PC13 low at about 500 ms, level at a later window start);
  GUI: example 07 shows PA0..PA3 and PC13 lanes by themselves in natural
  order, unused pins stay hidden, PC13 lists its transitions. Screenshot of
  the Logic tab with the binary counter.

## Verified on 2026-10-15 (SN-134)

- Renode UART case (greeting, "tick 1" at 999.875 ms, echo, LED commands);
  GUI: example 09 terminal shows the greeting and time-stamped ticks, `1`
  and `0` sent from the terminal switch the LED. Screenshot of the UART tab.

## Verified on 2026-10-15 (SN-133)

- ctest 9/9 (116 s): Renode EXTI case (falling edge runs the handler, pull-up
  readable in ODR, port B resets to 0x44444444); session: button on the
  schematic toggles the LED through EXTI1, input delay at most 1000 µs at
  Q = 1 ms, button-to-LED response 1.1 ms; GUI: example 08 press/release.
- Renode speed with `SetGlobalAdvanceImmediately` (blink firmware): 1.43x
  real time with 1 ms chunks, 4.21x with 10 ms (0.71x / 0.96x without).

## Verified on 2026-10-15 (SN-132)

- ctest 9/9 (91 s); new: blink firmware into 1 kΩ gives 3.1731 V with edges
  within 0.1 ms of 250/500/750/1000 ms; pin model unit cases (push-pull,
  open drain, pulls); GUI: the example's LED reaches > 2 mA and goes dark.
  Screenshot: V(PA0) 2 Hz, 50 %, 3.145 V with the LED, LED current 3.8 mA.

## Verified on 2026-10-15

- Windows 11, MSVC 19.51, Qt 6.11.1, ngspice-47, Renode 1.16.1: build
  without warnings, ctest 9/9 (about 90 s): model 39 cases, engine 12
  (including a Blue Pill run kept within the 20 ms window), Renode 19
  (blink firmware timing, runner limit and reset, failures), GUI 13 widget +
  12 editor cases (example 07: firmware field, pins, LED on/off, reset
  click). Repository checker passed.
- Package: 137 MB zip; `--check-engine`, `--check-mcu-engine` (Renode from
  the package, 7.7 s start) and `--smoke-test` pass; example 07 from the
  package runs at 0.95x real time with the expected counter and LED states.
  Linux: CI.

## Verified on 2026-10-14

- Windows 11, MSVC 19.51, Qt 6.11.1, ngspice-47: build without warnings,
  ctest 8/8 — unit (model, library, files, editing, netlist, 34 + 6 cases),
  engine (library components against the real engine, current probes,
  session: button charges a capacitor, continuity and pulse phase across
  segments, 1x pacing, pause, stop, transient accuracy), GUI (12 widget
  cases; 11 editor end-to-end cases including "hold the button → the LED
  lights" in a live run and a transient record in the analyzer), executable
  smoke test. Repository checker passed.
- Screenshots of examples 1, 2 and 4 running: symbols, junctions, probes,
  live scope (2 Hz pulse, RC charge/discharge, measured f = 2 Hz, duty 50 %).
- Portable zip (`dist/`, 26.5 MB) runs `--check-engine` and `--smoke-test`
  from the package. Linux: verified by CI for each PR (GCC 13, Qt 6.4,
  ngspice-42); the Windows CI job uses Qt 6.10.2.
- Renode engine (Windows, Renode 1.16.1): `engine-renode` 8/8 cases against
  the real Renode, ctest 9/9 in about 50 s. Renode starts in 3.5–8 s; a
  hand-assembled Thumb loop toggling PA0 gave 64 events in 20 µs (8 MIPS,
  5-instruction loop). Linux: `engine-renode` passed in CI (ubuntu-24.04,
  GCC 13, portable .NET Renode package, 26 s) on PR #107.
- Earlier evidence: [S1](../experiments/S1-continuous-run.md),
  [S2](../experiments/S2-ngspice-platforms.md), ADR 0080.

- **N4 debugging (SN-180), part 1:** GDB server in the emulator and
  `simnodus-emu --gdb PORT`; arm-none-eabi-gdb from CubeIDE works with it
  (breakpoints, step, registers, variables, reset, detach). **Part 2 done:**
  Simulate > Debug Firmware with GDB; the circuit stands still while the
  firmware is halted; status bar; guide in MICROCONTROLLER.md. **Next: the
  owner's test from STM32CubeIDE (gate G4)**, then watchpoints.
  **Owner's test 2026-10-18:** debugging from STM32CubeIDE worked; setup was
  hard because the firmware had to be set in SimNodus (the IDE did not load
  it). Fixed: the debugger's `load` now writes the firmware and a board may
  start without one. Owner asked for scope vertical scale, 0 V marker and
  offset (SN-189, later). Watchpoints added (SN-180 done; gate G4 met by the
  owner's test).

- **Component set B (SN-142) done 2026-10-18:** PNP, PMOS, Zener, op-amp,
  lamp, relay, DC motor, transformer (logic gates when a lesson needs them).

- **SN-175 serial frames done 2026-10-18:** TX bit by bit at the baud rate,
  board-to-board links (byte level).
- **SN-189 done 2026-10-18:** scope vertical scale, 0 V markers and offset
  by dragging (owner's request).

## Next steps

1. **Release 0.2 (due 2026-11-13):** version 0.2.0 and the package README are
   ready; the owner tests the package (ideally on a clean classroom PC) and
   gives the go-ahead, then: configure with `-DSIMNODUS_VERSION_SUFFIX=`,
   package, tag `v0.2.0`, attach the zip to a GitHub release, date the
   CHANGELOG summary.
2. Then: classroom feedback; logic gates when a lesson needs them; RX sampled
   from the circuit for slow baud rates if wanted.

Firmware is built with the GNU Tools for STM32 14.3.rel1 inside the owner's
STM32CubeIDE 2.1.1 (`python tools/build_firmware.py` finds it, and the
STM32CubeF1 1.8.7 package in `~/STM32Cube/Repository` for `hal_clock`).

## Known constraints

- Release zips should be built locally with Qt 6.11.1
  (`tools/package_windows.py`); CI artifacts use Qt 6.10.2.
- Semiconductor junction charge is not carried across interactive segments.
- Windows sleep granularity gives up to ~25 ms pacing lag (invisible at
  20 Hz display).
- QTest mouse-move events are not delivered offscreen; GUI tests send them
  explicitly.
- `gui-editor` failed intermittently on the Windows CI runner: the CI log
  showed the logic lanes case waiting 15 s for PA3, which first changes at
  4 s of simulated time and took 16.7 s there. The waits are now 60 s.
- Renode reports GPIO events only during `run_for`, with quantum-granular
  timestamps; on Linux the child dies with the thread that started it.
- Renode model deviations: SysTick 125 ppm fast at 8 MHz, 14 ppm at
  72 MHz (ADR 0083); one instruction per cycle, no clock gating (ADR 0088); EXTI line n
  follows pin n of every port, AFIO not modeled (ADR 0085). The GPIO reset
  and ODR deviations are fixed by `SimNodusGpio.cs`.
- ARM compiler on the owner's laptop: `arm-none-eabi-gcc.exe` in
  `C:\ST\STM32CubeIDE_2.1.1\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_*\tools\bin`;
  CI uses Arm GNU Toolchain 14.3.rel1 (`tests/experiments/renode-stm32/ci-toolchain.json`).

## Resuming in a new session

- **Waiting for:** the owner's test of the phase 2 features, then continue with "Next steps". Release builds:
  configure a separate tree with `-DSIMNODUS_VERSION_SUFFIX=` (empty; `main`
  stays `dev`), package it, tag `vX.Y.Z`, attach the zip to a GitHub release.
- **Local folder:** `D:\03_Projects\01_Actives\SimNodus_project` is a Git
  clone on `main`, in sync with GitHub. `handoff/` and `START_HERE.md` are
  local only.
- **Toolchain (owner's laptop):** VS 18 Build Tools (MSVC 14.51), Qt 6.11.1
  `C:\Qt\6.11.1\msvc2022_64`, CMake/Ninja from the Build Tools. Run
  commands in a developer shell (`vcvars64.bat`; it needs
  `C:\Program Files (x86)\Microsoft Visual Studio\Installer` on `PATH`).
- **Build and test:** `python tools/fetch_backends.py` (once), then
  `cmake -S . -B build/dev -G Ninja -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/msvc2022_64`, `cmake --build build/dev`,
  `ctest --test-dir build/dev` (9 suites, about 4 min), `python tools/check_repository.py`.
- **Package:** `python tools/package_windows.py --build-dir build/dev` writes
  `dist/SimNodus-0.1.0-dev-windows-x64.zip` and runs the packaged
  `--check-engine` and `--smoke-test`. Screenshots for checks:
  `simnodus.exe examples/01_LED_and_push_button.snx --run --screenshot out.png`.
- **Publishing:** `main` requires a pull request with the checks `Build
  (Linux)` and `Build (Windows)`; only squash merges are allowed. The owner
  authorized direct merges once the checks pass.
- **Pitfalls met so far:** Qt 6.4's moc (Linux CI) cannot parse C++20
  standard headers, so Q_OBJECT headers include project headers only under
  `#ifndef Q_MOC_RUN`; GCC treats missing aggregate initializers and implicit
  `[=]` captures as errors; never delete a widget inside its own signal
  (defer with `QTimer::singleShot(0, ...)`); offscreen GUI tests must send
  mouse-move events explicitly; an intermittent MCU test failure was a
  session artifact (points computed after Stop), not Renode: compare the
  runner's event stream with what the circuit reads before blaming the
  backend.
