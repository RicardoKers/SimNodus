# Backlog

Revision 2 — 2026-10-14. Tasks for the rebuilt product. IDs from SN-100 upward;
the previous backlog (SN-001..SN-045) is archived in
[archive/2026-10-legacy/BACKLOG.md](../archive/2026-10-legacy/BACKLOG.md).
Status: `todo`, `doing`, `done`. A task is `done` only when its check passes.

## Phase 0 — Reset (until 2026-10-16)

| ID | Task | Check | Status |
|---|---|---|---|
| SN-100 | New requirements, UI spec, architecture, roadmap, ADRs 0079–0081, archive of legacy planning | Owner review; repository checker passes | done |
| SN-101 | Move previous product code (`src/`, `apps/`, related tests) to `legacy/`, keep backend experiments; simplified CMake and CI | Clean configure/build on Linux; CI green | done |
| SN-102 | Application skeleton: main window with three resizable/collapsible columns, analyzer window, settings persistence, empty canvas with grid/zoom/pan | Runs on Linux (offscreen smoke test) and Windows | done |
| SN-103 | Spike S1 — continuous ngspice run: pacing in the data callback, external sources for controls, memory growth and segmented restart with initial conditions | Report with measured memory/speed; recommendation recorded in ADR 0080 | done |
| SN-104 | Spike S2 — ngspice on Windows: runtime loading of the bundled DLL, version 47 vs Linux distribution version | Same netlist gives same result on both | done |

## Phase 1 — Analog interactive editor, release 0.1 (until 2026-10-30)

| ID | Task | Requirements | Status |
|---|---|---|---|
| SN-110 | Document model with stable IDs and edit commands; unit tests | UI-08, PRJ-01 | done |
| SN-111 | Library format, loader and component set A symbols/templates | PRJ-04, SIM-06, UI-04 | done |
| SN-112 | Canvas: place, select, move, rotate, mirror, delete, copy/paste, undo/redo | UI-05, UI-08, UI-09, UI-11 | done |
| SN-113 | Wiring with orthogonal routing, junctions, attachment on move, net labels, ground | UI-06, UI-07 | done |
| SN-114 | Properties panel with engineering notation and validation | UI-10 | done |
| SN-115 | `.snx` save/open, recent files, backup, examples menu | PRJ-01..03, UI-15 | done |
| SN-116 | Connectivity, pre-run checks and SPICE generation with golden tests | SIM-05 | done |
| SN-117 | ngspice engine wrapper (runtime load, callbacks, errors) | SIM-04 | done |
| SN-118 | Interactive run: pacing, speed factor, controls, bounded memory, status | SIM-01, SIM-02, INT-01, UI-13 | done |
| SN-119 | Interactive components and live rendering (switches, button, pot, LED, meters, tooltips, live values) | INT-02..05 | done |
| SN-120 | Signal Analyzer scope: channels, roll/trigger, zoom, cursors, measurements, CSV/PNG | AN-01..06, AN-09 | done |
| SN-121 | Transient mode and settings dialog | SIM-03 | done |
| SN-122 | Messages panel, diagnostics with locate | UI-12, SIM-05 | done |
| SN-123 | Windows package 0.1 (zip with Qt runtime and ngspice), quick-start guide, 4–6 analog examples | NF-02 | done |

## Phase 2 — MCU GPIO, release 0.2 (until 2026-11-13; released 2026-10-18)

| ID | Task | Requirements | Status |
|---|---|---|---|
| SN-130 | Renode engine wrapper (Linux and Windows, pinned 1.16.1), process lifecycle, time control | MCU-02 | done |
| SN-131 | Blue Pill component, board profile, ELF selection/reload, pin state display | MCU-01, INT-06 | done |
| SN-132 | Coupler: lockstep quantum, GPIO output electrical model into ngspice | MCU-02, MCU-03 | done |
| SN-133 | GPIO input with Schmitt thresholds and EXTI | MCU-04 | done |
| SN-134 | UART terminal | MCU-05, AN-08 | done |
| SN-135 | Logic lanes in the analyzer | AN-07 | done |
| SN-136 | Lesson 1 and 2 example projects with firmware source | — | done |
| SN-137 | Clock model: RCC ready flags (HSE, PLL, SWS) and SysTick/CPU speed following the configured clock, so STM32CubeIDE HAL projects run | MCU-01, MCU-08 | done |

## Emulator track — own STM32 emulator (parallel, from 2026-10-16)

Plan, gates and decisions: [MCU_EMULATOR_PLAN.md](MCU_EMULATOR_PLAN.md);
design: [MCU_EMULATOR.md](../architecture/MCU_EMULATOR.md); [ADR 0089](../decisions/0089-own-stm32-emulator.md) (proposed).

| ID | Task | Milestone / gate | Requirements | Status |
|---|---|---|---|---|
| SN-170 | Experiment E-07: prototype emulator (`src/emu`, CLI, Thumb-2 core, exceptions, tier-1 devices), differential tests against Renode and QEMU, benchmark | N0 / G0a, G0b (met 10-15, D0) | MCU-11 | done |
| SN-186 | Spike E-07b: fast edges into the analog engine (breakpoints, rise times, logic-only nets) with a synthetic edge stream | N2 start (was N0; not run before D0) | MCU-10 | postponed 2026-10-25 (PWM part answered by SN-176 / ADR 0091; serial frames remain; the owner has no lesson that needs them — revisit for a serial frame on the scope or a serial module on RX) |
| SN-171 | Extract the `McuBackend` interface in nanoseconds with `analog_at`; current runner becomes `RenodeMcuRunner` | N1 / G1 | MCU-11 | done (analog input as `set_analog`, ADR 0092) |
| SN-172 | `NativeMcuRunner` and engine selection per board profile, with a settings override | N1 / G1 | MCU-11 | done (runner and diagnostics in Messages; the engine selection went with Renode, SN-183) |
| SN-173 | MCU tests on both engines plus native-only timing assertions; deterministic window coupling; measured `L` | N1 / G1 | MCU-02 | done |
| SN-174 | Packaging, MICROCONTROLLER.md per engine, owner test build | N1 / G1 | MCU-09 | done (owner tried the build on 2026-10-15) |
| SN-175 | Nanosecond MCU times in the session; USART frames on TX and RX sampling | N2 / G2 | MCU-10 | done 2026-10-18 (TX frames, board-to-board links; RX from the circuit not sampled) |
| SN-176 | Firmware edges into the circuit (PWM averages) | N2 / G2 | MCU-10 | done: step-averaged drive (ADR 0091); breakpoints measured, not taken |
| SN-177 | TIM1–4 with PWM on the pins | N3 / G3 | MCU-07 | done (example 13) |
| SN-178 | ADC1–2 with conservative synchronization to the circuit | N3 / G3 | MCU-06 | done (ADR 0092, example 14) |
| SN-179 | DMA1 as needed by HAL ADC+DMA and UART+DMA | N3 / G3 | MCU-06 | done (example 15, HAL project; owner's own CubeMX ADC+DMA project matched the board on 2026-10-25) |
| SN-187 | IWDG, WWDG, CRC, RTC/BKP (`src/emu/stm32f1_system.*`, HAL example 24) | after the freeze | — | done 2026-10-19 |
| SN-203 | Digital devices on the board's nets, hybrid coupling (ADR 0102): `src/devices`, `DeviceBus`, device pins into the circuit and the logic lanes; first device 74HC595 (example 25, bit-banged) | after 0.5 | MCU-10 | done 2026-10-19 |
| SN-204 | SPI1–2 master (`src/emu/stm32f1_spi.*`), MAX7219, 8x8 LED matrix and 4-digit 7-segment display parts, LED glow averaged over 20 ms, device events inside the machine's run (example 26) | after 0.5 | — | done 2026-10-19 |
| SN-205 | I2C1–2 master (`src/emu/stm32f1_i2c.*`), 24C02, LM75 and PCF8574, chip pins read from the circuit on nets without a board pin (example 27) | after 0.5 | — | done 2026-10-19 |
| SN-207 | Owner's test of the 0.6 preview: halo repaint, MAX7219 rows at low intensity, live LM75 temperature | after 0.5 | — | done 2026-10-20 |
| SN-208 | Memory viewer: see, save and load the contents of memory chips (24C02 now; parallel RAM and ROM later) | after 0.5 | — | done |
| SN-209 | Adjustable voltage and current sources (lower and upper limit, turned while running) and controlled sources VCVS, VCCS, CCCS, CCVS (example 29) | after 0.6 | — | done 2026-10-20 |
| SN-210 | 4x4 matrix keypad: keys pressed with the mouse while running, rows and columns connected like the real keypad | after 0.6 | — | done 2026-10-20 (example 30) |
| SN-211 | A/D and D/A converters with 4, 8, 10 and 12 bits (ideal, behavioral, with a reference input) | after 0.6 | — | done 2026-10-20 (example 31) |
| SN-212 | Parallel ROM and RAM chips of several sizes in C++, working with or without a board, contents in the Memory Viewer (ROM from a .bin/.hex file) | after 0.6 | — | done 2026-10-20 (ADR 0103, examples 32, 33) |
| SN-213 | 555 timer, behavioral, with the chip's pins; astable and monostable example | after 0.7 | — | done 2026-10-25 (example 34; passed the owner's test) |
| SN-214 | Generic subcircuit part: any vendor `.subckt` from the model folders as a box with one pin per port, named from the file's comments; a warning that models with internal state may not carry it across segments | after 0.7 | — | done 2026-10-25 (example 35; the owner placed and ran a vendor 555 model) |
| SN-206 | HD44780 character LCD (rows and columns as properties, 8- and 4-bit, busy flag), its text on the schematic (`screen` in part definitions), behind a PCF8574 in example 28 | after 0.5 | — | done 2026-10-19 |
| SN-188 | Revisit the interactive segment length (ADR 0080): ngspice keeps about 0.7 KB (analog example) to 2 KB (Blue Pill circuit) per segment restart; example 07 for one hour grew 7.5 MB with 1 s segments and stayed flat with 10 s segments, which also ran 2.9x instead of 1.8x real time but used 24 MB more | G1 finding | SIM-02 | done 2026-10-16: re-measured, no change (segment length no longer affects speed; 1 s kept) |
| SN-180 | GDB server and whole-simulation pause | N4 / G4 | MCU-08 | done (owner's STM32CubeIDE test 2026-10-18; load like an ST-LINK; watchpoints) |
| SN-181 | Cortex-M3 cycle model validated on the owner's board | N5 / G5 | — | done 2026-10-25 (29 of 30 board measurements within 5 %; default on, setting in Simulate > Settings) |
| SN-182 | Chip component and board profiles that reference it | N6 | PRJ-06 | todo |
| SN-183 | ADR: keep or remove Renode (D4) | N7 | — | done 2026-10-18: removed (owner's decision, ADR 0094) |
| SN-184 | Cortex-M4F core variant (DSP, FPU) | N8 | — | todo |
| SN-185 | STM32F407 peripherals and board | N8 | — | todo |

## Phase 3 — ADC, PWM, components B, release 0.3 (until 2026-11-27)

| ID | Task | Requirements | Status |
|---|---|---|---|
| SN-140 | ADC through the owned F103 model with circuit voltages | MCU-06 | superseded by SN-178 (ADR 0090) |
| SN-141 | Timer/PWM audit of Renode STM32F1 model and support | MCU-07 | superseded by SN-177 (ADR 0090) |
| SN-142 | Component set B | SIM-07 | done 2026-10-18 (logic gates in SN-191) |
| SN-189 | Scope: vertical scale on the left for the selected trace, a 0 V marker per trace, and a vertical offset per trace to move it up and down (owner's request, 2026-10-18) | AN-02 | done 2026-10-18 |
| SN-190 | Export selected digital signals to VCD: *Export VCD...* in the Logic tab writes the checked lanes (MCU pins and probe lanes) as IEEE 1364 VCD, 1 ns timescale, initial values in `$dumpvars`, then only value changes, over the retained history. Owner's request 2026-10-18 | AN-10 | done 2026-10-18 (unit and live GUI tests; the owner decoded a USART capture in PulseView) |
| SN-143 | Lesson 3 example | — | done (example 16, `lesson3_adc_uart.md`) |
| SN-191 | Logic gates (rest of component set B): gates for digital circuits next to the Blue Pill; first in the owner's order 2026-10-18 | SIM-07 | done 2026-10-18: NOT, AND, NAND, OR, NOR, XOR as behavioral 74HC-style gates on a hidden supply VDD (5 V default), example 17 (half adder) |
| SN-192 | USART RX sampled from the circuit (start bit, data bits at the configured baud rate) so that any source driving the RX pin is received, not only another board's link; second in the owner's order 2026-10-18 | MCU-10 | done 2026-10-18: frames decoded from edge times at the bit centers, FE and PE; needs exact edges or bits of four steps; fixed exact edges with two boards (no convergence) |
| SN-193 | Text notes on the schematic: Place > Text Note (`T`), multi-line text, edit by double-click or in Properties, move, copy, undo; `notes` in `.snx`; not part of the netlist. Owner's request 2026-10-18 | UI-17 | done 2026-10-18 (example 17 has a note) |
| SN-194 | Model variants of basic parts: `model` parameter (NPN, PNP, NMOS, PMOS, diode), built-in approximations in `basic_models.lib`, user models folder (Tools > Open Models Folder), chosen user models copied into the project; KiCad-Spice-Library not bundled (per-file vendor terms). Owner's request 2026-10-18 | PRJ-07 | done 2026-10-18 (ADR 0095) |
| SN-195 | Potentiometer wiper position on the symbol: the arrow moves along the body with the position (parameter, or the live control while running) and the percentage is shown next to it; `interactive.wiper` in the library. Owner's request 2026-10-18 | UI-18 | done 2026-10-18 |
| SN-196 | Buses with splitters like hneemann's *Digital*: Bus (`B`) and Splitter (`S`) tools; bit pins are labels named after the bus (`D0`...); renaming a bus renames its splitters; presentation only, same netlist; example 18. Owner's request 2026-10-18 | UI-19 | done 2026-10-18 (ADR 0096) |
| SN-197 | Teaching models: *Diode (fixed drop)* with Vf, *NPN/PNP transistor (simple)* with beta, Vbe and Vce(sat), category *Simplified models*; beta shown as "β=100". Owner's request 2026-10-18 | SIM-10 | done 2026-10-18 |
| SN-198 | Diode kinds: *Schottky diode* (hooked bar) and *Germanium diode* (marked Ge) with their models (1N5819, BAT54, 1N5822; 1N34A, 1N60); models carry the file's `type=`, so vendor diode libraries sort themselves; example 19. Owner's request 2026-10-18 | SIM-11 | done 2026-10-18 |
| SN-199 | Digital storage parts: measured that XSPICE `d_dff` takes `ic` but sees an edge when a segment starts with the clock high (ADR 0099); behavioral D/SR latches, D/JK/T flip-flops, 4-bit counter and shift register with their state in capacitors; optional pins; example 21. Owner's request 2026-10-18 | SIM-12 | done 2026-10-19 |
| SN-200 | Blocks drawn by the user: *Block port* part (name, side), Place > Block copies a project into the project as `block:<name>` with a rectangle made from the ports; the netlist puts the circuit in place (`U1.R1`, nets around it keep their names, capacitors carried across segments); double-click opens the block's file, *Update* reads it again, opening a project offers to update changed copies; copy/paste carries blocks; format version 2; example 20. Owner's choice 2026-10-18 ("Arquivo próprio") | PRJ-06 | done 2026-10-19 |
| SN-201 | Regulators and references: *Voltage regulator (78xx)* (Vout, dropout, current limit, quiescent current), *Low-dropout regulator (LDO)* (3.3 V, 1117), *Adjustable regulator (LM317)*, *Shunt reference (TL431)*; behavioral, input current = load current; example 22; every analog example now runs in an interactive session test. Owner's request 2026-10-19 | SIM-07 | done 2026-10-19 |
| SN-202 | 7-segment display (common cathode and common anode, decimal point): seven LED segments drawn as bars that light with their own current (one glow per segment, not one per part as today); an example with a counter and a decoder or a Blue Pill driving it. Mentioned as a set-B candidate on 2026-10-16 but never started; owner's question 2026-10-19 | SIM-07 | done 2026-10-19 (displays CC/CA with real pinout, 4511/7447 with all pins and power, hex decoders, decade counter, example 23) |

## Phase 4 — Debugging and packaging, release 0.4 (until 2026-12-11)

| ID | Task | Requirements | Status |
|---|---|---|---|
| SN-150 | CubeIDE/GDB debugging with the cancellable Renode build | MCU-08 | dropped: debugging runs on the own emulator (SN-180); Renode removed (ADR 0094) |
| SN-151 | Engine in a child process: `simnodus-engine`, state mirrored by `RemoteSession`, crash test (ADR 0101) | SIM-08 | done 2026-10-19 |
| SN-152 | Windows installer (Inno Setup, ADR 0100): per user or all users, Start menu, `.snx` association, uninstaller, pt-BR dialogs; `package_windows.py --installer` checks a silent install. Tested by the owner in a clean VM | NF-02 | done 2026-10-19 |
| SN-153 | pt-BR translation: 650 texts of the interface and the component library, Tools > Language (system, English, Português), `tools/update_translations.py`, repository check for untranslated entries | UI-16 | done 2026-10-19 |
| SN-154 | User guide (`docs/user/USER_GUIDE.md`, Help > User Guide, packaged as USER-GUIDE.md) with known limitations; shorter quick start | MCU-09 | done 2026-10-19 |

## Connectivity — AI agents and external I/O (from 2026-10-25)

Owner's priority 2026-10-25: agents first, the virtual plant afterwards
([ADR 0104](../decisions/0104-external-control-and-io.md)).

| ID | Task | Status |
|---|---|---|
| SN-215 | JSON command vocabulary (`src/automation`) and `simnodus-cli` (one-shot commands, `serve`, `exec`); `Session::pause_at` (engine protocol 8); `docs/user/AUTOMATION.md`; packaged | done 2026-10-25 |
| SN-216 | Control channel in the desktop application: local endpoint off by default, the same commands, edits on the canvas and in the undo history, pacing and highlights for recorded tutorials (an agent builds a circuit step by step on screen); `simnodus-cli connect`, Tools > Allow External Control, `--control`, `view.*` commands | done 2026-10-25 |
| SN-217 | MCP server over the same commands (`simnodus-cli mcp [--window]`, stdio), for Claude Code, Codex and other agents | done 2026-10-25 |
| SN-218 | External I/O for the virtual plants: bit and word input/output parts with a literal address (`%IX0.10`), polarity, word scale 0–1000 ↔ 0–3.3 V by default (a parameter); TCP link with one JSON object per line (host, port, 20 ms period), active only in projects with such parts; safe outputs sent on stop and pause | done 2026-10-25 |

## Component library expansion (from 2026-10-26)

Owner's request of 2026-10-26: let students simulate almost every part of
their automation projects. Batches, each with parts, examples, guide and
translation; the order may change with the owner's lessons.

| ID | Task | Status |
|---|---|---|
| SN-219 | Save a project with its firmware: File > Save with Firmware copies each board's `.elf` and memory contents files into `<name>_files` next to the `.snx` with relative paths, so a student's project runs on the teacher's PC; `project.save` with `"with_files": true`; saving elsewhere keeps relative paths in the CLI too | done 2026-10-26 |
| SN-220 | Placement shortcuts: V voltage source, I current source, Shift+V voltmeter, Shift+I ammeter | done 2026-10-26 |
| SN-221 | Sensors and transducers in SPICE, the measured quantity dragged while running: LM35, NTC 10 kΩ, PT100, type K thermocouple, LDR, BPW34, ACS712, load cell (bridge), A3144, PC817, solenoid (current and plunger travel); `tools/generate_sensor_parts.py`, example 36; potentiometers on a quantity (°C, lux, mT; logarithmic for light) | done 2026-10-26 |
| SN-222 | Logic chips in DIP packages with VCC and GND pins (no supply, no output): 74HC00/02/04/08/14/32/86, 74HC138, 74HC157, 74HC245, 74HC74 (behavioral SPICE); 74HC161, 74HC193, 74HC165, CD4017 (digital devices); the 74HC595 already exists as a chip for microcontrollers; `tools/generate_logic_chip_parts.py`, example 37 | done 2026-10-26 |
| SN-223 | I2C chips: BMP280, MPU6050, DS3231, TCS34725, SSD1306 OLED (pixels on the schematic: pixel screens in the library, `screen.pixels`); firmware `hal_i2c_oled`, example 38 | done 2026-10-26 |
| SN-224 | SPI chips: MCP3208 (inputs read from the circuit: `device.analog_pins`), MCP41010 (a resistance in the circuit: `device.internal_pins`), W25Q32 (Memory Viewer, large memories copied at most every 0.2 s), microSD card (SPI mode, an image file), HX711 with the load cell; firmware `hal_spi_sensors`, example 39 | done 2026-10-26 |
| SN-225 | Timed one-wire protocols: DS18B20 (1-Wire, search ROM), DHT22/DHT11, HC-SR04 (distance set while running), WS2812B strip (colors on the schematic); device events scheduled from an input now stop the core on time; firmware `hal_onewire_sensors`, example 40 | done 2026-10-26 |
| SN-226 | Motion: hobby servo (angle from the pulse width), stepper motor with two coils, incremental encoder with push button, L298 dual H-bridge | todo |
| SN-227 | Power and interface: LM2596 buck regulator (averaged), TL494 PWM controller, MAX485, MAX232 | todo |
| SN-228 | Sound: active and passive buzzers heard through the computer's speakers | todo |
| SN-229 | Candidates for automation lessons (owner to confirm): inductive proximity sensor NPN/PNP 24 V, 4–20 mA transmitter, ULN2003, HC-SR501 PIR, HC-05 Bluetooth serial module, logic-level MOSFET (IRLZ44N) | todo |

## Later (after freeze or after classroom)

| ID | Task |
|---|---|
| SN-160 | Symbol Editor window (PRJ-05) |
| SN-161 | Subcircuits (PRJ-06): done — vendor `.subckt` models (#161, ADR 0097) and user blocks (SN-200, ADR 0098) |
| SN-162 | Dark theme (done 2026-10-19: View > Theme, system/light/dark, applied at once); ANSI symbol variant still open |
| SN-163 | Headless CLI over the same session API (ADR 0055 direction): done as SN-215 |
| SN-164 | Linux package |
