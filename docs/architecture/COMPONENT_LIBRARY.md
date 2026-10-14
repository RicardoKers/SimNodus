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
| `parameters` | — | `{"name", "label", "unit", "default", "min", "max", "choices", "description"}` |
| `spice` | unless `ties_net` | SPICE template lines (below) |
| `models` | — | `.model` lines, emitted once per netlist |
| `ties_net` | — | All pins belong to this net; `"0"` makes the ground symbol |
| `interactive` | — | `{"kind": "toggle|button|potentiometer", "parameter"}`: control operated while running |
| `readout` | — | Live value: `{"kind": "voltage", "pins": ["+", "-"]}` or `{"kind": "current", "element": "V_{ref}"}` |

Parameter values are text in [engineering notation](../../src/model/engineering.hpp)
(`4k7`, `100n`, `1meg`; upper-case `M` is mega). A parameter with `choices`
takes one of the listed words instead (LED color, initial switch state).

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

Element names start with the SPICE element letter and include `{ref}`
(`R_{ref}`), so they are unique. Interactive components are driven by an
external voltage source the netlist generator adds on `{control}` (ADR 0080).
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
