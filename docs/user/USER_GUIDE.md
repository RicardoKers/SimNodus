# SimNodus user guide

SimNodus is a classroom simulator: you draw a circuit, press F5 and it keeps
running while you press buttons, turn potentiometers and watch LEDs, meters
and the Signal Analyzer. A Blue Pill board (STM32F103C8) can run your own
firmware inside the circuit. This guide describes release 0.5 (on `main`
after 0.4.0). For a first circuit in five minutes, see the
[quick start](QUICK_START.md); for every detail of the simulated
microcontroller, see [MICROCONTROLLER.md](MICROCONTROLLER.md).

Contents: [Installing](#installing) · [The window](#the-window) ·
[Drawing a circuit](#drawing-a-circuit) · [Components](#components) ·
[Models](#device-models) · [Buses](#buses-and-splitters) ·
[Blocks](#blocks) · [Running](#running-a-simulation) ·
[Signal Analyzer](#signal-analyzer) · [Microcontroller](#microcontroller) ·
[Files](#files-examples-and-lessons) · [Preferences](#preferences) ·
[Command line](#command-line) · [Troubleshooting](#troubleshooting) ·
[Known limitations](#known-limitations)

## Installing

SimNodus runs on 64-bit Windows 10 and 11 and works offline; nothing is
downloaded while you work.

- **Installer:** `SimNodus-<version>-windows-x64-setup.exe` installs for you
  alone (no administrator rights needed) or for all users. It adds a Start
  menu entry, an optional desktop icon and opens `.snx` files with a
  double-click. Uninstall from Windows Settings > Apps.
- **Portable:** unzip `SimNodus-<version>-windows-x64.zip` anywhere (a USB
  stick works) and run `simnodus.exe`.

Downloads are on the [releases page](https://github.com/RicardoKers/SimNodus/releases).
Your settings (theme, language, model folders, recent files, window layout)
are kept per Windows user; projects are ordinary files wherever you save
them.

## The window

| Area | What it does |
|---|---|
| **Components** (left, Ctrl+1) | The parts tree by category with a search box. The preview below shows the selected part's symbol, pins and a short description |
| **Schematic** (center) | The drawing. Mouse wheel zooms at the cursor, the middle button or Space+drag pans, F fits the circuit |
| **Properties** (right, Ctrl+2) | Values of the selected part; while running, its live voltages, currents and power |
| **Messages** (bottom, Ctrl+3) | Circuit checks, engine and firmware messages. Double-click a message to show its part |
| **Toolbar** | New, Open, Save, Undo, Redo, Wire, Probe, Run, Pause, Stop and the speed box |
| **Status bar** | Run state, simulated time and the actual speed relative to real time; its tooltip lists the approximations of the current run |

F11 hides all panels (focus on the canvas). **F1** (Help > Keyboard
Shortcuts) lists every shortcut and mouse gesture; Help > User Guide opens
this guide.

## Drawing a circuit

### Placing parts

Choose a part in the tree (double-click, Enter, or drag it onto the
schematic), then click to place it; each click places another copy, Esc
ends. Ctrl+R rotates and Ctrl+E mirrors, also before placing. Single-key
shortcuts: **R** resistor, **C** capacitor, **L** inductor, **D** diode,
**G** ground.

### Wires, labels and notes

| Tool | Key | How |
|---|---|---|
| Wire | W | Click a pin to start, click to add bends, click a pin or a wire to finish. Space changes the corner direction; a double-click ends at an open point; Esc cancels |
| Net label | N | Click a wire or pin and type a name. Labels with the same name are connected without a wire; `GND` or `0` is ground. Ctrl+E puts the name on the other side |
| Bus | B | See [Buses and splitters](#buses-and-splitters) |
| Splitter | S | See [Buses and splitters](#buses-and-splitters) |
| Text note | T | Click where the text starts and type, for example instructions for students. Double-click a note to edit it; an empty text deletes it |
| Probe | P | See [Signal Analyzer](#signal-analyzer) |

A **red circle** marks a pin or wire end that is not connected; a **dot**
marks a junction. Hovering a wire or pin shows its net name. Every circuit
needs a **ground**.

### Editing

| Action | How |
|---|---|
| Select | Click, Ctrl+click to add, or drag a rectangle; Ctrl+A selects all |
| Move | Drag (wires stay attached) or arrow keys (one grid step) |
| Rotate, mirror | Ctrl+R, Ctrl+E |
| Delete | Del |
| Clipboard | Ctrl+C, Ctrl+X, Ctrl+V |
| Undo, redo | Ctrl+Z, Ctrl+Y (every edit, including probe moves) |
| Move a label | Drag a part's name or value text |

### Values

Values accept engineering notation: `4k7`, `100n`, `2.2u`, `1meg` (an
upper-case `M` also means mega; `m` is milli). Press Enter to apply. An
invalid value is outlined in red and not applied.

## Components

| Category | Parts |
|---|---|
| **Passive** | Resistor, capacitor, inductor (ideal); potentiometer; transformer (primary P1-P2, secondary S1-S2) |
| **Sources** | DC voltage source, battery (with internal resistance), DC current source (out of +), pulse generator, sine generator, ground |
| **Switches & Controls** | SPST switch, SPDT changeover switch, push buttons NO (closed while pressed) and NC (open while pressed) with optional contact bounce |
| **Instruments** | Voltmeter (1 GΩ input), ammeter (ideal; current into +) |
| **Semiconductors** | Diode (silicon), Schottky, germanium, Zener, LED (glows with its current, color selectable), NPN and PNP transistors, N- and P-channel MOSFETs (body tied to source) |
| **Simplified models** | Teaching models: diode with a fixed forward voltage; NPN and PNP with fixed Vbe, a beta shown on the symbol and a saturation voltage |
| **Integrated circuits** | Op-amp (behavioral or a vendor model, powered by V+ and V-); 78xx regulator (set Vout); 3.3 V low-dropout regulator; LM317 (OUT 1.25 V above ADJ); TL431 shunt reference (2.495 V between REF and A) |
| **Logic gates** | NOT, AND, OR, NAND, NOR, XOR (74HC-like, with a hidden supply) |
| **Flip-flops & counters** | D and SR latches; D, JK and T flip-flops (rising edge); 4-bit counter, decade counter, 4-bit shift register. Q̅ and CLR may stay unconnected |
| **Displays** | 7-segment displays (common cathode, common anode) with the pins of a real display; 4511 (CMOS, common cathode) and 7447 (TTL, common anode, open-collector) decoders with every pin of the chip; generic hex decoders 0-F |
| **Electromechanical** | Lamp (glows with its current), relay (coil and changeover contact; add a flyback diode), DC motor (shows its speed) |
| **Microcontrollers** | Blue Pill (STM32F103C8) running your ELF firmware |
| **Chips for microcontrollers** | 74HC595 shift register; these chips work together with a board (see [MICROCONTROLLER.md](MICROCONTROLLER.md#chips-for-microcontrollers)) |
| **Blocks** | Block port, to turn a circuit into a block |

Notes on some parts:

- **Potentiometer:** the arrow on the body shows the wiper and the
  percentage beside it (0 % at pin 1). Set Properties > Position, or while
  running drag the potentiometer or turn the mouse wheel over it.
- **7-segment displays and decoders:** one resistor per segment; several
  displays side by side make a number (either COM pin may be used). For the
  4511 tie LT and BI high and LE low and wire VDD/VSS; for the 7447 wire VCC
  and GND, RBI/RBO blank leading zeros. Each segment lights with its own
  current, fully at 10 mA.
- **Regulators:** the input current is the load current plus a few mA; with
  too little input voltage the output drops (dropout).
- **Logic gates and flip-flops** run on a hidden supply, so you wire only
  their logic pins. Their outputs are real voltages: an LED on an output
  needs a series resistor.
- **Diode kinds** each have their own symbol and list the models of their
  kind.

## Device models

Select a transistor, MOSFET, diode or op-amp: **Properties > Model** lists
the built-in parts (2N2222A, BC547B, 1N4007, 1N5819, IRF540N, ...) and the
models in your model folders. Built-in models are SimNodus approximations of
typical datasheet values, made for teaching.

**Your own models (Tools > Model Folders):** list the folders that hold
`.lib` files, for example copied from a manufacturer's website. The default
is `Documents/SimNodus/models`; any number of folders can be added.

- `.model` lines appear on parts of the matching kind (diode models marked
  with `type=` appear on the matching diode kind; example 19).
- `.SUBCKT` blocks (for example an LM358, TL072 or TIP120 model) appear on
  op-amps, transistors, MOSFETs and diodes. **Pins...** sets which port of
  the subcircuit is which pin; the file's comments usually give the order.
- A chosen model is copied into the project, so the project opens on other
  computers without the folder.
- Lines or parameters SimNodus cannot use are listed in the Messages panel.

## Buses and splitters

Press **B** and draw a bus like a wire; a double-click ends it. Name it
with a net label, for example `D`. Bit 3 of bus `D` is the net `D3`.

Press **S** to place a splitter and give the widths of its pins on each
side, as in the *Digital* simulator: `8` on the left and `8*1` on the right
splits an 8-bit bus into wires; `1,1,1,1` and `4` join four wires into a
bus; `4` and `2,2` split it into two buses. Splitters of the same bus join
equal bits. Example 18.

## Blocks

A block is a circuit you drew once and place many times, shown as a
rectangle with pins.

1. Draw the stage in its own project and mark its connections with **Block
   port** parts (Blocks in the tree): give each a name (IN, OUT, VCC, ...)
   and the side of the rectangle where its pin goes. Save it.
2. In another project choose **Place > Block...** and pick that file. Place
   it as often as needed.
3. The project keeps a copy of the block. Double-click a block to change its
   file; after editing the block's own project, click **Update** in
   Properties (or reopen the project) to use the new version.

Example 20 uses `examples/blocks/RC_lowpass.snx` twice.

## Running a simulation

| Key | Action |
|---|---|
| **F5** | Run |
| **F6** | Pause and resume (the Pause button stays pressed while paused) |
| **Shift+F5** | Stop |

Before a run SimNodus checks the circuit (ground, unconnected pins, invalid
values) and lists problems in the Messages panel.

While running:

- Click a **switch** to toggle it; hold the mouse on a **push button**; drag
  or use the wheel on a **potentiometer**.
- LEDs, lamps and display segments glow with their current; meters show
  their reading; the relay contact moves; the motor shows its speed.
- Hover a wire for its voltage. **View > Show Voltages** colors the wires.
- Select a part to see its live voltages, current and power in Properties.
- The **speed box** sets the speed relative to real time (0.001x to 1x, or
  Maximum). The status bar shows the speed actually reached.
- Changing the circuit asks to stop first; value changes apply at the next
  run.

### Simulation settings

**Simulate > Simulation Settings** chooses the mode. Both modes start with
discharged capacitors and inductors without current.

| Mode | Use | Settings |
|---|---|---|
| **Interactive** (default) | Continuous run you can interact with | Maximum time step: 100 µs (default), 10 µs (fast signals, more CPU) or 1 ms (slow circuits). Segment length 0.5-2 s (the engine restarts each segment to keep memory bounded). *Exact microcontroller edges* (see below) |
| **Transient** | A fixed time span, computed completely, then opened in the analyzer | Stop time and maximum time step |

Edges of switches and generators are resolved to the time step. With a
microcontroller, **Exact microcontroller edges** gives the circuit a time
point at every firmware edge, so PWM and other square waves are exact on the
scope; runs are slower.

## Signal Analyzer

**Window > Signal Analyzer** (Ctrl+Shift+A) opens it as a separate window,
which can sit on another monitor. It has three tabs: Scope, Logic and UART.

### Probes

Press **P** and click:

| Click | Measures |
|---|---|
| A wire | Its voltage |
| A component pin | The current into that pin |
| Shift+click on a pin | The pin's voltage |
| Alt+click on a second net | The last voltage probe relative to that net (differential) |

Probes are saved with the project. To move one, drag its marker with the
select tool onto another wire or pin: it keeps its color and channel (a
current probe needs a pin). Ctrl+Z undoes the move.

### Scope

- **Time base**, **Roll** or **Triggered** mode. Triggered mode has a
  source, level and slope, and Auto, Normal or Single sweep. The trigger
  point is marked with a T flag and a dotted line; the display says
  *Triggered at ...*, *Waiting for trigger* or *Auto: no trigger, running
  free*.
- **Vertical:** per-channel scale or **Autoscale**. The left edge shows the
  selected channel's volts per division and a colored triangle at each
  channel's 0 V; drag a triangle (or turn the wheel over the scale) to move
  the channel, double-click it to center 0 V again.
- **Cursors** A and B show Δt, 1/Δt and the values. The row below the plot
  shows min, max, mean, RMS, peak-to-peak, frequency and duty cycle of the
  selected channel.
- **Transient results** open as a complete record: the wheel zooms the time
  axis, dragging pans.
- **Export CSV** writes the visible channels (the first line says how the
  data was computed); **Export PNG** saves the plot.

### Logic

Microcontroller pins appear as lanes by themselves when they change. Check
or uncheck any pin or voltage channel in the list; voltage channels read 1
above the **Probe threshold**. The lanes follow the scope's time base, and
the cursors A and B (shared with the scope) show their positions, distance
and frequency. Hover an edge for its exact time; click a lane to list its
transitions. **Export VCD...** writes the checked lanes for PulseView or
GTKWave.

### UART

Choose the port (USART1 on PA9/PA10, USART2 on PA2/PA3, USART3 on
PB10/PB11) to read what the firmware prints. Type a line and press Enter or
Send, with the line ending of your choice. **Time stamps** starts every line
with its simulated time.

## Microcontroller

1. Place **Blue Pill (STM32F103C8)** from Microcontrollers and connect one of
   its **G** pins to ground.
2. In Properties, **Browse...** chooses the firmware: an `.elf` built for the
   STM32F103C8, for example by STM32CubeIDE (HAL projects with the generated
   `SystemClock_Config()` run unchanged). The path is stored relative to the
   project. **Reload** takes a rebuilt file and restarts a running
   simulation.
3. **F5** starts the firmware in SimNodus's own STM32 emulator.

While running, each GPIO pin shows its state: red = output high, blue =
output low, dotted outline = alternate function, hollow = input (its border
turns red when it reads 1). The PC13 LED lights when PC13 is low; click
**RESET** to restart the firmware.

- Outputs drive the circuit like real pins: push-pull 3.3 V or 0 V behind
  about 40 Ω; open-drain only pulls low; pull-ups and pull-downs are 40 kΩ.
- Inputs read 1 above 1.83 V and 0 below 1.16 V (Schmitt trigger).
- Supported: GPIO, EXTI, USART1-3, TIM1-TIM4 (PWM, output compare, input
  capture), ADC1/2, DMA1, SysTick, the clock tree up to 72 MHz, the
  independent and window watchdogs, the RTC with its backup registers, and
  CRC. Two boards can talk over their serial pins.
- **Debugging:** check **Simulate > Debug Firmware with GDB (port 3333)**,
  press F5 and connect STM32CubeIDE or arm-none-eabi-gdb to `localhost:3333`.
  While the firmware is halted at a breakpoint, the circuit stands still.

Step-by-step instructions, the full list of peripherals and every difference
from a real board are in [MICROCONTROLLER.md](MICROCONTROLLER.md).

## Files, examples and lessons

- Projects are `.snx` files: File > Save (Ctrl+S), Save As (Ctrl+Shift+S),
  Open (Ctrl+O), Open Recent. Saving keeps the previous version as
  `.snx.bak`.
- Unsaved changes are written to a **recovery file** every minute. If
  SimNodus closes unexpectedly, the next start offers to restore them.
- **File > Examples** opens the bundled circuits as untitled copies, so the
  originals stay unchanged. The [examples list](../../examples/README.md)
  says what each one shows and what to try: basic analog circuits (01-06),
  Blue Pill firmware (07-09, 12-15), logic and buses (17, 18), diode kinds
  (19), blocks (20), a counter (21), regulators (22), a 7-segment counter
  (23), the RTC with a watchdog (24) and a 74HC595 shift register (25).
- **Lessons** 1-3 (examples 10, 11 and 16) come with notes in
  `examples/lessons/`: goals, steps and the expected observations.
  Their firmware sources are in `examples/firmware/`.
- Projects that contain blocks are saved as format version 2, which
  SimNodus 0.3 cannot open; other projects stay readable by 0.3.

## Preferences

| Menu | Setting |
|---|---|
| View > Theme | System (follows Windows, default), Light or Dark; applied at once |
| Tools > Language | System language (default), English or Português (Brasil); takes effect at the next start |
| Tools > Model Folders... | Folders with your `.lib` model files |
| Tools > Check Simulation Engine | Loads the analog engine and solves a reference circuit |
| View > Components / Properties / Messages | Show or hide the panels (Ctrl+1/2/3); the layout is remembered |

## Command line

| Command | Effect |
|---|---|
| `simnodus.exe FILE.snx` | Open a project |
| `simnodus.exe FILE.snx --run` | Open and start the simulation |
| `simnodus.exe --check-engine` | Load ngspice and solve a reference circuit |
| `simnodus.exe --check-mcu-engine` | Run `blink.elf` in the microcontroller emulator |
| `simnodus.exe --smoke-test` | Open the windows, run built-in checks, exit |
| `simnodus.exe --settings FILE` | Use FILE instead of the user's settings |

## Troubleshooting

| Problem | What to do |
|---|---|
| The run does not start | Read the Messages panel: usually a missing ground, an unconnected pin or an invalid value. Double-click the message to find the part |
| The engine reports that the circuit did not converge | Look for floating nodes (a capacitor alone between two nets, a gate without a path to ground), ideal sources in parallel or a switch shorting a source. Try a smaller maximum time step |
| The status bar shows a speed below 1x | The circuit needs more computation than real time allows. Use a 100 µs step or longer, turn off *Exact microcontroller edges* if you do not need it, or lower the speed |
| A PWM looks like a ramp or a UART frame is not visible on the scope | The edges are averaged over the time step: turn on *Exact microcontroller edges*, use a 10 µs step, or look at the Logic tab |
| The firmware does nothing | Check that a G pin is grounded, that the ELF was built for the STM32F103C8, and the Messages panel for unsupported peripherals |
| The board waits forever | *Debug Firmware with GDB* is checked: connect a debugger or uncheck it |
| A model from my folder is missing | Check Tools > Model Folders and the Messages panel; the model must match the part kind (a diode model on a diode) |
| The engine cannot be found or "stopped unexpectedly" | Tools > Check Simulation Engine; reinstall if `simnodus-engine.exe` or the `ngspice` folder next to `simnodus.exe` is missing. If one circuit stops the engine every time, please report it with the project file |

## Known limitations

SimNodus is a teaching tool. These limits are known and stated so that
results are read correctly.

### Circuit simulation

- Device models are approximations of typical values, not datasheet
  matches. Open switches are 10^15 Ω, the voltmeter has a 1 GΩ input and
  the ammeter is ideal; LEDs have no current limit, so an LED without a
  resistor draws an unrealistic current.
- Interactive runs are computed in segments (1 s by default).
  Semiconductor junction charge is not carried from one segment to the
  next, so a tiny step may appear once per segment in circuits that depend
  on it. Capacitor voltages and inductor currents are carried over.
- Events (a click, a switch, a firmware edge) are resolved to the maximum
  time step unless *Exact microcontroller edges* is on.
- On Windows, pacing to real time can lag by up to about 25 ms (timer
  granularity); simulated time itself is not affected.
- There is no AC (frequency sweep), noise or FFT analysis; use the
  transient and interactive modes.
- Logic gates, flip-flops and counters are behavioral models on a hidden
  supply, not transistor-level models of a 74HC chip.

### Microcontroller

- Only the Blue Pill (STM32F103C8) is available.
- Not simulated yet: SPI and I2C (planned with devices for the library), CAN,
  USB; timer encoder and
  center-aligned modes, dead time and break; ADC conversions started by a
  timer or EXTI, dual and discontinuous modes. The Messages panel says when
  the firmware uses one of the partly supported features.
- One instruction per clock cycle: busy-wait loops run faster than on the
  board; code timed with SysTick, timers or `HAL_Delay()` is right.
  Peripherals work even if their clock enable was never set.
- Firmware and circuit are coupled every 100 µs: circuit changes reach the
  firmware up to 0.1 ms late, and the ADC reads the pin voltage of
  0.1-0.2 ms before the sample instant.
- Exact microcontroller edges cost speed (about 0.8x-2.9x real time at a
  100 µs step; about 0.2x at 10 µs with a 10 kHz PWM, on a 2026 laptop).
- USART RX from the pin needs exact edges or a bit at least four time steps
  long (2400 baud or slower at 100 µs); a received byte reaches the firmware
  about 0.4 ms after its stop bit.
- Transient mode does not run microcontrollers: use interactive mode.
- Chips for microcontrollers (74HC595) work only together with a board and
  read levels digitally; their inputs on nets without a board pin read only
  constant ties (ground, a supply, open = low).

### Application

- Windows only for now; a Linux package is planned (the code builds on
  Linux).
- The simulation runs in a separate program (`simnodus-engine`): if it
  fails, the run stops with a message and SimNodus and your project stay
  open; press F5 to run again.
- Resistors use the IEC rectangle; an ANSI zigzag variant and a symbol
  editor are not available yet.
- The interface is in English and Brazilian Portuguese; this guide is in
  English only.
