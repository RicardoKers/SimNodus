# Current state

Updated: 2026-10-26. Keep this file short: present state, last results, next
steps. Replace it, do not append. History of the first development line
(2026-08-31 → 2026-10-06) is in
[archive/2026-10-legacy/CURRENT.md](../archive/2026-10-legacy/CURRENT.md);
details of each change are in the [CHANGELOG](../../CHANGELOG.md) and the ADRs.

## Where we are

- **On `main` since 0.10:** File > Save with Firmware (SN-219) and the
  placement shortcuts V, I, Shift+V, Shift+I (SN-220); sensors and
  transducers (SN-221, example 36); logic chips in DIP packages (SN-222,
  example 37); I2C chips with an OLED (SN-223, example 38); SPI chips and the
  HX711 (SN-224, example 39); timed one-wire chips (SN-225, example 40). The
  owner asked on
  2026-10-26 for a much larger part library; the plan is SN-221..SN-229 in
  the backlog (sensors, 74HC chips in DIP packages, I2C/SPI/1-Wire chips,
  motion, power, sound).
- **Release 0.10.0 published on 2026-10-25** (the owner asked for it after
  the robustness pass): tag `v0.10.0`, GitHub release with
  `SimNodus-0.10.0-windows-x64.zip` and `SimNodus-0.10.0-windows-x64-setup.exe`.
  Releases 0.1.0-0.9.0 were published between 2026-10-14 and 10-25.
- **New in 0.10:** user guides in Brazilian Portuguese (`docs/user/pt-BR`,
  Help > User Guide by interface language; `check_repository.py` keeps them
  in step with the English ones); the classroom robustness pass
  (`tools/examples_pass.py`, `tools/examples_gui_pass.py`; shared thresholds
  in the logic parts, `tools/spice_levels.py`; one floating-net message per
  group).
- **New in 0.9:** Cortex-M3 instruction timing with flash wait states,
  calibrated on the owner's Blue Pill (SN-181, `cycle_loops` firmware,
  Simulate > Settings, engine protocol 9); the owner's CubeMX ADC+DMA project
  matched the board; polish from the owner's test (adjustable sources drag
  up to raise, simple transistors converge from 0 V, copying messages,
  meter polarity marks, live values of selected wires and probes).
- **New in 0.8 — connectivity (ADR 0104):** `simnodus-cli` with the JSON
  command vocabulary (`src/automation`; build, check, inspect, simulate,
  measure, save; `Session::pause_at`, engine protocol 8; SN-215); the same
  commands controlling the window (Tools > Allow External Control or
  `--control`, `simnodus-cli connect`; edits in the undo history, runs shown
  live, `view.pace`/`view.caption` for recorded tutorials; SN-216);
  `simnodus-cli mcp [--window]`, the commands as MCP tools (SN-217); external
  I/O parts and the TCP link for virtual plants (Simulate > External I/O,
  shown only with such parts; SN-218). Owner's tests on 2026-10-25: Claude
  Code built and simulated an LED circuit with captions in the window through
  MCP (registered with `claude mcp add --scope user`), and the link drove the
  owner's Godot plant correctly.
- **Also new in 0.8:** a behavioral 555 timer (SN-213, example 34;
  interactive segments carry capacitor node voltages with `.ic`) and any
  vendor `.SUBCKT` as a part of its own via Place > Subcircuit (SN-214, ADR
  0097 update, example 35).
- **New in 0.7:** adjustable and controlled sources (SN-209, example 29), a
  4x4 matrix keypad (SN-210, example 30), ideal A/D and D/A converters
  (SN-211, example 31), parallel ROM and RAM and chips without a board
  (SN-212, ADR 0103, examples 32 and 33).
- **New in 0.6:** CRC, IWDG, WWDG, RTC/BKP (SN-187, example 24); chips for
  microcontrollers with hybrid coupling (SN-203, ADR 0102: 74HC595, example
  25); SPI1/2 master, MAX7219, 8x8 matrix and 4-digit display, glow averaged
  over 20 ms (SN-204, example 26); I2C1/2 master with 24C02, LM75 and PCF8574
  (SN-205, example 27); HD44780 character LCD with its text on the schematic
  (SN-206, example 28); fixes from the owner's test and live chip parameters
  (SN-207); Memory Viewer for the 24C02 (SN-208).
- **New in 0.5:** simulations in a separate engine process (#182, ADR 0101:
  `simnodus-engine`; an engine crash ends only the run), Windows installer
  (#177, ADR 0100), Brazilian Portuguese interface (#176), dark theme (#178),
  user guide with known limitations (#181), polish from the owner's test
  (#179).
- **New in 0.4:** blocks drawn by the user (#163, ADR 0098, PRJ-06 complete
  with vendor subcircuits #161, ADR 0097); latches, flip-flops, counters and
  a shift register (#164, ADR 0099); regulators and references (#165);
  7-segment displays and decoders with real pinouts (#169, #171); teaching
  models and diode kinds (#162); Digital-style splitters (#168); model folders
  (#167); symbol ergonomics (#166); Signal Analyzer speed, stable scrolling
  and trigger marker (#170, #172-#174); examples 19-23.
- **New in 0.3:** logic gates and buses with splitters (ADR 0096), text
  notes, model variants with the user's model folders (ADR 0095),
  potentiometer position, USART RX from the pin, Export VCD, Renode removed
  (ADR 0094).
- **Carried over from 0.2:**
  - Analog interactive editor (phase 1): component sets A and B (B: PNP, PMOS,
    Zener, op-amp, lamp, relay, DC motor, transformer), undo/redo, `.snx` files, examples 01–23, continuous interactive
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
  debugging, the 0.2 package on a clean VM, and (2026-10-25) the owner's own
  CubeMX ADC+DMA project: the same circuit on a Blue Pill and in SimNodus
  printed coherent voltages on the serial port, matching the voltage at the
  analog input (differences only from real resistor tolerance).

## Next steps

1. Component library expansion, in backlog order: SN-226 (motion: servo,
   stepper, encoder, L298), then SN-227 and SN-228. Still slow on a 2026 laptop: example 26 (LED matrix, 0.4x), 24
   and 15 (about 1x). SN-186 postponed. Run `tools/examples_pass.py` and
   `tools/examples_gui_pass.py` before each release.
2. Classroom feedback (postponed: academic calendar).

## Known constraints

- Release zips are built locally with Qt 6.11.1 (`tools/package_windows.py`);
  CI artifacts use Qt 6.10.2.
- Semiconductor junction charge is not carried across interactive segments.
- Windows sleep granularity gives up to ~25 ms pacing lag.
- Exact microcontroller edges cost speed (0.8x–2.9x real time at 100 µs;
  0.2x at 10 µs with 10 kHz PWM).
- USART RX from the pin needs exact edges or bits of four circuit steps;
  bytes reach DR about 0.4 ms after their stop bit.
- Examples close to or below real time on a 2026 laptop: 26 (LED matrix,
  0.4x), 24 and 15 (about 1x), 23 (1.2x); listed in the user guide.

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
  `ctest --test-dir build/dev -j 8` (about 3 min),
  `python tools/check_repository.py`. GUI test executables print nothing to
  the console; use `-o file.txt,txt` and `QT_QPA_PLATFORM=offscreen`, with
  `build/deps/ngspice-runtime` on `PATH`.
- **Test package for the owner:** `python tools/package_windows.py
  --build-dir build/dev` writes `dist/SimNodus-<version>-windows-x64.zip`
  and runs the packaged `--check-engine` and `--smoke-test`.
- **Release:** `build/release` is configured with `-DSIMNODUS_VERSION_SUFFIX=`
  (empty; `main` stays `dev`); bump the version in CMake first, build,
  `package_windows.py --build-dir build/release --installer` (Inno Setup 6,
  `winget install JRSoftware.InnoSetup`), run `tools/examples_pass.py` and
  `tools/examples_gui_pass.py --qt-bin C:/Qt/6.11.1/msvc2022_64/bin` on
  `build/release`, date the CHANGELOG section, tag `vX.Y.Z`,
  `gh release create` with the zip and the setup.exe.
- **User guides:** `docs/user/pt-BR` translates QUICK_START, USER_GUIDE and
  MICROCONTROLLER; a change to one of them includes the same change in the
  translation and its new source hash (`check_repository.py` prints it).
- **Generated parts:** the library JSON of logic, storage, display, timer,
  converter, regulator, sensor, logic chip and I/O parts comes from `tools/generate_*_parts.py`;
  edit the generator and rerun it, not the JSON.
- **Pitfalls met so far:** ninja kept stale objects in `build/dev`
  (header changes not seen: 0 recorded dependencies, then heap corruption in
  the tests) because some objects had been compiled where cl's Portuguese
  "Observação: incluindo arquivo:" did not match the prefix in
  `rules.ninja`; build from a cmd shell with vcvars (code page 850), and if
  `ninja -t deps <obj>` shows `#deps 0`, rebuild with `--clean-first`. Qt 6.4's moc (Linux CI) cannot parse C++20
  standard headers, so Q_OBJECT headers include project headers only under
  `#ifndef Q_MOC_RUN`; GCC treats missing aggregate initializers and implicit
  `[=]` captures as errors (check firmware-side code with
  `arm-none-eabi-g++ -fsyntax-only`); never delete a widget inside its own
  signal (defer with `QTimer::singleShot(0, ...)`); offscreen GUI tests must
  send mouse-move events explicitly; connect to a child widget only after it
  is created (a `connect` with a null sender fails silently: this was the
  0 V marker bug); compare the event stream with what the circuit reads
  before blaming a backend.
