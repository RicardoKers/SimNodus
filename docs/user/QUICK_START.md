# SimNodus quick start

For release 0.1 (analog circuits). Windows: unzip the package and run
`simnodus.exe`; nothing needs to be installed.

## The window

- **Components** (left): search, then double-click, press Enter or drag a
  part onto the schematic. The preview shows the symbol and its pins.
- **Schematic** (center): mouse wheel zooms at the cursor, the middle
  button or Space+drag pans, F fits the circuit.
- **Properties** (right): edit the selected part. Values accept engineering
  notation: `4k7`, `100n`, `2.2u`, `1meg` (an upper-case `M` also means
  mega). Press Enter to apply; invalid values are outlined in red and not
  applied.
- **Messages** (bottom, Ctrl+3): circuit checks and engine messages.
  Double-click a message to show the part on the schematic.
- Ctrl+1 / Ctrl+2 / Ctrl+3 hide and show the panels, F11 hides all of them.
- **F1** (Help > Keyboard Shortcuts) lists every shortcut and mouse gesture.

## Drawing a circuit

| Action | How |
|---|---|
| Place a part | Choose it in the tree; click to place, repeat, Esc ends. Ctrl+R rotates, Ctrl+E mirrors before placing. Shortcuts: R, C, L, D, G (ground) |
| Wire | W, then click a pin to start, click to add bends, click a pin or wire to finish. Space changes the corner. Double-click ends at an open point. Esc cancels |
| Net label | N, then click a wire or pin. Labels with the same name are connected; GND or 0 is ground |
| Potentiometer | The arrow on the body shows the wiper position and the percentage next to it (0 % at pin 1); set it in Properties > Position, or while running drag the potentiometer or turn the mouse wheel over it |
| Simplified parts | *Simplified models* in the tree: a diode with a fixed forward voltage (0.7 V or what you set) and NPN/PNP transistors with a fixed Vbe, a beta shown on the symbol and a saturation voltage, for first lessons |
| Diode kinds | Diode (silicon), Schottky diode, Germanium diode, Zener diode and LED have their own symbols; each lists the models of its kind, also from vendor files that mark them with `type=` (example 19) |
| Part models | Select a transistor, MOSFET or diode: Properties > Model lists built-in parts (2N2222A, BC547B, 1N4007, 1N5819, IRF540N...; SimNodus approximations of typical datasheet values) and the models of your folder. Tools > Model Folders: list the folders that hold your `.lib` files with `.model` lines (for example copied from a manufacturer); a chosen model is saved in the project. Lines or parameters SimNodus cannot use are listed in the Messages panel |
| Vendor subcircuits | Put a manufacturer's file with `.SUBCKT` blocks (for example an LM358, TL072 or TIP120 model) in the models folder; select an op-amp, transistor, MOSFET or diode and choose it in Properties > Model. *Pins...* sets which port of the subcircuit is which pin; the file's comments (for example "non-inverting input, inverting input, ...") tell the order |
| Bus | B, then draw it like a wire and finish with a double-click; give it a name such as D. S places a splitter on it (first bit and number of bits); wire single signals to its numbered pins. Bit 3 of bus D is the net D3: splitters of the same bus join equal bits. Example 18 |
| Regulators and references | *Integrated circuits* in the tree: 78xx regulator (set Vout: 5 V for a 7805, 12 V for a 7812), low-dropout 3.3 V regulator, LM317 (OUT stays 1.25 V above ADJ) and TL431 shunt reference (2.495 V between REF and A). The input current is the load current plus a few mA; too little input voltage drops the output (dropout). Example 22 |
| Flip-flops and counters | *Flip-flops & counters* in the tree: D and SR latches, D, JK and T flip-flops (rising edge), a 4-bit counter and a 4-bit shift register, on a hidden supply like the logic gates. Q̅ and CLR may stay unconnected. Example 21 counts the presses of a button |
| Block | Draw a stage in its own project and mark its connections with *Block port* parts (*Blocks* in the tree; name IN, OUT, VCC... and the side of the symbol). In another project, *Place > Block...* and choose that file: the stage appears as a rectangle with those pins and can be placed many times. The project keeps a copy. Double-click a block to change its file, then *Update* in Properties (or reopen the project) to use the new version. Example 20 |
| Text note | T, then click where the text starts and type it, for example instructions for the simulation. Double-click a note to edit it; an empty text deletes it |
| Select and move | Click, Ctrl+click, or drag a rectangle; drag to move (wires stay attached), arrow keys move one grid step |
| Edit | Ctrl+R rotate, Ctrl+E mirror, Del delete, Ctrl+C / Ctrl+X / Ctrl+V, Ctrl+Z / Ctrl+Y undo and redo |
| Texts | Drag a part's name or value text to move it |

A red circle marks a pin or wire end that is not connected; a dot marks a
junction. Hovering a wire or pin shows its net name. Every circuit needs a
ground symbol.

## Running

- **F5** runs, **F6** pauses and resumes, **Shift+F5** stops. The speed box
  sets the speed relative to real time.
- While running, click a **switch** to toggle it, hold the mouse on a **push
  button**, drag or use the wheel on a **potentiometer**. LEDs glow with their
  current, meters show their reading, and hovering a wire shows its voltage.
  View > Show Voltages colors the wires.
- Select a part to see its live voltages, current and power in Properties.
- Changing the circuit while running asks to stop first; value changes apply
  at the next run.
- Simulate > Simulation Settings switches between **interactive** (continuous)
  and **transient** (fixed time span, complete result) mode. With a
  microcontroller, **Exact microcontroller edges** makes PWM and other
  firmware square waves exact on the scope at the cost of speed.
- **Simulate > Debug Firmware with GDB** lets STM32CubeIDE or
  arm-none-eabi-gdb debug the firmware while it drives the circuit (see
  [MICROCONTROLLER.md](MICROCONTROLLER.md#debugging-with-stm32cubeide-or-gdb)).

Interactive runs are computed in 1 s segments with a 100 µs maximum time
step by default; edges of switches and generators are resolved to that step.

## Microcontroller (Blue Pill)

What the simulated STM32F103C8 supports and where it differs from a real
board: [MICROCONTROLLER.md](MICROCONTROLLER.md).

- Place **Blue Pill (STM32F103C8)** from Microcontrollers and connect one of
  its **G** pins to ground. In Properties, **Browse...** chooses the firmware
  (an `.elf` built for the STM32F103C8, for example with STM32CubeIDE); the
  path is stored relative to the project. **Reload** uses a rebuilt file and
  restarts a running simulation.
- **F5** starts the firmware in SimNodus's own STM32 emulator at once.
- While running, every GPIO pin shows its state: red = output high, blue =
  output low, dotted outline = alternate function, hollow = input. The PC13
  LED on the board lights when PC13 is low; click **RESET** to restart the
  firmware.
- GPIO outputs drive the circuit like the real pins: push-pull outputs are
  3.3 V or 0 V behind about 40 Ω, open-drain outputs only pull low, inputs
  with pull-up or pull-down have 40 kΩ. An LED with a series resistor on an
  output lights as on the real board.
- Example: File > Examples > `07_Blue_Pill_blink` (PC13 blinks every 500 ms,
  PA0..PA3 count, the LED on PA0 blinks at 2 Hz; see V(PA0) and the LED
  current in the analyzer). Its source is in `examples/firmware/blink/`.
- GPIO inputs read the circuit: a voltage above 1.83 V reads 1, below
  1.16 V reads 0 (Schmitt trigger, as the STM32F103 datasheet guarantees).
  A push button from an input with internal pull-up to G works as on the
  board, and EXTI interrupts fire on its edges. The input border on the
  symbol turns red when the pin reads 1.
- Example: `08_Blue_Pill_button_interrupt` — every press of SW1 (PA1)
  toggles the LED on PA0 from the EXTI1 interrupt; PC13 lights while the
  button is held.
- Timing: circuit changes reach the firmware within 0.1 ms and output edges
  keep their exact time. Transient mode does not run microcontrollers yet.
- **Serial terminal**: Signal Analyzer, tab **UART**. Choose the port (USART1
  on PA9/PA10, USART2 on PA2/PA3, USART3 on PB10/PB11), read what the
  firmware prints, type a line and press Enter or Send (choose the line
  ending). **Time stamps** starts every line with its simulated time. Each
  byte goes out on the TX pin bit by bit at the baud rate (see
  [MICROCONTROLLER.md](MICROCONTROLLER.md)).
- Example: `09_Blue_Pill_serial_terminal` prints "tick N" every second;
  send `1` or `0` to switch the LED on PA0.
- Push buttons have a **Contact bounce** property: with *yes* their contacts
  bounce for about 3 ms after each press and release (a fixed pattern), as a
  real button does to a microcontroller input.
- Lessons: `10_Lesson1_GPIO_output` and `11_Lesson2_button_and_EXTI`, with
  notes in `examples/lessons/` (goals, steps, expected observations).
- **Logic lanes**: Signal Analyzer, tab **Logic**. Microcontroller pins
  appear as lanes by themselves when they change; check or uncheck any pin
  or voltage channel in the list (voltage channels read 1 above the **Probe
  threshold**). The lanes follow the scope's time base and cursors. Hover an
  edge for its exact time; click a lane to list its transitions in the
  window.

## Signal Analyzer

- **P** (Probe tool): click a wire for a voltage, a component pin for the
  current into that pin (Shift+click on a pin: its voltage). Alt+click a
  second net to measure the last voltage probe relative to it (differential).
  Probes are saved with the project. To move a probe, drag its marker (with
  the select tool) onto another wire or pin: it keeps its color and analyzer
  channel; a current probe needs a pin. Ctrl+Z undoes the move.
- Window > Signal Analyzer (Ctrl+Shift+A) can be moved to another monitor.
  Choose the time base, Roll or Triggered mode (source, level, slope; Auto,
  Normal or Single), per-channel scale, or Autoscale. The left edge of the
  scope shows the selected channel's volts per grid line and a colored
  triangle at each channel's 0 V; drag a triangle (or turn the wheel over the
  scale) to move that channel up or down, double-click it to put 0 V back in
  the middle.
- Cursors show Δt, 1/Δt and the values; the row below the plot shows min,
  max, mean, RMS, peak-to-peak, frequency and duty cycle of the selected
  channel.
- Transient results open as a complete record: wheel zooms the time axis,
  drag pans. Export CSV writes the visible channels (the first line says how
  the data was computed), Export PNG the plot.

## Files

Projects are `.snx` files (File > Save, Ctrl+S). Saving keeps the previous
version as `.snx.bak`. Unsaved changes are also written to a recovery file
every minute; if SimNodus closes unexpectedly, the next start offers to
restore them. File > Examples opens the bundled circuits as untitled
copies:

1. LED and push button
2. RC filter with a pulse generator
3. Potentiometer with voltmeter and ammeter
4. Transistor switch driving an LED
5. RC charging (transient mode)
6. Half-wave rectifier with smoothing capacitor

## Known limits of 0.1

- Device models are typical-value approximations for teaching, not
  datasheet matches. Open switches are 10^15 Ω, the voltmeter has a 1 GΩ
  input, the ammeter is ideal.
- Semiconductor junction charge is not carried across interactive segments;
  a tiny step may appear once per second in circuits that depend on it.
- The microcontroller simulates GPIO, EXTI, USART (whole bytes), SysTick and
  the clock tree; timers, ADC, SPI, I2C and DMA come later
  ([MICROCONTROLLER.md](MICROCONTROLLER.md)). No AC/FFT analysis.
- The user interface is in English; the Portuguese translation comes later.
