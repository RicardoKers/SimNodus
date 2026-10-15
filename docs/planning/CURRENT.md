# Current state

Updated: 2026-10-15. Keep this file short: present state, last results, next
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
- **Phase 2 (release 0.2, due 2026-11-13): SN-130..SN-134 done.** The
  Blue Pill component runs real firmware (ELF) in Renode 1.16.1 during an
  interactive run: Renode starts first, the firmware leads the circuit by at
  most the coupling quantum (default 1 ms, ADR 0083/0085), pins show their
  state, the PC13 LED lights, RESET
  restarts the firmware, Browse/Reload choose the ELF. GPIO outputs drive the
  circuit through the pin model (ADR 0084); example 07 blinks an external
  LED on PA0. Inputs read the circuit with Schmitt thresholds and fire EXTI
  interrupts (ADR 0085); example 08: a button toggles an LED from EXTI1.
  The analyzer's UART tab is a serial terminal (ADR 0086); example 09.
  Renode ships in the Windows zip (ADR 0082).

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

## Next steps

1. Fix whatever classroom use of 0.1.0 brings up (patch releases 0.1.x).
2. SN-135 logic lanes in the analyzer (MCU pins over time, from the runner's
   pin history), then SN-136 lesson examples (the owner has none yet:
   create them), MCU-09 limitations page, and the 0.2 package by 2026-11-13.
4. Renode ships in the Windows zip (owner approval 2026-10-14, license review
   in ADR 0082); `--check-mcu-engine` checks it from the package. Firmware is
   built with the GNU Tools for STM32 14.3.rel1 inside the owner's
   STM32CubeIDE 2.1.1 (same compiler version as E-02).
5. Carry semiconductor junction charge across segments only if the owner
   sees steps (not seen in the retest).

## Known constraints

- Release zips should be built locally with Qt 6.11.1
  (`tools/package_windows.py`); CI artifacts use Qt 6.10.2.
- Semiconductor junction charge is not carried across interactive segments.
- Windows sleep granularity gives up to ~25 ms pacing lag (invisible at
  20 Hz display).
- QTest mouse-move events are not delivered offscreen; GUI tests send them
  explicitly.
- Renode reports GPIO events only during `run_for`, with quantum-granular
  timestamps; on Linux the child dies with the thread that started it.
- Renode model deviations: SysTick 125 ppm fast (ADR 0083); EXTI line n
  follows pin n of every port, AFIO not modeled (ADR 0085). The GPIO reset
  and ODR deviations are fixed by `SimNodusGpio.cs`.
- ARM compiler on the owner's laptop: `arm-none-eabi-gcc.exe` in
  `C:\ST\STM32CubeIDE_2.1.1\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_*\tools\bin`;
  CI uses Arm GNU Toolchain 14.3.rel1 (`tests/experiments/renode-stm32/ci-toolchain.json`).

## Resuming in a new session

- **Waiting for:** nothing; continue with "Next steps". Release builds:
  configure a separate tree with `-DSIMNODUS_VERSION_SUFFIX=` (empty; `main`
  stays `dev`), package it, tag `vX.Y.Z`, attach the zip to a GitHub release.
- **Local folder:** `D:_Projects_Actives\SimNodus_project` is a Git
  clone on `main`, in sync with GitHub. `handoff/` and `START_HERE.md` are
  local only.
- **Toolchain (owner's laptop):** VS 18 Build Tools (MSVC 14.51), Qt 6.11.1
  `C:\Qt.11.1\msvc2022_64`, CMake/Ninja from the Build Tools. Run
  commands in a developer shell (`vcvars64.bat`; it needs
  `C:\Program Files (x86)\Microsoft Visual Studio\Installer` on `PATH`).
- **Build and test:** `python tools/fetch_backends.py` (once), then
  `cmake -S . -B build/dev -G Ninja -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/msvc2022_64`, `cmake --build build/dev`,
  `ctest --test-dir build/dev` (8 suites), `python tools/check_repository.py`.
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
  mouse-move events explicitly.
