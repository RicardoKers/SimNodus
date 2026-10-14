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

## Drawing a circuit

| Action | How |
|---|---|
| Place a part | Choose it in the tree; click to place, repeat, Esc ends. Ctrl+R rotates, Ctrl+E mirrors before placing. Shortcuts: R, C, L, D, G (ground) |
| Wire | W, then click a pin to start, click to add bends, click a pin or wire to finish. Space changes the corner. Double-click ends at an open point. Esc cancels |
| Net label | N, then click a wire or pin. Labels with the same name are connected; GND or 0 is ground |
| Select and move | Click, Ctrl+click, or drag a rectangle; drag to move (wires stay attached), arrow keys move one grid step |
| Edit | Ctrl+R rotate, Ctrl+E mirror, Del delete, Ctrl+C / Ctrl+X / Ctrl+V, Ctrl+Z / Ctrl+Y undo and redo |
| Texts | Drag a part's name or value text to move it |

A red circle marks a pin or wire end that is not connected; a dot marks a
junction. Every circuit needs a ground symbol.

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
  and **transient** (fixed time span, complete result) mode.

Interactive runs are computed in 1 s segments with a 100 µs maximum time
step by default; edges of switches and generators are resolved to that step.

## Signal Analyzer

- **P** (Probe tool): click a wire for a voltage, a component pin for the
  current into that pin (Shift+click on a pin: its voltage). Probes are saved
  with the project.
- Window > Signal Analyzer (Ctrl+Shift+A) can be moved to another monitor.
  Choose the time base, Roll or Triggered mode (source, level, slope; Auto,
  Normal or Single), per-channel scale, or Autoscale.
- Cursors show Δt, 1/Δt and the values; the row below the plot shows min,
  max, mean, RMS, peak-to-peak, frequency and duty cycle of the selected
  channel.
- Transient results open as a complete record: wheel zooms the time axis,
  drag pans. Export CSV writes the visible channels, Export PNG the plot.

## Files

Projects are `.snx` files (File > Save, Ctrl+S). Saving keeps the previous
version as `.snx.bak`. File > Examples opens the bundled circuits as untitled
copies:

1. LED and push button
2. RC filter with a pulse generator
3. Potentiometer with voltmeter and ammeter
4. Transistor switch driving an LED
5. RC charging (transient mode)
6. Half-wave rectifier with smoothing capacitor

## Known limits of 0.1

- Device models are typical-value approximations for teaching, not
  datasheet matches.
- Semiconductor junction charge is not carried across interactive segments;
  a tiny step may appear once per second in circuits that depend on it.
- No microcontroller yet (release 0.2), no AC/FFT analysis.
- The user interface is in English; the Portuguese translation comes later.
