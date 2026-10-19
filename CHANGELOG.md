# Changelog

All entries describe actual repository changes, not promised simulator capabilities.

## Unreleased

### Changed (2026-10-19, owner's ergonomics requests)

- Live values of vertical parts (a voltmeter or ammeter standing upright) appear on the right of the symbol; of horizontal parts, below it as before. Texts inside symbols (the V and A of the meters, gate and IC pin names) stay upright and unmirrored whatever the part's rotation.
- Net labels can be mirrored (Ctrl+E): the name then sits left of the connection point, for wires that arrive from the right. Saved as `"mirrored": true`.
- The relay's contact moves: it rests on NC and switches to NO while the coil current is above pull-in (released below drop-out, as the part's switches do); the line from the coil to the contacts is gone.
- A *Block port*'s side turns the port: it points left, right, up or down as the pin will on the block, with its wire coming from the circuit. A choice parameter may set the part's orientation (`"orientations"`).
- Logic analyzer cursors work like the scope's: drag A and B on the lanes (they are shared with the scope). For the selected lane the analyzer shows its timing in the window — period, frequency, mean high and low pulse width, duty cycle — and with cursors on, A, B, Δt, 1/Δt, the lane's level at A and B and the edges between them.
- Model folders are the user's choice: *Tools > Model Folders...* lists any number of folders (add, remove, open, back to the default Documents/SimNodus/models); all of them are read for the Model lists. The list is kept in the settings.

### Added (2026-10-19, owner's request)

- Regulators and references (SN-201) in *Integrated circuits*: *Voltage regulator (78xx)* with output voltage, dropout, current limit and quiescent current; *Low-dropout regulator (LDO)* (3.3 V, like the 1117); *Adjustable regulator (LM317)* (1.25 V between OUT and ADJ, 50 µA out of ADJ); *Shunt reference (TL431)*. Behavioral; the input current is the load current plus the quiescent current. Engine tests: 7805 4.9995 V at 50 mA, 4.000 V from 6 V (dropout), 1.000 V into 1 Ω (1 A limit), LDO 3.299 V, LM317 5.036 V with 240 Ω/720 Ω, TL431 2.495 V and 4.99 V with a 1:1 divider. Example 22. A new engine test runs every analog example in an interactive session: the first version of the regulators passed the DC checks but could not start a run from 0 V, so each regulator now has an error amplifier with a 1 µs dominant pole.

### Added (2026-10-19, owner's request)

- Latches, flip-flops and counters (SN-199, SIM-12, ADR 0099) in a new *Flip-flops & counters* category: D latch, SR latch, D, JK and T flip-flops (rising edge), a synchronous 4-bit counter and a 4-bit shift register with asynchronous clear, on a hidden supply like the logic gates. They are behavioral master-slave parts whose state is in capacitors, so it survives interactive segments: measured, ngspice's XSPICE `d_dff` sees a false edge when a segment starts with the clock high. Engine tests: all parts in an 18 ms transient; a counter over 17 segments with no wrong count in 6257 samples. Pins may be marked optional (Q̅, CLR stay unconnected without a warning). Example 21: a push button clocks the counter, LEDs show the count.

### Added (2026-10-19, owner's priority)

- Blocks drawn by the user (SN-200, PRJ-06, ADR 0098): a project with *Block port* parts (name and side, category *Blocks*) can be placed into other projects with *Place > Block...*; it appears as a rectangle with the ports as pins, in the order they have on the block's schematic, and in the parts tree. The project keeps a copy of the block file (`"blocks"`, format version 2; projects without blocks stay at version 1). The netlist puts the block's circuit in its place: parts are named `U1.R1`, nets around the block keep the names the editor shows, capacitors inside keep their charge across interactive segments (engine test: 4.7511 V at 300 ms against 4.7511 V expected), findings inside point at the block. A double-click opens the block's file, *Update* in Properties reads it again, and opening a project offers to update blocks whose files changed. Copy and paste carry the blocks of the copied parts; blocks may hold blocks. Example 20: a square wave through two RC low-pass blocks.

### Added (2026-10-18, owner's requests)

- Teaching models in a new *Simplified models* category (SN-197): *Diode (fixed drop)* conducts above an adjustable forward voltage (0.7 V by default, 0.3 V for germanium lessons); *NPN/PNP transistor (simple)* have a fixed Vbe, an adjustable beta shown on the symbol (β=100) and a saturation voltage (0.2 V). Engine tests: Vbe 0.700 V, Vc 7.70 V in the active region and 0.185 V saturated.
- Diode kinds (SN-198): *Schottky diode* (hooked bar) with 1N5819, BAT54, 1N5822 and *Germanium diode* (marked Ge) with 1N34A, 1N60; the plain diode lists silicon models. A model file's `type=` (as in LTspice's diode library: silicon, schottky, germanium, zener, led...) decides which part lists it. A named default model now shows on the symbol. Example 19 compares the four kinds (0.58, 0.18, 0.27 and 0.70 V at about 1 mA).

### Added (2026-10-18, owner's priority)

- Vendor subcircuits as part models (PRJ-06 first part, ADR 0097): the models folder now gives `.subckt` blocks too, each with the models and helper blocks of its file that it uses. Op-amps (IN+, IN-, V+, V-, OUT), NPN/PNP (C, B, E), MOSFETs (D, G, S) and diodes (A, K) offer the subcircuits with one port per pin in Properties > Model; *Pins...* assigns ports to pins with the file's comments in view. The text is kept in the project. Blocks with `.include`, `.lib`, `.control` or other commands are skipped and reported. The package ships ngspice's `spice2poly.cm` code model, loaded at start, so `POLY` sources of vendor models work. Checked with real files: LM358, TL072, OP07 (gain-2 amplifier within 0.2 %), TIP120 and AO3400; tests use SimNodus's own subcircuits.

### Fixed (2026-10-18)

- After an error ngspice cannot recover from (for example a malformed model or subcircuit), every later run failed until SimNodus was restarted. ngspice asks to be detached in that case (its ControlledExit callback); SimNodus now unloads the library and loads a fresh copy before the next run. Engine test: a broken model, then 39 more engine cases on the reloaded engine.

### Fixed (2026-10-18, owner's question about invalid models)

- Imported SPICE models are checked parameter by parameter (ADR 0095): a value that is not a number (for example `IS=abc`) stopped ngspice until SimNodus was restarted, and a name unknown to the model type was ignored without a word; both are now left out of the model and reported. The Messages panel lists every skipped statement and left-out parameter of the models folder with file and line; a part whose kept model has such a parameter gets a pre-run warning. Descriptive vendor fields (`mfg`, `Vceo`, `Iave`...) are left out silently. If ngspice still stops after an error, the message says that SimNodus must be restarted.

## 0.3.0 — 2026-10-18

### Summary

- Logic gates (NOT, AND, NAND, OR, NOR, XOR) and buses with splitters, like hneemann's *Digital*; examples 17 (half adder) and 18 (4-bit bus).
- Text notes on the schematic, for instructions to students.
- Model variants of transistors, MOSFETs and diodes (2N2222A, BC547B, 1N4007, 1N5819, IRF540N...), and the user's own `.model` files from a models folder; a chosen model is saved in the project.
- The potentiometer shows its wiper position (arrow and percentage); dragging follows the mouse whatever the part's rotation.
- USART RX decoded from the circuit (any source on the RX pin, with exact edges or slow baud rates); *Exact microcontroller edges* works with two boards.
- *Export VCD...* in the Logic tab for PulseView and GTKWave.
- Renode removed: the own emulator is the only microcontroller engine; the package is 27 MB instead of 145 MB.

### Fixed (2026-10-18, owner's test of the 0.3 preview)

- Boards seemed dead (no pin colors, flat waveforms) after a debugging session: *Simulate > Debug Firmware with GDB* was remembered between sessions, so every later run waited for a debugger, with only a short status-bar hint. The option now always starts off, and a run in debug mode also says so in the Messages panel.
- Dragging a potentiometer moved its wiper against the mouse when the part was turned (examples 14-16 have it upside down): the drag now follows the wiper's track on screen whatever the rotation, and starts from the clicked point.

### Added (2026-10-18)

- Buses with splitters (SN-196, UI-19, ADR 0096), like hneemann's *Digital*: *Place > Bus* (`B`) draws a thick named line (`D`); *Place > Splitter* (`S`) puts a splitter on it with a first bit and a number of bits, and each numbered bit pin acts as the net label `D0`, `D1`, ...; splitters of the same bus join equal bits. Renaming a bus in Properties renames the splitters on it; splitters rotate and mirror, and wires on their pins stay attached. The netlist is the same as with single wires. Example 18 (four switches to four LEDs over a 4-bit bus). Tests: connectivity and net names, rename, rotation, paste, file round trip, Properties.

### Added (2026-10-18)

- The potentiometer shows where its wiper is (SN-195, UI-18): the arrow moves along the body with the position (0 % at pin 1) and the percentage is written next to it, from the Position property while editing and from the control while running. The tree's preview shows the arrow too. Library: `interactive.wiper` with the arrow's track and pin.

### Added (2026-10-18)

- Model variants of basic parts (SN-194, PRJ-07, ADR 0095): NPN, PNP, N- and P-channel MOSFETs and diodes have a *Model* in Properties. Built in: 2N2222A, 2N3904, BC547B, BD139, 2N2907A, 2N3906, BC557B, BD140, 1N4148, 1N4007, 1N5408, 1N5819, 2N7000, BS170, IRF540N, IRLZ44N, BS250, IRF9540N and the former generic models (the default, so existing projects simulate as before); they are SimNodus's own approximations from typical datasheet values. *Tools > Open Models Folder* opens a folder for the user's `.lib` files with `.model` statements (for example from manufacturers); a model chosen from it is saved inside the component, so the project opens the same elsewhere. Models from the KiCad-Spice-Library are not bundled: each file has its own vendor terms. Tests: model file reading, netlists, file round trip, undo, simulations (diode forward drops, MOSFET on-resistance: IRF540N 0.045 ohm, 2N7000 1.8 ohm at 10 V) and the Properties list.

### Added (2026-10-18)

- Text notes on the schematic (SN-193, UI-17): *Place > Text Note* (`T`) puts free text, for example instructions for the simulation, where you click; double-click a note or use the Properties panel to edit it, an empty text deletes it. Notes move, copy, paste and undo like other elements, are saved in the project file (`notes`) and never change the circuit. Example 17 carries one. Tests: file round trip, editing operations, and a GUI case (drag, edit in Properties, undo).

### Removed (2026-10-18, owner's decision)

- Renode (SN-183, ADR 0094). SimNodus's own STM32F103 emulator, the default since 0.2, is the only microcontroller engine: the Simulate menu option *Use Renode for Microcontrollers (Legacy)*, the Renode engine and runner, the Renode client in `third_party/`, the platform description and C# models in `resources/renode/`, the Renode bundle in the Windows package, `tools/fetch_backends.py --renode`, the CMake options `SIMNODUS_RENODE` and `SIMNODUS_REQUIRE_RENODE`, the Renode test suites and comparisons (`engine-renode`, `engine-renode-session`, `emu-firmware`, `emu-differential`, `gui-editor-renode`) and the experiment workflows that downloaded Renode. Board profiles lose the Renode-only fields (`engine`, `platform`, `extensions`, `before_firmware`, `after_firmware`). The test suite runs in about 20 s instead of about 3 minutes. `--check-mcu-engine` now runs the blink example's firmware in the emulator for 1.1 s of virtual time. The *Microcontroller quantum* choice leaves the Simulation Settings dialog: the emulator always couples in windows of 100 µs (projects keep the stored value). The Windows zip shrinks from 145 MB (0.2.0) to about 27 MB.

### Added (2026-10-18)

- USART RX from the circuit (SN-192, own emulator): the receiver decodes frames on its RX pin from the times the pin voltage crossed the input thresholds, sampling at the bit centers like the chip; a low stop bit sets FE, a wrong parity PE (PEIE raises the interrupt). Any source works, for example another board's TX through a resistor or gates. The circuit must resolve the bits: with *Exact microcontroller edges* any baud rate, otherwise bits of at least four time steps; faster frames are not decoded and the Messages panel says why. A frame is decided after a fixed lag (about 0.4 ms) so that every edge has arrived; boards on one net keep the byte link. Tests: emulator cases (in time, late, FE, PE, glitch, disabled) and a session where one board's output reaches the other through 1 kΩ at 115200 baud.

### Fixed (2026-10-18)

- *Exact microcontroller edges* with two or more boards stopped at once ("did not converge ... time step too small"): the boards' edge breakpoints were queued unsorted, so a past edge of the second board made a negative step. They are kept in time order now.

### Added (2026-10-18)

- Logic gates (SN-191, rest of component set B): NOT, AND, NAND, OR, NOR and XOR in a new *Logic gates* category. They are behavioral models like a 74HC gate on a hidden supply VDD (parameter, 5 V by default; 3.3 V next to a Blue Pill): inputs switch at VDD/2, the output goes to 0 or VDD through 50 Ω after about 10 ns, and no supply current is drawn. Engine tests check every truth table at 5 V and 3.3 V and a three-inverter ring oscillator; example 17 is a half adder with switches and LEDs.
- Backlog entries for the owner's requests of 2026-10-18 (not urgent): text notes on the schematic (SN-193, UI-17), model variants for basic components with user-added models (SN-194, PRJ-07), the potentiometer's wiper position on its symbol (SN-195, UI-18), buses with splitters (SN-196, UI-19). Next in order: logic gates (done), then USART RX sampled from the circuit (SN-192).

### Added (2026-10-18, owner's request)

- Signal Analyzer, Logic tab: *Export VCD...* writes the checked lanes (microcontroller pins and probe lanes) to an IEEE 1364 Value Change Dump with a 1 ns timescale, for PulseView or GTKWave (SN-190, AN-10). It covers the retained history from the point where every chosen lane has data (probe lanes are limited by the sample buffer); the initial levels go in `$dumpvars`, then only the value changes. Lane names become VCD identifiers (`U1.PA0` → `U1_PA0`, `V(OUT)` → `V_OUT`). The writer is a Qt-free module (`src/sim/vcd.*`) with a unit test; a live GUI test exports the blinking pins of example 07.

### Changed (2026-10-18)

- `CURRENT.md` rewritten short for resuming in a new session.

## 0.2.0 — 2026-10-18

### Summary

- Microcontrollers run in SimNodus's own STM32F103 emulator (starts at once; Renode stays as a legacy option): GPIO, EXTI, USART with frames on the TX pin and board-to-board links, timers with PWM and input capture, ADC, DMA; STM32CubeIDE HAL projects at 72 MHz.
- Firmware edges reach the circuit as step averages (exact PWM averages) or, with *Exact microcontroller edges*, as exact edges.
- Debugging from STM32CubeIDE or arm-none-eabi-gdb (breakpoints, stepping, watchpoints, `load`); the circuit pauses with the firmware.
- Scope: vertical scale, 0 V markers and offsets.
- Component set B: PNP, P-channel MOSFET, Zener, op-amp, lamp, relay, DC motor, transformer.
- Examples 13–16, among them lesson 3 (ADC and UART); lessons 1–3 with notes.
- Fixed: interactive potentiometers and closed switches started offset by their initial value (since 0.1.0).

### Fixed (2026-10-18, owner's test)

- Scope: the 0 V marker of a channel could not be moved in the Signal Analyzer: the analyzer connected to the scope's marker signals before the scope existed, so each live refresh put the old offset back and clicking a marker did not select its channel in the list. The connections are now made after the scope is created; a live GUI test drags a marker during a run. Where markers overlap, the selected channel's marker now wins.

### Added (2026-10-18, SN-175, serial frames)

- **USART frames on the TX pin:** with BRR set, each byte goes out bit by bit at PCLK / BRR (start, 8 or 9 data bits with even/odd parity, 0.5–2 stop bits); TXE, TC and the byte's time follow the frame, and DMA moves one byte per frame. Bytes from the terminal arrive one frame apart. Measured: 0x55 at 115200 baud gives an edge every 8.625 µs; the HAL example's first line takes 1.74 ms.
- **Board-to-board serial:** a board's UART TX pin on the same net as another board's RX pin passes its bytes on (byte level, one frame apart). Session test: the HAL example on one board, `uart_echo` on the other, echoing what it receives.

### Added (2026-10-18, SN-189, owner's request)

- **Scope vertical scale and offset:** the left edge shows the selected channel's value at each grid line and its 0 V as a dashed line; every channel has a colored 0 V marker (an arrow at the edge when 0 V is off screen). Dragging a marker moves its channel up or down (in fifths of a division), the wheel over the scale moves the selected channel by half a division, and a double-click puts 0 V back at the center. Clicking a marker selects the channel in the list.

### Added (2026-10-18, SN-142, component set B)

- **New components:** PNP transistor (2N2907-like), P-channel MOSFET (BS250-like), Zener diode (Vz parameter), behavioral op-amp (gain 100 000, 1 MHz gain-bandwidth, output within 0.1 V of its supply pins, 50 ohm output), lamp (rated voltage and current; glows), relay (coil voltage, inductive coil, changeover contact with pull-in at 74 % and drop-out at 10 % of the coil current), DC motor (resistance, inductance, back-EMF, inertia, friction; shows its speed in rpm), transformer (turns ratio, primary inductance, coupling 0.999). New categories *Integrated circuits* and *Electromechanical*.
- Library format: readouts may glow (`glow`, used by the LED and the lamp) or show a scaled internal node (`"kind": "node"`, the motor's rpm).
- Tests: each new part in a reference circuit (switching, regulation, gain and clipping, contact change, motor speed, transformer ratio).

### Added (2026-10-18, SN-180)

- **Watchpoints** in the GDB server (Z2 write, Z3 read, Z4 access, any number and length): the core halts after the accessing instruction and reports `watch`/`rwatch`/`awatch` with the address. Checked with arm-none-eabi-gdb: `watch uwTick` stops at each change (old 0, new 1; old 1, new 2), `rwatch` at the read in `main`. While a watchpoint is set the emulator leaves its fast path.

### Changed (2026-10-18, SN-180, owner's test)

- **The debugger loads the firmware, as with an ST-LINK:** with Simulate > Debug Firmware with GDB, a board may start without a firmware file, and the debugger's `load` (STM32CubeIDE's *Download*) writes the image; the first run or step after a download resets the chip so it starts from the new vector table. Checked with arm-none-eabi-gdb loading `hal_clock.elf` over a running blink firmware.
- The owner debugged a firmware from STM32CubeIDE 2.1.1 (gate G4 test); the guide's STM32CubeIDE steps now keep *Download* on.

### Added (2026-10-16, SN-180 part 2)

- **Simulate > Debug Firmware with GDB (port 3333)**: the next run gives each board on the own emulator a GDB server (3333, 3334, ...); the firmware waits at its reset handler for STM32CubeIDE or arm-none-eabi-gdb. While a debugger halts the firmware, the circuit stands still; the status bar shows *Waiting for the debugger* or *Halted by the debugger*, and after a resume the run continues at its pace instead of catching up. User guide: MICROCONTROLLER.md, "Debugging with STM32CubeIDE or GDB".
- Session test: waiting at 0 s, Ctrl-C, a breakpoint in the SysTick handler with the circuit standing still (no drift over 300 ms), detach and both run on.

### Added (2026-10-16, SN-180 part 1)

- **GDB server in the own emulator** (`src/emu/gdb_server.*`, `gdb_socket.*`): GDB remote serial protocol on 127.0.0.1 only. Supported: a Cortex-M target description (r0–pc, xpsr, msp, psp, primask, control), register and memory reads and writes (flash through the loader, so `load` works), any number of breakpoints, continue, step, Ctrl-C, `monitor reset` / `monitor reset halt`, detach and kill. A client that connects halts the firmware where it is; after detach it runs on. With a debugger attached, `BKPT` halts the core instead of raising a HardFault. Watchpoints are not supported yet.
- `simnodus-emu firmware.elf --gdb PORT` waits for a GDB client and runs under its control. Checked with arm-none-eabi-gdb from STM32CubeIDE 2.1.1 on the HAL example: breakpoints in `main`, the SysTick handler and `HAL_GPIO_TogglePin`, a backtrace through the interrupt, variables, `next`, `monitor reset`, detach.
- Breakpoints cost almost nothing: the core checks them before each instruction and keeps its fast path (181 MIPS without, the HAL example at about real time with one).

### Measured (2026-10-16, SN-188)

- Segment length re-measured with the current emulator coupling (30 s of circuit time at maximum speed): lesson 1 1.97x / 1.74x / 1.72x, example 13 1.71x / 1.78x / 1.77x, example 14 1.05x / 1.06x / 1.04x real time with 1 s / 5 s / 10 s segments, memory within a few MB in every case. The earlier 2.9x-vs-1.8x difference no longer appears, so the 1 s default stays; the one-hour growth of 7.5 MB with 1 s segments is within the G1 limit.

### Added (2026-10-16, SN-143)

- **Lesson 3 — ADC and UART** (`examples/16_Lesson3_ADC_and_UART.snx`, notes `examples/lessons/lesson3_adc_uart.md`, firmware `lesson3_adc`): potentiometer on ADC1 IN2, single conversions with EOC polling, millivolts over USART1, PWM duty from the code into a 10 kΩ / 1 µF filter read back on IN1. Measured: code 2048 at 1.65 V and 1024 at 0.825 V; the RC reading sits 25–30 mV under duty × 3.3 V because the 200 ms sampling always meets the same phase of the ripple (a question in the lesson).

### Added (2026-10-16, ADR 0093, owner's request)

- **Simulate > Simulation Settings > Exact microcontroller edges (slower)**, saved in the project (`exact_edges`, off by default): the circuit takes a time point at every firmware edge of a wired pin (ngspice breakpoints, steps capped at the firmware's time), so square waves stay square at the 100 µs step. Lesson 1: no points between the levels and a ripple of 0.806 V (formula 0.805 V) instead of 0.58 V read from the points; 1.2x instead of 2.9x real time. Other examples 0.8x–2.9x (table in ADR 0093); a 10 µs step gave the owner the same waveforms at 0.25x.
- The status bar's description of the approximations describes the own emulator (it still described Renode).

### Added (2026-10-16, SN-179)

- **DMA1 in the own emulator** (`src/emu/stm32f1_dma.*`): seven channels with increments, 8/16/32-bit sizes, circular and memory-to-memory modes, half-transfer, transfer-complete and transfer-error flags and interrupts (IRQ 11–17). ADC1 requests channel 1 after each regular conversion; USART1–3 request their TX and RX channels (CR3.DMAT/DMAR). Transfers take no time.
- Example 15 (`15_Blue_Pill_HAL_ADC_DMA.snx`, firmware `hal_adc_dma`): an STM32CubeIDE-style HAL project at 72 MHz with `HAL_ADCEx_Calibration_Start`, `HAL_ADC_Start_DMA` (scan of PA1 and VREFINT, circular), `HAL_TIM_PWM_Start` and `HAL_UART_Transmit_DMA`. Measured: 23 808 scans per second (42 µs each, as the sample times give), VREFINT code 1489, PWM high time 605 µs for code 2482; 2.3x real time without a circuit.
- `simnodus-emu --info` prints the final PC and its function.

### Fixed (2026-10-16)

- ADC: setting RSTCAL or CAL with a read-modify-write (as HAL does, ADON still 1) started a conversion, so HAL's calibration timed out. Only a write that changes no other bit starts one (RM0008 11.12.3).

### Added (2026-10-16, SN-178, ADR 0092)

- **ADC1 and ADC2 in the own emulator** (`src/emu/stm32f1_adc.*`): power-on, calibration, single, continuous and scan conversion, injected channels with offsets and JAUTO, per-channel sample times and the conversion time from PCLK2 and ADCPRE, alignment, analog watchdog, EOC/JEOC/AWD interrupt (IRQ 18), temperature sensor and VREFINT. Pins IN0–IN9 read the circuit: the session sends the wired pins' voltages at every accepted point, and a sample reads the voltage at a circuit time the circuit has always passed (0.1–0.2 ms before the sample), so codes repeat exactly. DMA, timer/EXTI triggers, discontinuous and dual modes are reported once.
- The machine's scheduler runs timers and ADCs as timed devices (`timed_device.hpp`).
- Example 14 (`14_Blue_Pill_ADC_to_PWM.snx`, firmware `adc_pwm`): potentiometer → ADC → PWM duty on an LED and an RC filter, code on USART1. Measured: code 2048 at 1.65 V, 819 at 0.66 V (the codes of the voltages ngspice computed); 1.26x real time with 16 000 conversions per second.
- Tests: ADC codes, conversion and sample times, calibration, scan with per-channel sample times, injected group with offsets and alignment, watchdog, diagnostics; session cases for example 14 and for controls (below).

### Fixed (2026-10-16)

- **Potentiometers, closed switches and normally-closed buttons started at the wrong value in interactive runs** (since 0.1.0): ngspice adds an external source's DC value to the value SimNodus supplies, so a potentiometer at 25 % behaved as 50 % (example 03 at 50 % showed the wiper near the top) and a switch that started closed could not be opened. Interactive control sources are now DC 0.
- After Stop, ngspice's last trial steps no longer let the firmware run ahead.

### Changed (2026-10-16, SN-176, ADR 0091)

- **PWM averages are exact in the circuit.** Each circuit step gets the pin's drive averaged over the step (own emulator), instead of the level at the step's end. A 1 kHz, 25 % PWM into an RC filter: 0.8251 V (ideal 0.825 V) at the default 100 µs step, where it was 0.66–0.99 V; speed unchanged (example 13 and lesson 1 at about 2x real time). An edge is spread over the step that contains it; the logic lanes keep the exact edges.
- Example 13 runs its PWM at 1 kHz again (1 µF filter, capture window 0.9–1.1 ms).
- Tests: the runner's averaged drive over windows with exact edges; example 13 checked within 5 mV past a segment boundary; lesson 1's ripple tolerance is 0.03 V (0.784 V measured from the time points).

### Measured (2026-10-16, SN-176)

- ngspice breakpoints at every firmware edge (`ngSpice_SetBkpt`) with steps capped at the firmware's time (GetSyncData): exact edges and ripple, but about 11 extra time points per edge and no overlap between circuit and firmware: lesson 1 at 0.85x real time (1.84x before), example 13 at 1.0–1.4x. Not taken; numbers in ADR 0091.

### Added (2026-10-16, emulator track N3: SN-177)

- **Timers TIM1–TIM4 in the own emulator** (`src/emu/stm32f1_timer.*`): up/down counting with prescaler, auto-reload preload, one-pulse mode, update disable and request source, TIM1 repetition counter; output compare and PWM modes 1/2 with CCR preload, polarity, TIM1 main output enable and complementary outputs; input capture with edge selection, capture prescaler and overcapture; update and capture/compare interrupts; channel pins with the AFIO remaps; timer clock from the APB prescalers (x2 when divided). Edges come at the exact timer-clock cycle: the machine stops the core at the next timer event. A pin driven by two peripherals (TIM1_CH2 and USART1_TX on PA9) is high while either drives it high. Center-aligned counting, slave modes, input filters, dead time, break and timer DMA are reported once in the Messages panel.
- Example 13 (`13_Blue_Pill_timer_PWM.snx`, firmware `timer_pwm`): 100 Hz PWM into an RC filter (0.823 V measured, 0.825 V ideal), input capture of the period, update interrupt, complementary output.
- Tests: five emulator cases with exact edge times (PWM, complementary output, toggle on match, down-counting PWM, APB divider, one-pulse, URS, preload, remap, input capture with overcapture, diagnostics) and a session case for example 13.

### Measured (2026-10-16, SN-177)

- 1 kHz PWM (25 %) into the circuit at the default 100 µs maximum step: the RC filter averaged 0.66–0.99 V instead of 0.825 V, because an edge acts at the circuit's next time point. With a 10 µs step: 0.825 V, but 0.3x real time. Exact edges in the circuit are SN-176 (ngspice breakpoints).

### Changed (2026-10-16, decisions D1 and D2, ADR 0090)

- **Microcontrollers run in SimNodus's own emulator by default.** Renode is the legacy engine: **Simulate > Use Renode for Microcontrollers (Legacy)** (or `SIMNODUS_MCU_ENGINE=renode`). New MCU features (ADC, timers/PWM, serial frames, debugging) are built only on the emulator; Renode is frozen and stays in the package until decision D4.
- Tests: the default suites run the MCU cases on the emulator; `engine-renode-session` and `gui-editor-renode` keep the Renode path tested.

### Measured (2026-10-16, gate G1 evidence)

- One hour of example 07 on the own emulator: memory grew 7.5 MB with the default 1 s segments and stayed flat (+0.9 MB) with 10 s segments. The growth comes from ngspice at each segment restart (also without a microcontroller: +0.9 MB per 20 minutes with 1 s segments, +0.2 MB with 10 s), not from the emulator. Segment length to be revisited (SN-188). Soak cases: `engine_tests ... "soak:" native [segment]`.

### Changed (2026-10-16)

- Pin histories of pins without a wire stay bounded in both microcontroller runners (they were only trimmed when the circuit read the pin).
- User guide: the quick start and MICROCONTROLLER.md describe both engines (start time, timing, AFIO with the own emulator).

### Changed (2026-10-15, emulator track N1: SN-173)

- The own emulator couples to the circuit deterministically: firmware time advances in windows of 100 µs, and an input from the circuit reaches the firmware at the next window boundary. Two runs with the same circuit repeat exactly; inputs are late by less than 0.1 ms (button-to-LED response 100 µs instead of 1.1 ms with Renode). Measured cost of the smaller window: 2.40x instead of 2.48x real time at maximum speed. Tests: tighter timing assertions on the emulator, a repeatability case and the window measurement.

### Added (2026-10-15, emulator track N1: SN-172, SN-173)

- **Simulate > Use the SimNodus Emulator for Microcontrollers (Experimental):** boards run their firmware in SimNodus's own STM32F103 emulator instead of Renode (`NativeMcuRunner`). The board starts at once; pin changes, inputs from the circuit and UART bytes carry nanosecond times; faults and unsimulated peripherals appear in the Messages panel with the board's name. Board profiles may set `"engine": "native"`; the environment variable `SIMNODUS_MCU_ENGINE` overrides both (for tests).
- The session's microcontroller tests and the GUI editor tests also run on the emulator (`engine-native`, `gui-editor-native`): 7 s instead of 50 s, and 7 s instead of 41 s.
- The emulator reports pin configuration changes as events, so the pin history needs no register polling.

### Decided (2026-10-15)

- Decision D0: SimNodus continues with its own STM32 emulator; [ADR 0089](docs/decisions/0089-own-stm32-emulator.md) accepted after experiment E-07 met its gates ([report](docs/experiments/E-07-results.md)).

### Changed (2026-10-15, emulator track, SN-171)

- The session talks to microcontrollers through an engine-independent interface (`McuBackend`, times in nanoseconds); the Renode runner implements it as `RenodeMcuRunner`. No change in behavior; the logic lanes and the UART time stamps read nanoseconds.

### Added (2026-10-15, emulator track E-07)

- Exception-model scenarios (`tests/experiments/emu-exceptions`): preemption, priority grouping, BASEPRI, pending order, PSP, invalid EXC_RETURN, stack alignment, FAULTMASK with NMI, WFI under PRIMASK, SVC escalation; logs worked out from the ARMv7-M manual and checked in `emu-unit`. Renode deviates in three of them (documented in the scenarios' README).
- CI prints the emulator's speed on Linux and Windows (step "Emulator speed"); `tests/experiments/emu-benchmark/bench.elf` is committed for it.

### Fixed (2026-10-15, found by experiment E-07)

- GPIO: a BSRR write that resets some pins and sets others now changes them in one step, as on the chip. The pins used to go low and high again at the same instant (zero-width glitches in the Logic tab, for example on PA0-PA3 of example 07).

### Added (2026-10-15, emulator track E-07, SN-170: gate G0b work)

- Example firmware with FreeRTOS (`examples/firmware/freertos_tasks`, FreeRTOS 10.3.1 from STM32CubeF1): three tasks and a queue; every task switch writes the task's number to PB0-PB2. No example project yet.
- Emulator: explicit reset kinds (power-on, NRST pin, software) with RCC_CSR flags; every fault exception is reported with its cause, PC and function name. `emu-firmware` compares every example firmware between Renode and the emulator; `emu-unit` adds resets, repeatability, fault diagnostics and a decoder robustness case. Gate status in `docs/experiments/E-07-results.md`.

### Added (2026-10-15, emulator track E-07, SN-170)

- Differential test of the emulator against Renode: `tests/experiments/emu-differential` generates random Thumb-2 blocks covering every encoding group (no UNPREDICTABLE forms) and compares all registers, flags, PRIMASK and scratch memory after each block; ctest `emu-differential` runs a committed 512-block corpus when Renode is present. 20 480 blocks compared with zero differences.
- Emulator speed: frequent instruction forms execute inline in the run loop (2.3x faster); 168-185 MIPS on the HAL example's busy loop and 119-141 MIPS on the benchmark mix in `tests/experiments/emu-benchmark` (Windows, owner's laptop).
- First version of SimNodus's own STM32 emulator, not yet used by the application: `src/emu` (system bus with bit-band aliases, ELF loader, Cortex-M3 core with the ARMv7-M Thumb-2 instruction set, exception model with NVIC priorities, SysTick, SCB, faults and lockup; STM32F103 machine with RCC, flash interface, PWR, GPIO A-E, AFIO, EXTI, USART1-3 at byte level, DWT cycle counter and register stubs that report unsimulated peripherals). `simnodus-emu` runs an ELF from the command line and prints pin changes, UART text, diagnostics and the speed reached. Test suite `emu-unit`: instruction vectors from the ARMv7-M pseudocode and the example firmware (exact 500 ms SysTick periods, the HAL example at 72 MHz, EXTI).

### Planned (2026-10-15, owner decision)

- Own STM32 emulator track: design [MCU_EMULATOR.md](docs/architecture/MCU_EMULATOR.md), plan with gates and decision points [MCU_EMULATOR_PLAN.md](docs/planning/MCU_EMULATOR_PLAN.md), [ADR 0089](docs/decisions/0089-own-stm32-emulator.md) (proposed), experiment E-07 with predeclared criteria, backlog SN-170..SN-187, requirements MCU-10 (serial frames on pins) and MCU-11 (own emulator), risks R-110..R-117. The plan was reviewed by an independent agent review before publication; its findings are folded in (revision 2).

### Fixed (2026-10-15, owner question)

- A USART TX pin (PA9, PA2, PB10) in alternate-function mode now rests at 3.3 V while the USART and its transmitter are enabled, as on the board; before it read 0 V. The bits themselves are still not drawn on the pin (Renode moves whole bytes). Renode test `a USART TX pin rests high while the transmitter is enabled`.

### Added (2026-10-15, phase 2: SN-137)

- Clock model for the Blue Pill ([ADR 0088](docs/decisions/0088-clock-model.md)): RCC ready flags (HSE, PLL, LSE, LSI) come on when enabled, SWS follows SW, and the CPU speed and SysTick follow HCLK, so the `SystemClock_Config()` that STM32CubeIDE generates runs and HAL projects work at 72 MHz. The Cortex-M3 bit-band aliases are mapped (the F1 HAL uses them), and `FLASH_ACR`/`PWR` keep what is written. RESET returns to 8 MHz.
- Example 12 (`examples/12_Blue_Pill_HAL_72MHz.snx`): firmware written like an STM32CubeIDE project with the STM32CubeF1 HAL; prints the clock frequencies on USART1, blinks PC13 every 500 ms and shows a 1 ms pulse of 72000 instructions on PA1. `tools/build_firmware.py` builds such projects from the local STM32Cube repository (`cube.json`); notices for the HAL, CMSIS and newlib in THIRD-PARTY-NOTICES.txt.
- Renode tests: the clock model (ready flags, SWS, SysTick nine times faster after a switch to PLL x9, back to 8 MHz after reset) and the HAL example (UART report, 500 ms ticks, 1000 µs pulse).

### Added (2026-10-15, owner request)

- Application icon (provided by the owner): `simnodus.exe` carries the multi-resolution Windows icon, the windows use it through a Qt resource, and Linux installs it under `share/icons/hicolor` with a `simnodus.desktop` file. Files in `resources/icons/` ([origin and use](resources/icons/README.md)). The app's `--smoke-test` checks the icon.

### Added (2026-10-15)

- User page [MICROCONTROLLER.md](docs/user/MICROCONTROLLER.md): supported STM32F103C8 peripherals, what is not simulated yet, and the differences from a real board (MCU-09, release 0.2 state).

### Added (2026-10-15, phase 2: SN-136)

- Lessons 1 and 2 (`examples/10_Lesson1_GPIO_output.snx`, `examples/11_Lesson2_button_and_EXTI.snx`) with firmware sources and notes in `examples/lessons/` (goals, circuit, firmware, steps, expected observations, questions, limits). Tests check them against theory: lesson 1's RC ripple matches 3.3·tanh(T/4τ) within 2.5 %; lesson 2 counts 9 falling edges per press and release without the RC filter and exactly one with it. Input changes from the circuit are now queued and replayed in order with their spacing, so pulses shorter than the coupling quantum (bounces) reach the firmware.
- Push buttons have a **Contact bounce** parameter: a fixed bounce pattern for about 3 ms after each press and release.

### Changed (2026-10-15)

- No GPIO event is lost or late any more: the loopback server holds events Renode raises after a RunFor returned and sends them with the next one, and the runner reports a chunk as complete only after the next chunk (ADR 0085 update). This removes intermittent late or missing edges (a whole 2 ms chunk of PA1 edges in about one run of seven, seen on Linux CI and reproduced on Windows). The firmware now leads by up to two quanta and inputs arrive up to two quanta late (2 ms at the default 1 ms quantum); runs stay at 1.00x real time.
- After Stop, the circuit no longer records the few milliseconds that the analog engine computes after the firmware has stopped (they showed frozen pin levels; on a loaded computer up to 2.5 ms, which made the lesson 1 test fail on CI). Events of an earlier chunk keep their own time instead of being moved to the chunk start, and an output pin's own level seen by the circuit no longer runs extra firmware time.
- Interactive runs use Gear integration ([ADR 0087](docs/decisions/0087-gear-integration-interactive.md)): the trapezoidal default rang on abrupt switching (a bouncing button over 1 µF swung to −2.4 V). Transient mode is unchanged.

### Added (2026-10-15, owner request)

- Probes can be moved: drag the probe marker with the select tool onto another wire or pin; the probe keeps its color and analyzer channel (a current probe needs a pin); undoable. GUI test `probes_are_dragged_to_another_wire_or_pin`.

### Added (2026-10-15, phase 2: SN-135)

- Logic lanes in the Signal Analyzer (AN-07): every board GPIO pin and a digital view of every voltage channel (adjustable threshold); pins appear by themselves when they change; the lanes follow the scope's time window and cursors; edge times on hover and a transition list for the selected lane. The runner keeps the newest 50 000 level changes per pin.

### Added (2026-10-15, phase 2: SN-134)

- UART terminal in the Signal Analyzer ([ADR 0086](docs/decisions/0086-uart-terminal.md)): choose a board's USART1–3, read what the firmware transmits, send text with a selectable line ending, optional time stamps per line. New `UART` command of the loopback control protocol (server and client); USART1–3 in the platform; the board's `uarts` list in the library.
- Example 09 with `firmware/uart_echo` (USART1 at 115200 baud on a real board): "tick N" every second, echo, `1` / `0` switch the LED on PA0.

### Added (2026-10-15, phase 2: SN-133)

- GPIO inputs read the circuit ([ADR 0085](docs/decisions/0085-gpio-inputs-and-quantum.md)): Schmitt thresholds 1.16 V / 1.83 V on each pin voltage; levels reach the firmware within the coupling quantum, new project setting `mcu_quantum` (default 1 ms, 0.1–20 ms) in Simulation Settings. EXTI lines 0–15 from ports A–C routed to the NVIC as on the chip; a push button on the schematic fires the firmware's interrupt (measured response 1.1 ms).
- Own STM32F1 GPIO model `resources/renode/SimNodusGpio.cs` (from Renode's MIT model): correct CRL/CRH reset value and a real ODR register, so internal pull-up/down work; board `extensions`, `ground_pin` and input thresholds in `pin_model`.
- Renode skips the CPU's idle time (`SetGlobalAdvanceImmediately`): idle firmware runs 2–4x faster with unchanged timing.
- Example 08 with `firmware/button_irq` (EXTI1 toggles an LED at each press); input pins show their level on the board symbol.

### Added (2026-10-15, phase 2: SN-132)

- GPIO outputs drive the circuit ([ADR 0084](docs/decisions/0084-gpio-output-coupling.md)): each Blue Pill GPIO pin is a behavioral source set from the firmware's pin history (push-pull 3.3 V / 0 V behind 40 Ω, open drain, 40 kΩ pull-up/down for inputs); the circuit never runs ahead of the firmware. New template token `{ext:NAME}` and board `pin_model`.
- Example 07 drives a red LED through 330 Ω from PA0 (2 Hz, about 3.8 mA) with V(PA0) and the LED current in the analyzer. Test: blink firmware into 1 kΩ gives 3.1731 V and edges within 0.1 ms of 250/500/750/1000 ms.

### Added (2026-10-15, phase 2: SN-131)

- Blue Pill (STM32F103C8) component (`resources/library/mcu/bluepill.json`): the 40 header pins in board order, 3.3 V regulator, PC13 LED and reset button; board profile as library data (`mcu` section, [format](docs/architecture/COMPONENT_LIBRARY.md)). File parameters (`"kind": "file"`) with Browse and Reload in Properties; paths are stored relative to the project and rewritten when the project is saved elsewhere; example copies resolve them against their source folder.
- Interactive runs with a board start Renode first and keep firmware and circuit within 20 ms of simulated time in both directions ([ADR 0083](docs/decisions/0083-microcontroller-lockstep.md)). While running, each GPIO pin shows output high/low, alternate function or input; the PC13 LED lights; clicking RESET restarts the firmware.
- Example `07_Blue_Pill_blink.snx` with `examples/firmware/blink` (C without libraries, committed ELF, `tools/build_firmware.py`, compiled with the GNU Tools for STM32 14.3.rel1 of STM32CubeIDE); the platform description now has GPIO ports B and C.
- `--screenshot-delay` for checks of runs that start slowly.
- Measured Renode deviations recorded in ADR 0083: SysTick 125 ppm fast (RVR instead of RVR + 1 cycles), GPIO configuration reads as analog after reset.

### Added (2026-10-14, phase 2)

- The Windows package contains the microcontroller engine: the unmodified Renode 1.16.1 portable bundle in `renode/` (license texts in `renode/licenses/`, reviewed in ADR 0082) and the support files in `mcu/`. New `simnodus.exe --check-mcu-engine` starts Renode, runs 1 ms of virtual time and stops it; the package script runs it. The zip grows to about 137 MB.
- SN-130: microcontroller engine `src/mcu` ([ADR 0082](docs/decisions/0082-renode-engine-integration.md)). `RenodeEngine` starts one Renode 1.16.1 process per run (Windows Job Object, Linux parent-death signal), loads the loopback-only control server and the STM32F103C8 platform from a per-run folder, and offers exact virtual-time `run_for`, GPIO set/get, GPIO change events with virtual timestamps, 32-bit bus access, and `quit` with a kill fallback. Startup failures report the last lines of the Renode log.
- Vendored Renode external control client (`third_party/renode_external_control`, MIT) with a Winsock/POSIX loopback transport; its callback table now belongs to the connection, and long `RunFor` requests get a longer deadline.
- `tools/fetch_backends.py --renode` also fetches the pinned Linux portable Renode; both CI jobs run the new `engine-renode` test against the real Renode (start, exact time steps, GPIO input, CPU-driven GPIO events from a hand-assembled Thumb loop, failed commands, broken platform, killed backend).

## 0.1.0 — 2026-10-14

First release: analog interactive editor for Windows (portable zip; Linux builds from source).

### Added (2026-10-14, after the owner's retest)

- Help > Keyboard Shortcuts (F1): the shortcut list is read from the menu actions, so it cannot drift from the real keys, followed by the canvas keys and mouse gestures. File > Exit now has Ctrl+Q (Qt's standard Quit key has no binding on Windows). GUI test `shortcut_list_follows_the_menus`.

### Added (2026-10-14, 0.1 polish)

- Crash recovery (NF-04): unsaved changes are written to a recovery file next to the settings every minute; a clean exit or a save removes it, and the next start after a crash offers to restore it.
- Net names on hover while editing (UI-07); differential voltage probes by Alt+click in the Probe tool (AN-02).
- The approximations of interactive mode are stated in the status bar tooltip and as the first line of exported CSV files (SIM-09).

### Fixed (2026-10-14, owner test of the 0.1 build)

- Two more places could delete a widget inside its own signal: the recent-files menu (opening a recent file) and the analyzer's channel scale and visibility controls. Both now rebuild after the event.

- An open switch or push button leaked current (1 GOhm off resistance), so a capacitor behind it slowly charged (0.9 V in 20 s in a 5 V circuit). Off resistance is now 1e15 Ohm; the voltmeter input is 1 GOhm so it does not form a divider with an open switch. Regression test: `session: an open push button does not charge a capacitor` (now about 1 uV after 20 s).
- The application could crash when leaving a value field in the Properties panel by clicking the schematic: applying the value rebuilt the form and deleted the field inside its own focus-out handler. The form is now rebuilt after the event, and old editors are deleted later. Regression test: `leaving_a_value_field_by_clicking_the_schematic_applies_it` (crashed 2 of 3 runs before, 0 of 20 after).

### Added (2026-10-14, phase 1)

- SN-110: Qt-free document model (`src/model`) with stable 64-bit IDs, components, wires, net labels, probes and simulation settings; engineering-notation parser/formatter (`4k7`, `100n`, `1meg`, units); undoable edits computed as the difference between two document states.
- SN-111 (library part): JSON component library format and loader with template validation (`src/library`, [format](docs/architecture/COMPONENT_LIBRARY.md)); the 20 types of component set A in `resources/library`.
- SN-115 (file part): `.snx` project format with atomic save and `.bak` backup, version check and clear errors (`src/io`, [format](docs/architecture/PROJECT_FORMAT.md)).
- Editor operations (`src/editing`): place with automatic designators, orthogonal wires, move/rotate/mirror with attached wires that stay orthogonal, delete, parameters, designators, text offsets, copy/paste with new IDs.
- nlohmann/json 3.11.3 vendored in `third_party/` (MIT).
- SN-116: connectivity from geometry (T junctions, labels, ground, junction dots, open ends), SPICE generation from library templates with control sources for interactive parts, phase-continuous periodic sources (`t0`), and pre-run checks: no ground, no DC path to ground, voltage-source/inductor loops, shorted sources, invalid or out-of-range values, duplicate designators, unknown types, unconnected pins, open wire ends, invalid probes.
- SN-117/118/121 (engine side): `SimulationSession` runs interactive simulations in 1 s segments restarted with capacitor voltages and inductor currents as initial conditions, paced to wall clock x speed (live speed changes, pause/resume, prompt stop), with controls through external sources and bounded ring buffers; transient mode keeps the complete result. Engine failures are explained in plain language. One process-wide engine host shared by the engine check and the session. Tests against the real engine: button charging a capacitor, continuity across segments, pulse phase across segments, 1x pacing, pause, stop, transient accuracy.
- The LED measures its current with a series 0 V source (live brightness).
- Current probes: the netlist inserts a 0 V source in series with a probed pin, giving the exact current into any component pin.
- SN-112/113: schematic editing on the canvas: placement with a ghost symbol (rotate/mirror before placing, repeat until Esc, drag from the tree), selection (click, Ctrl, rubber band), dragging with attached wires that stay orthogonal (one undo step), arrow-key moves, rotate, mirror, delete, cut/copy/paste, select all, movable designator and value texts, orthogonal wire tool (bends, corner toggle, finish on pins/wires), net labels, ground, junction dots and open-end markers. Symbols are drawn from the library data.
- SN-114: Properties panel with engineering-notation validation, apply on Enter/focus-out as one undoable change, common parameters of a multi-selection, designator and label renaming, live values (pin voltages, V, I, P).
- SN-115: New, Open, Save, Save As, recent files, unsaved-changes prompt, examples as untitled copies, project file on the command line.
- SN-118/119: run controls (Run/Pause/Stop/Reset, live speed factor), status bar with simulated time and achieved real-time factor; interactive switches, push buttons (held while the mouse is down) and potentiometers (drag or wheel); LED glow from current, meter readouts on symbols, voltage tooltips, optional wire coloring by voltage; structural edits ask to stop the run first.
- SN-120: Signal Analyzer scope with probe channels, roll and triggered modes (source, level, slope, auto/normal/single), per-channel scale and autoscale, cursors with Δt and values, min/max/mean/RMS/p-p/frequency/duty, CSV and PNG export, transient records with zoom and pan.
- SN-121: Simulation Settings dialog (interactive or transient, steps, segment length).
- SN-122: pre-run diagnostics in the Messages panel; double-click selects and centers the related items; engine failures explained in plain language.
- SN-123 (part): six example projects, quick-start guide (`docs/user/QUICK_START.md`), package with library, examples and guide.
- SN-111: every component of set A is simulated with the real engine in `engine-ngspice` (divider, battery, current source, potentiometer, all switches and buttons, diode, LED colors, NPN, NMOS, instruments, RL/RC transient, pulse and sine generators).

### Added (2026-10-14, phase 0)

- SN-102: Qt 6 Widgets application skeleton `apps/simnodus`: Circuit Editor with component tree (search, preview), schematic canvas (dot grid, zoom at cursor, middle-button and Space+drag pan, fit) and properties columns in adjustable splitters; collapsible panels that reopen at their previous size (Ctrl+1/2/3, F11 focus canvas); collapsible messages panel; status bar; full menu/toolbar with planned actions disabled; separate Signal Analyzer window with scope graticule, timebase and Logic/UART placeholders; window/panel/analyzer settings persisted and restored only onto an available monitor; light Fusion theme; all strings through `tr()`.
- `src/sim`: run-time loading of ngspice (Windows DLL and Linux shared library) with callbacks, external sources and background runs; explicit engine check (Tools > Check Simulation Engine, `--check-engine`); bounded sample ring; process memory query.
- SN-103 spike S1 (continuous run) and SN-104 spike S2 (Windows/Linux ngspice comparison) with reports, reference netlists, the committed Windows ngspice-47 reference result and ADR 0080 recommendation.
- Tests: Qt-free unit tests, real-engine tests, S1/S2 checks and offscreen GUI tests in ctest.
- Tools: `fetch_backends.py` (download, SHA-256 check and staging of the pinned ngspice runtime), `package_windows.py` (portable zip with Qt runtime, ngspice, MSVC runtime, licenses; checks the packaged executable).
- CI workflow building and testing on Ubuntu 24.04 and Windows, uploading the Windows zip.

### Changed (2026-10-14, phase 0)

- SN-101: previous product code (`src/`, `apps/`, `tests/headless`, `tests/schema`, `tests/resources`, root CMake project, editor demo tool) moved to `legacy/`; it still builds on its own (`cmake -S legacy`). Documentation links and experiment scripts point to the new locations. The root CMake project builds only the rebuilt application.

### Changed (2026-10-14, product rebuild)

- Revised requirements (revision 2), QSpice-like desktop UI design with continuous interactive simulation and a separate Signal Analyzer, new product architecture, roadmap with two-week releases (0.1 on 2026-10-30), backlog restarted at SN-100, risks and questions.
- ADR 0079 (product rebuild, lightweight process), ADR 0080 (continuous interactive simulation), ADR 0081 (desktop UI technology).
- Planning documents of the first development line moved to `docs/archive/2026-10-legacy/`; `NEXT_CONVERSATION.md` retired.
- Local SN-045 documentation (automation-first direction, ADR 0055) integrated from the handoff snapshot.

### Added

- E-04 focused STM32F103 ADC extension, owned firmware and native verifier, pinned Renode/ST source audit, repeated 363-conversion local profile, compact evidence, result report, ADR 0012, and Windows workflow.
- Verified integer-microvolt input, one 12-bit quantization, static/boundary/saturation points, two-channel mapping, 101-point ramp, all eight fixed-clock timing selections, start-time sampling, and adverse cases.
- E-03 real Renode/ngspice GPIO/RC/EXTI coupling host, owned firmware, 54-case repeated matrix, compact evidence, result report, ADR 0011, and Windows workflow.
- Measured known-schedule replay and bounded sampled feedback at 1000 us, 100 us, and 20 us, including boundary, pulse, late-input, forced-failure, and fresh-recovery cases.
- SN-013 temporal capability profile and ADR 0010: checked time conversion, measured backend semantics, supported replay, explicitly approximate sampled exchange, unsupported live-feedback operations, and predeclared E-03 gates.
- Owned STM32F103C8 firmware, offline exact-memory profile, real GPIO/input/EXTI experiment, coverage audit, evidence report, ADR 0009, and repeated Windows workflow.
- Native Windows Renode control/time experiment, generated loopback-only server, 20-case suite, pinned source provenance, and a dedicated Windows workflow.
- SN-019 report and ADR 0008: measured timing/reconnection, bounded failures, safe error ownership, and explicit network exposure restrictions; E-02 is now ready.
- Real ngspice RC/lifecycle experiment with eight cases, three recorded local runs, analytical checks, a reference chart, and an opt-in Windows CI workflow.
- ADR 0007 records measured trial/callback, pause, reset, error, and worker-lifetime semantics; no coupled simulation is claimed.
- Pinned Windows backend packages, SHA-256 verifier, C++20 ngspice startup probe, reproducible setup guide, and SN-010 execution report.
- ADR 0006 and SN-019 for the verified native Windows Renode client incompatibility; E-02 external control requires that adaptation.
- English product requirements, architecture, backend/time contracts, and component/subcircuit design.
- Architecture decisions and a Windows-first roadmap targeting classroom use in February 2027.
- Backlog, risk register, open questions, and reproducible experiment specifications.
- MIT license for original project material and third-party review guidance.
- Development scaffolding, repository checks, and GitHub issue/PR/CI templates.
- Public repository under RicardoKers, with Ricardo Kerschbaumer as the initial author.
- Nine initial issues, four dated milestones, private vulnerability reporting, and main-branch protection requiring Windows/Ubuntu checks.

### Not implemented

Production simulation kernel/adapters, general causal feedback, electrical ADC coupling, complete STM32F103 ADC support, circuit editor, project loader, component runtime, and synchronized debugging.
