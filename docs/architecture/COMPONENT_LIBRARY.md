# Component library format

Revision 1 — 2026-10-14 (SN-111, PRJ-04). One JSON file per component type
under `resources/library/<category>/`. The loader is
[component_library](../../src/library/component_library.hpp); invalid files are
reported and skipped, the rest of the library stays usable. A future Symbol
Editor (PRJ-05) writes the same files.

## Coordinates

All coordinates are **grid units** relative to the symbol origin; one grid
unit is one canvas grid step. Pins, the designator and value offsets must be
on integer grid points; graphics may use fractions. The y axis points down.
Placed symbols are mirrored about their vertical axis first, then rotated
clockwise in 90° steps.

## Fields

| Field | Required | Meaning |
|---|---|---|
| `type` | yes | Stable key stored in projects (`resistor`) |
| `name`, `category`, `description` | name, category | Text shown in the component tree and preview |
| `prefix` | yes | Designator prefix (`R` gives R1, R2, ...) |
| `symbol.graphics` | — | Drawing primitives (below) |
| `symbol.states` | — | Extra graphics per control state (`"0"`, `"1"`), for switches and buttons |
| `symbol.pins` | yes | `{"name", "at": [x, y], "dir": "left|right|up|down"}`; names and positions unique |
| `designator_offset`, `value_offset` | — | Default text positions; default `[2, -1]` and `[2, 1]` |
| `show_designator` | — | `false` hides the designator (ground) |
| `value_parameter` | — | Parameter shown next to the symbol |
| `more_values` | — | Further parameters shown after it (`["temperature"]`, `["load"]`); a `%` unit is kept. While running, a parameter that an interactive potentiometer control sets shows the control's value (a solar panel's irradiance, a motor's load) |
| `parameters` | — | `{"name", "label", "unit", "default", "min", "max", "choices", "description", "kind", "filter"}`; `"hidden": true` keeps a parameter out of the Properties panel (it is still saved and set by `component.set`; a chart's reference data) |
| `spice` | unless `ties_net` or `ties_net_parameter` | SPICE template lines (below) |
| `models` | — | `.model` lines, emitted once per netlist |
| `ties_net` | — | All pins belong to this net; `"0"` makes the ground symbol |
| `lights` | — | Shapes that light with their own currents (display segments): `{"full": "10m", "color_parameter": "color", "segments": [{"element": "V_{ref}_sa", "graphic": 1}, ...]}`; each graphic (index in the symbol's graphics) is filled with the color at the brightness of the current through its measuring source |
| `pins[].optional` | — | `true`: the pin may stay unconnected without a warning (Q̅, clear inputs) |
| `ties_net_parameter` | — | All pins belong to the net named by this text parameter: the *Block port* part ([ADR 0098](../decisions/0098-user-blocks.md)) |
| `interactive` | — | `{"kind": "toggle|button|potentiometer|keypad", "parameter"}`: control operated while running. A keypad lists its keys' rectangles, `"keys": [[x, y, width, height], ...]`; its control is the number of the key held (1 for the first), 0 for none, and `symbol.states` may draw each held key (`"1"` to `"16"`). A potentiometer may add `"wiper": {"track": [x0, y0, x1, y1], "pin": "W"}`: the arrow's tip moves from (x0, y0) at 0 % to (x1, y1) at 100 %, joined to the wiper pin, and the percentage is shown next to it (UI-18). `"rate"` (0..1 range per simulated second) limits how fast the first control moves towards a new value, and `"detents"` makes it rest only on k / detents (SN-280: the encoder turns through every contact state of a detent). On a chip (a part with `device`), a potentiometer on a `live` parameter sends the value to the chip (SN-280) |
| `readout` | — | Live value: `{"kind": "voltage", "pins": ["+", "-"]}`, `{"kind": "current", "element": "V_{ref}"}` or `{"kind": "node", "element": "w_{ref}", "scale": 9.549, "unit": "rpm"}` (an internal node, scaled). A current readout may glow instead of showing text: `"glow": {"full": "10m", "color_parameter": "color"}` or `{"full_parameter": "I", "color": "#ffd27a"}` (full brightness at that current), or switch the symbol's state graphics: `"switch": {"on": "0.42 * 0.36 / Vcoil", "off": "0.32 * 0.36 / Vcoil"}` draws state `1` from the `on` current until it falls below `off` (the relay's contact; expressions of the parameters) |
| `live` | — | Lines of the Properties panel's live values: `[{"label": "I", "kind": "current", "element": "V_{ref}_i", "scale": -1, "unit": "A"}, {"label": "Load", "kind": "node", "element": "ctl_{ref}", "scale": 100, "unit": "%"}]`: a measuring source's current or an internal node (`ctl_{ref}` is the first control, 0..1), times the scale. The line labeled `I` is the part's current (into its first pin, or delivered by a source); with two pins the panel also gives V and P. Without `live`, only a current readout or a source's own branch gives a current |
| `chart` | — | Chart recorder (SN-281): `{"at": [x, y], "limits": [min_w, min_h, max_w, max_h], "channels": [{"plus": "CH1+", "minus": "CH1-", "reference_parameter": "ch1_reference"}, ...]}`. The plot's top-left corner is `at`; its size in grid units comes from the parameters `width` and `height` (or `width_parameter`, `height_parameter`) within the limits and is changed by dragging its corner. The editor records each channel's voltage (+ relative to -) over the whole run; a channel whose + pin is unconnected is off, an unconnected - pin is ground. The chart also reads the parameters `time_window`, `time_mode` (`fixed`, `rolling`), `time_unit` (`auto`, `s`, `min`, `h`), `vertical` (`auto`, `manual`), `v_min` and `v_max`, and draws each channel's reference parameter, "t v;t v;..." in seconds and volts. The part does not rotate |
| `mcu` | — | Microcontroller board (below) |
| `device` | — | Digital chip run in C++ ([ADR 0102](../decisions/0102-digital-devices-hybrid-coupling.md), [ADR 0103](../decisions/0103-chips-without-a-board.md)): `kind` (`74hc595`, `24c02`, `rom`, ...), `supply_pin`, `ground_pin`, `output_resistance`, `driven_pins`; memories add `memory_size` (bytes) or `memory_size_parameter` (a parameter with choices such as `2K`), `memory_fill` (the value of unwritten bytes, default 255) and `contents_parameter` (the file parameter with the starting contents) |

Parameter values are text in [engineering notation](../../src/model/engineering.hpp)
(`4k7`, `100n`, `1meg`; upper-case `M` is mega). A parameter with `choices`
takes one of the listed words instead (LED color, initial switch state).
`"kind": "file"` makes a file path (firmware) with an optional `filter`
(`"*.elf"`); it is stored relative to the project when possible and is not
used by the SPICE template. `"kind": "model"` with `"model_type"` (`NPN`,
`PNP`, `D`, `NMOS`, `PMOS`) names a SPICE model ([ADR 0095](../decisions/0095-model-variants.md));
the template uses it like any parameter (`Q_{ref} {node:C} {node:B} {node:E} {model}`)
and the netlist adds the chosen `.model` statement under a SimNodus name.
`"subcircuit": ["C", "B", "E"]` lists the part's pins in the usual port order
of vendor subcircuits; then a `.subckt` with as many ports may be chosen and
replaces the template ([ADR 0097](../decisions/0097-vendor-subcircuits.md)).
Without `model_type` (the op-amp) only subcircuits fit, and the default is the
part's own template. `"model_kinds": ["schottky"]` limits the offered models
to those whose file says `type=schottky` (LTspice diode libraries classify
models this way; `""` stands for models without `type=`). A parameter's
`"symbol"` is shown before its value on the schematic (`"β"` gives `β=100`).
`"kind": "text"` takes any words (a block port's name). A choice may turn the part:
`"orientations": {"right": {"rotation": 0, "mirrored": true}}` sets the orientation when that value
is chosen in Properties (a block port's side).

Vendor subcircuits placed with Place > Subcircuit (SN-214) are not files
either: type `subckt:<name>`, a box with one pin per port in DIP order and a
`model` parameter whose subcircuit text each component keeps
([ADR 0097](../decisions/0097-vendor-subcircuits.md)).

Blocks drawn by the user (PRJ-06) are not files of the library: a project's
blocks become definitions of type `block:<name>` with a rectangle and pins
made from the block's ports, in the category *Blocks*
([ADR 0098](../decisions/0098-user-blocks.md)).

### SPICE models

`*.lib` files of the library folder hold the built-in models as `.model`
statements; the comment line just above each one is its description
(`resources/library/models/basic_models.lib`). A model parameter's default
must be one of them. The user's model folders (Tools > Model Folders: any
number of folders, Documents/SimNodus/models by default; `SIMNODUS_MODELS_DIR`
for tests) are read the same way by the application; a model chosen
from it is copied into the component (`models` in the project file), so the
project does not need the folder. Only `.model` statements of the five types
are taken; subcircuits and other statements are skipped. Each parameter must
be a number and a parameter of its type, otherwise it is left out and reported
([ADR 0095](../decisions/0095-model-variants.md)); the built-in files load
without any report.

## Microcontroller boards

`mcu` turns a component into a board whose firmware runs in SimNodus's own
STM32F103 emulator ([ADR 0083](../decisions/0083-microcontroller-lockstep.md),
[ADR 0094](../decisions/0094-remove-renode.md)). The Renode-only fields
`engine`, `platform`, `extensions`, `before_firmware` and `after_firmware` are
no longer part of the format. Pins named like
`PA0` are GPIO pins (port letter and number); the others are power and
control pins. Unconnected board pins are not reported.

| Field | Meaning |
|---|---|
| `firmware_parameter` | The `file` parameter holding the ELF |
| `led` | `{"pin": "PC13", "active_low": true, "at": [x, y], "radius"}`: on-board LED drawn lit by the pin |
| `reset_button` | `[x, y, width, height]`: clicking it while running resets the microcontroller |
| `pin_model` | Electrical GPIO model: supply, output and pull resistance, input thresholds `input_high` / `input_low` |
| `ground_pin` | Reference of the pin voltages (default `G1`) |
| `uarts` | `[{"name": "usart1", "tx_pin": "PA9", "rx_pin": "PA10", "label": "USART1 (PA9 TX, PA10 RX)"}]`: serial ports for the terminal and their pins |

`pin_model` (`{"supply": 3.3, "output_resistance": 40, "pull_resistance":
40000}`) sets the electrical GPIO model ([ADR 0084](../decisions/0084-gpio-output-coupling.md)).
The template drives each GPIO pin with a behavioral current source
`I = V({ext:pa0_i}) - V({ext:pa0_g}) * (V(pin) - V(G))`; the simulation sets the
two external sources from the firmware's pin state. A 100 MΩ leakage keeps
every pin's DC path.

## Graphics

```json
{"line": [x1, y1, x2, y2]}
{"rect": [x, y, width, height]}
{"circle": [cx, cy, r]}
{"arc": [cx, cy, r, start_degrees, span_degrees]}
{"polyline": [x1, y1, x2, y2, ...]}
{"polygon": [x1, y1, ...], "fill": true}
{"text": "V", "at": [x, y], "size": 1.4}
```

Arc angles are counter-clockwise from 3 o'clock as seen on screen. All
lines use the common symbol line width; color is chosen by the application
(selection, live state), never by the library.

## SPICE templates

| Placeholder | Replaced by |
|---|---|
| `{ref}` | designator (`R1`) |
| `{node:NAME}` | net of pin `NAME` |
| `{NAME}` | parameter value as a plain number, or the chosen word |
| `{= EXPR}` | arithmetic on parameters: `+ - * /`, parentheses, `min()`, `max()`, `mod()`; `t0` is the start time of the current segment |
| `{control}` | node of the interactive control source (0/1 for toggles and buttons, 0–1 for a potentiometer) |
| `{control:NAME}` | node of the part's further control `NAME` (SN-272), listed in `interactive.controls` |

Element names start with the SPICE element letter and include `{ref}`
(`R_{ref}`), so they are unique. Interactive components are driven by an
external voltage source the netlist generator adds on `{control}` (ADR 0080).
A part may have several live controls (SN-272): `interactive.controls` lists
further toggles, buttons and potentiometers, each with a `name`, a `kind`, an
optional `parameter` (its starting value) and an optional `area` (x, y, width,
height in symbol coordinates) where a click works a toggle or button. Control
slot 0 is the first control, slot i + 1 is `controls[i]`; the session API,
the engine protocol (version 13) and `sim.control` (`"control": NAME`) address
them by slot. `interactive.stick` (SN-273) makes a potentiometer first control
and the potentiometer named by `y` move together when the symbol is dragged
inside `area` (left/bottom 0, right/top 1); `spring` (default true) and an
optional yes/no `spring_parameter` send both back to 0.5 on release. Shift
with the wheel turns the first further potentiometer.
Periodic sources use `t0` to keep their phase when an interactive run
restarts a segment (ngspice-47 accepts a negative PULSE delay; checked on
2026-10-14). The loader checks that every placeholder refers to an existing pin or
parameter.

## Component set A

`resources/library/` contains the 20 types of REQUIREMENTS SIM-06:
resistor, capacitor, inductor, potentiometer, DC voltage and current
sources, battery, pulse and sine generators, ground, SPST and SPDT switches,
NO and NC push buttons, diode (1N4148-like), LED (five colors), NPN
(2N2222-like), N-MOSFET (2N7000-like), voltmeter and ammeter. Device models
are typical-value approximations for teaching; they are not datasheet
matches.

`resources/library/mcu/bluepill.json` adds the Blue Pill (STM32F103C8) with
the 40 header pins in board order, the PC13 LED and the reset button.
