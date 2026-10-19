# Current state

Updated: 2026-10-18. Keep this file short: present state, last results, next
steps. Replace it, do not append. History of the first development line
(2026-08-31 → 2026-10-06) is in
[archive/2026-10-legacy/CURRENT.md](../archive/2026-10-legacy/CURRENT.md);
details of each change are in the [CHANGELOG](../../CHANGELOG.md) and the ADRs.

## Where we are

- **Release 0.3.0 published on 2026-10-18** (due 2026-11-27, so 40 days
  early): tag `v0.3.0`, GitHub release with `SimNodus-0.3.0-windows-x64.zip`
  (27 MB). The owner tried the preview package on 2026-10-18: all examples
  run, and a USART capture exported as VCD decodes in PulseView. Releases
  0.1.0 and 0.2.0 were published on 2026-10-14 and 2026-10-18.
- Everything is merged in `main`; no open branches or PRs.
- **New in 0.3:** logic gates (SN-191) and buses with splitters (SN-196,
  ADR 0096), examples 17 and 18; text notes (SN-193); model variants with
  the user's models folder (SN-194, ADR 0095); potentiometer position
  (SN-195); USART RX decoded from the pin (SN-192); Export VCD (SN-190);
  Renode removed (SN-183, ADR 0094); fixes from the owner's preview test.
- **Carried over from 0.2:**
  - Analog interactive editor (phase 1): component sets A and B (B: PNP, PMOS,
    Zener, op-amp, lamp, relay, DC motor, transformer), undo/redo, `.snx` files, examples 01–18, continuous interactive
    simulation with ngspice (ADR 0080), transient mode.
  - Signal Analyzer: scope with probes, roll/trigger, cursors, measurements,
    per-channel vertical scale, 0 V markers and offsets (SN-189), CSV/PNG
    export; Logic tab (pin and probe lanes); UART terminal.
  - Blue Pill (STM32F103) running ELF firmware in **SimNodus's own emulator**
    (ADR 0089/0090; 0.2 still had Renode 1.16.1 as a legacy option): GPIO, EXTI,
    USART with frames on the TX pin and board-to-board links, TIM1–TIM4 with
    PWM and input capture, ADC1/2, DMA1, RCC at 72 MHz; STM32CubeIDE HAL
    projects run unchanged. Firmware edges reach the circuit as step averages
    (ADR 0091) or, with *Exact microcontroller edges*, exactly (ADR 0093).
  - Debugging from STM32CubeIDE or arm-none-eabi-gdb (Simulate > Debug
    Firmware with GDB): breakpoints, stepping, watchpoints, `load` like an
    ST-LINK; the circuit pauses with the firmware. Gate G4 met by the owner.
  - Lessons 1–3 with firmware and notes in `examples/lessons/`.
- **Owner's tests passed:** CubeMX/CubeIDE TIM1 PWM project, 10 kHz PWM with
  a 10 µs step and exact edges (0.2x real time, acceptable), CubeIDE
  debugging, the 0.2 package on a clean VM. Not yet tested by the owner: a
  CubeMX ADC+DMA project.

## Next steps

1. Classroom feedback (postponed: academic calendar); fix what the owner
   reports when it arrives.
2. Backlog candidates, none urgent; ask the owner which block comes next:
   installer (SN-152), pt-BR translation (SN-153), dark theme (SN-162), user
   guide (SN-154), subcircuits (PRJ-06, also for `.subckt` part models),
   engine in a child process (SN-151).
3. Release 0.4 is due 2026-12-11.

## Known constraints

- Release zips are built locally with Qt 6.11.1 (`tools/package_windows.py`);
  CI artifacts use Qt 6.10.2.
- Semiconductor junction charge is not carried across interactive segments.
- Windows sleep granularity gives up to ~25 ms pacing lag.
- Exact microcontroller edges cost speed (0.8x–2.9x real time at 100 µs;
  0.2x at 10 µs with 10 kHz PWM).
- USART RX from the pin needs exact edges or bits of four circuit steps;
  bytes reach DR about 0.4 ms after their stop bit.

## Resuming in a new session

- **Owner:** writes in Portuguese; repository content stays in English. Asks
  for token economy: no unnecessary tests or packages. Ask before cutting a
  release; PRs may be squash-merged once `Build (Linux)` and `Build (Windows)`
  pass.
- **Local folder:** `D:\03_Projects\01_Actives\SimNodus_project`, a Git clone
  on `main`, in sync with GitHub. `handoff/` and `START_HERE.md` are local
  only.
- **Toolchain (owner's laptop):** VS 18 Build Tools (MSVC 14.51), Qt 6.11.1
  in `C:\Qt\6.11.1\msvc2022_64`, CMake/Ninja from the Build Tools. Run
  commands in a developer shell (`vcvars64.bat`; it needs
  `C:\Program Files (x86)\Microsoft Visual Studio\Installer` on `PATH`).
  Firmware is built with the GNU Tools for STM32 14.3.rel1 inside the owner's
  STM32CubeIDE 2.1.1 (`python tools/build_firmware.py` finds it; `hal_clock`
  also needs STM32CubeF1 1.8.7 in `~/STM32Cube/Repository`).
- **Build and test:** `python tools/fetch_backends.py` (once), then
  `cmake -S . -B build/dev -G Ninja -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/msvc2022_64`, `cmake --build build/dev`,
  `ctest --test-dir build/dev -j 8` (14 suites, about 3 min),
  `python tools/check_repository.py`. GUI test executables print nothing to
  the console; use `-o file.txt,txt` and `QT_QPA_PLATFORM=offscreen`, with
  `build/deps/ngspice-runtime` on `PATH`.
- **Test package for the owner:** `python tools/package_windows.py
  --build-dir build/dev` writes `dist/SimNodus-0.2.0-dev-windows-x64.zip`
  and runs the packaged `--check-engine` and `--smoke-test`.
- **Release:** `build/release` is configured with `-DSIMNODUS_VERSION_SUFFIX=`
  (empty; `main` stays `dev`); bump the version in CMake first, build,
  `package_windows.py --build-dir build/release`, date the CHANGELOG section,
  tag `vX.Y.Z`, `gh release create` with the zip.
- **Pitfalls met so far:** Qt 6.4's moc (Linux CI) cannot parse C++20
  standard headers, so Q_OBJECT headers include project headers only under
  `#ifndef Q_MOC_RUN`; GCC treats missing aggregate initializers and implicit
  `[=]` captures as errors (check firmware-side code with
  `arm-none-eabi-g++ -fsyntax-only`); never delete a widget inside its own
  signal (defer with `QTimer::singleShot(0, ...)`); offscreen GUI tests must
  send mouse-move events explicitly; connect to a child widget only after it
  is created (a `connect` with a null sender fails silently: this was the
  0 V marker bug); compare the event stream with what the circuit reads
  before blaming a backend.
