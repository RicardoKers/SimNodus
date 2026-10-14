# Desktop user interface

Revision 2 — 2026-10-14. Owner direction: a clean QSpice-like editor with the
component tree on the left, the schematic in the middle and properties on the
right, continuous interactive simulation, and signal analysis in a separate
window. Requirements are numbered in [REQUIREMENTS](../REQUIREMENTS.md); this
document gives the design detail. The first UX direction is archived in
[archive/2026-10-legacy/DESKTOP_UX.md](../archive/2026-10-legacy/DESKTOP_UX.md);
its separate analyzer window, contextual probes and shared measurement
definitions are retained.

## Windows

| Window | Purpose |
|---|---|
| Circuit Editor (main window) | Build and edit the circuit, run it, interact with controls, see live values |
| Signal Analyzer | Oscilloscope, logic analyzer, UART terminal, cursors, measurements, export |
| Symbol Editor (future, PRJ-05) | Create and edit component symbols and pin layouts |

The editor and analyzer share one simulation session. Each can be on a different
monitor. Closing the analyzer only hides the view; acquisition continues.

## Circuit Editor layout

- **Left — Components.** Search field on top; tree of categories (Passive,
  Sources, Switches & Controls, Semiconductors, Instruments, Microcontrollers,
  Logic). Under the tree a small **Preview** with the symbol, pin count and a
  one-line description. Enter or double-click starts placement; dragging onto the
  canvas places directly.
- **Center — Schematic canvas.** Near-white background, dot grid (major grid
  every 10 minor points, minor points only drawn when zoomed in enough). Symbols
  in dark gray/black with uniform line weight; selected items in blue; probes in
  their trace color. No decorative gradients or shadows.
- **Right — Properties.** Header with designator and component type; editable
  parameter grid (name, value, unit); while running, a **Live values** section
  below it.
- **Bottom — Messages** (collapsed by default, opens automatically on errors).
- **Status bar** — state, simulated time, achieved speed, mode, warnings.

Side panels are `QSplitter` children (or docks restricted to left/right) whose
widths the user drags. Collapse/expand: `Ctrl+1` (components), `Ctrl+2`
(properties), `Ctrl+3` (messages); the previous width is restored. A *Focus
canvas* action (`F11`) hides all panels temporarily and restores them.

## Canvas interaction

| Action | Interaction |
|---|---|
| Select | Click; rubber band from empty area; Ctrl+click toggles |
| Move | Drag selection; arrow keys move one grid step (Shift: 10 steps); wires stay attached and stay orthogonal |
| Place | Ghost follows cursor; `Ctrl+R` rotate 90°, `Ctrl+E` mirror; click to place; continues until `Esc` |
| Wire | `W` or toolbar; click pin/wire/empty point to start; each click adds an orthogonal bend; `Space` toggles horizontal/vertical first; click a pin or wire to finish; double-click ends at an open point; `Esc` cancels |
| Junctions | Automatic dot when three or more connections meet; T connection when a wire ends on another wire; red circle on unconnected pin/wire ends |
| Labels | Net label tool (`N`); same name means same net; ground symbol is net `0` |
| Edit value | Double-click a component opens its value field in Properties; double-click its value text edits it in place |
| Zoom/pan | Wheel at cursor; middle-drag or Space+drag; `F` or Home fits the circuit |
| Probe | `P` or toolbar; click net = voltage probe, click component terminal = current probe; Alt+click second net = differential voltage |
| Context menu | Rotate, mirror, delete, properties, add probe, open in analyzer |

During **Run** the canvas switches to *interactive* behavior: clicks on
interactive controls operate them instead of selecting, the rest of the circuit
is still selectable for inspection, and structural editing asks to stop first.

### Interactive controls and live rendering

| Component | Interaction while running | Live rendering |
|---|---|---|
| Toggle switch (SPST/SPDT) | Click toggles | Lever position |
| Push button (NO/NC) | Pressed while mouse is held; keyboard binding optional | Pressed cap |
| Potentiometer | Drag the wiper or use the wheel over it; position 0–100 % | Wiper position and % |
| Adjustable source | Wheel or value field | Present value |
| LED | — | Brightness and color proportional to forward current |
| Lamp | — | Glow from power |
| Voltmeter / ammeter | — | Digital readout on the symbol |
| Wires (optional) | — | Color by voltage (View → Show voltages) |
| MCU pins (phase 2) | Reset button and BOOT jumpers clickable | Pin state color: high/low/input/analog |

## Signal Analyzer

```text
+--------------------------------------------------------------------------+
| Signal Analyzer                                   [Run/Stop display] [x] |
+------------------+-------------------------------------------------------+
| CHANNELS         |  [Scope] [Logic] [UART]                               |
| [x] CH1 V(out)   |                                                       |
|     1 V/div  0 V |          waveform area (grid, traces, cursors)        |
| [x] CH2 V(in)    |                                                       |
| [x] CH3 I(R1)    |                                                       |
| TIMEBASE         |-------------------------------------------------------|
|  1 ms/div  Roll  |  Cursor A 1.200 ms  B 2.350 ms  dt 1.150 ms  869.6 Hz |
| TRIGGER          |  CH1: min 0.00 max 3.30 mean 1.64 RMS 1.89 f 1.000 kHz |
|  CH1 / 1.65 V    |                                                       |
+------------------+-------------------------------------------------------+
```

- Left column: channel list (visibility, color, scale, offset, remove),
  time base, trigger and acquisition settings.
- Tabs: **Scope** (analog), **Logic** (digital lanes, phase 2), **UART**
  (terminal, phase 2). Scope and Logic share the time axis and cursors.
- Interactive runs default to roll mode; with a trigger the display behaves like
  a bench oscilloscope (auto/normal/single). Transient results open as a
  complete record with zoom and pan.
- Measurement row for the selected channel; two draggable cursors.
- Export: CSV of visible channels in the current time window; PNG of the plot.

## Measurement identity

Probes refer to stable IDs (net ID with explicit reference, or component
terminal with explicit orientation), not to labels or ngspice names. Renaming a
component or net keeps the probe. Deleting the target marks the probe invalid
with a message instead of rebinding by name. The same probe definition feeds the
canvas readout, the analyzer and CSV export.

## Keyboard shortcuts

| Shortcut | Action |
|---|---|
| `Ctrl+N` / `Ctrl+O` / `Ctrl+S` / `Ctrl+Shift+S` | New / Open / Save / Save As |
| `Ctrl+Z` / `Ctrl+Y` | Undo / Redo |
| `Ctrl+X` / `Ctrl+C` / `Ctrl+V` / `Del` | Cut / Copy / Paste / Delete |
| `Ctrl+R` / `Ctrl+E` | Rotate / Mirror |
| `W` / `N` / `G` / `P` | Wire / Net label / Ground / Probe |
| `R` / `C` / `L` / `D` | Quick place resistor / capacitor / inductor / diode |
| `F` or `Home` | Fit circuit |
| `F5` / `F6` / `Shift+F5` | Run / Pause-resume / Stop |
| `Ctrl+Shift+A` | Show Signal Analyzer |
| `Ctrl+1` / `Ctrl+2` / `Ctrl+3` / `F11` | Toggle components / properties / messages / focus canvas |
| `Esc` | Cancel current tool, return to selection |

## Symbols

Symbols are data (see [PRODUCT_ARCHITECTURE](PRODUCT_ARCHITECTURE.md#component-library)):
lines, arcs, circles, rectangles, polylines, text and pins with names and
electrical direction, on the same grid as the canvas. IEC-style resistor
(rectangle) is the default; an ANSI zigzag variant may be offered later as a
preference. The future Symbol Editor edits these files directly.

## Language

The UI base language is English; all strings use `tr()` and a pt-BR translation
is shipped for the classroom release. Component names in the library are
translatable metadata; reference designators stay international (R, C, L, D, Q,
U, SW, LED).
