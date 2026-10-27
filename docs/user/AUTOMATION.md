# Automation: SimNodus for AI agents and scripts

`simnodus-cli` (next to `simnodus.exe`) runs SimNodus without a window. AI
coding agents such as Claude Code or Codex, scripts and CI jobs use it to build
circuits, check them, simulate them and read measurements. Every result is
JSON. It uses the same parts, checks and simulation as the desktop
application (design decision: ADR 0104 in the source repository).

The same commands can also control the open SimNodus window, so that an agent
builds a circuit step by step on screen, for example in a recorded video
tutorial: see [Controlling the window](#controlling-the-window).

It does not open a network port. It reads and writes only the files you name.
Opening a project never runs firmware: a Blue Pill's firmware runs only when
a simulation is started.

## Quick start

```text
simnodus-cli parts capacitor                 list parts whose text mentions "capacitor"
simnodus-cli describe resistor               pins, parameters and size of a part
simnodus-cli inspect examples/05_RC_charging_transient.snx
simnodus-cli check   examples/05_RC_charging_transient.snx
simnodus-cli run     examples/01_LED_and_push_button.snx --time 20m --event "10m SW1 1"
simnodus-cli exec    build_circuit.jsonl     run a file of JSON requests
simnodus-cli serve                           JSON requests on standard input
simnodus-cli commands                        every JSON command with its arguments
simnodus-cli connect [script.jsonl]          send requests to the SimNodus window
simnodus-cli mcp [--window]                  MCP server for AI agents (see below)
```

Add `--pretty` to indent the output. Exit codes: 0 done, 1 a command failed
(or `check` found circuit errors), 2 wrong usage, 3 no component library,
4 no SimNodus window accepts external control.
Whatever ngspice or the emulator prints goes to standard error, so standard
output carries only JSON.

To let an agent use it, register it as an MCP server (see
[MCP server](#mcp-server)), or tell the agent where the program is and to start with
`simnodus-cli commands`. For example: *"Use `simnodus-cli.exe` in the SimNodus
folder to build an RC low-pass filter with a 1 kHz cutoff, check it, simulate it and
save it as `rc.snx`. Run `simnodus-cli commands` first."*

## Requests and replies

`exec` and `serve` take one JSON object per line; lines starting with `#` are
comments. A request names a command and its arguments; `id` is optional and
comes back unchanged:

```json
{"id": 1, "command": "component.add", "args": {"type": "resistor", "at": [6, -6], "rotation": 270, "parameters": {"R": "1k"}}}
```

```json
{"id": 1, "ok": true, "result": {"designator": "R1", "id": 2, "pins": [{"name": "1", "at": [3, -6]}, {"name": "2", "at": [9, -6]}], "...": "..."}}
{"id": 2, "ok": false, "error": {"code": "not-found", "message": "R1 has no pin \"3\" (pins: 1, 2)."}}
```

Error codes are stable: `bad-request`, `unknown-command`, `invalid-argument`,
`not-found`, `blocked`, `io-error`, `circuit-errors`, `not-running`,
`run-failed`, `timeout`, `cancelled`, `internal`, and `no-window` from
`connect`. Messages are for people and may change.

`serve` keeps the project and the simulation between requests, so an agent can
start a run, operate switches and read values step by step. `exec` stops at the
first failed request unless `--keep-going` is given. Both start with an empty
project, or with the project named after the script.

## Conventions

- **Grid points** are integers `[x, y]`; y grows downwards; one unit is the
  canvas grid step. `parts.describe` gives each pin's point relative to the
  part's origin and the part's bounds, so parts can be placed without overlap.
- **Components** are named by designator (`"R1"`) or numeric ID. **Pins** are
  `DESIGNATOR.PIN`: `"R1.2"`, `"V1.+"`, `"U1.PA0"`. Wherever a point is
  expected, a pin may be given instead.
- **Quantities** are numbers or engineering text: `"4.7k"`, `"10m"`,
  `"10ms"`, `"100u"`.
- **Signals** are `V(NET)`, `V(R1.2)` (the net at a pin), `V(A,B)` (A minus B)
  and `I(R1.1)` (the current into a pin). Without `signals`, a run measures
  the project's probes, or every named net when there are none.

## Commands

| Command | What it does |
|---|---|
| `hello`, `help` | Protocol version and whether the window is controlled (`directory` sets where relative paths start); every command with its arguments |
| `parts.list` | Parts, filtered by `search` or `category` |
| `parts.describe` | Pins, parameters (units, limits, choices, built-in models), bounds |
| `project.new`, `project.open`, `project.save`, `project.info` | Project files (`save` keeps the previous version as `.snx.bak`; with `"with_files": true` it also copies firmware and memory contents files into `<name>_files` next to the project; in the window, `new` and `open` refuse to drop unsaved changes unless `"discard": true`) |
| `circuit.inspect` | Components with pins, points and nets; wires, labels, nets, probes, notes, unconnected pins, settings |
| `circuit.check` | The pre-run checks of the Messages panel, with codes |
| `component.add` | Places a part `at` a point, or with a pin `attach`ed to a pin or point (`{"to": "V1.-"}` for a ground) |
| `component.set` | Parameters (checked against the part), designator, rotation, mirroring |
| `items.move`, `items.delete` | Move or delete components and other elements; attached wires follow |
| `wire.add` | An orthogonal wire through points |
| `wire.connect` | Wires two pins or points with an L-shaped route; refuses (`blocked`) a route that would touch another pin or wire; `via` adds corner points |
| `label.add` | Net label; equal names connect, `GND` or `0` is ground |
| `probe.add`, `note.add` | Probes for the Signal Analyzer, text notes |
| `settings.get`, `settings.set` | Mode, transient stop and step, interactive step, segment, speed, `exact_edges`, `cortex_m3_timing` |
| `edit.undo`, `edit.redo` | Undo history of the session |
| `sim.run` | Start, run, measure, stop: a transient analysis, or an interactive run for `duration` with timed `events` |
| `sim.start`, `sim.advance`, `sim.control`, `sim.read`, `sim.status`, `sim.pause`, `sim.stop` | A run step by step (a sensor's control also takes its quantity with the unit: `"35 °C"`, `"500 lux"`) |
| `sim.uart` | Text a board sent on a serial port; `send` types into it |

`sim.read` and `sim.run` return, for each signal, the last value and the
minimum, maximum, mean, RMS, peak-to-peak, frequency and duty cycle in the
window (`from`, `to`). `points` adds that many evenly spaced values, and `csv`
writes every sample to a file. Character displays (an HD44780 LCD) come back
under `displays`, and the boards' messages under `messages`.

## Example: an RC low-pass filter from nothing

```json
{"command": "component.add", "args": {"type": "vpulse", "at": [0, 0], "parameters": {"f": "100"}}}
{"command": "component.add", "args": {"type": "resistor", "at": [6, -6], "rotation": 270, "parameters": {"R": "1k"}}}
{"command": "component.add", "args": {"type": "capacitor", "at": [12, 0], "parameters": {"C": "1u"}}}
{"command": "component.add", "args": {"type": "ground", "attach": {"to": "V1.-"}}}
{"command": "wire.connect", "args": {"from": "V1.+", "to": "R1.1"}}
{"command": "wire.connect", "args": {"from": "R1.2", "to": "C1.1"}}
{"command": "wire.connect", "args": {"from": "C1.2", "to": "V1.-"}}
{"command": "label.add", "args": {"name": "OUT", "at": "C1.1"}}
{"command": "circuit.check"}
{"command": "sim.run", "args": {"mode": "transient", "duration": "40m", "signals": ["V(OUT)", "I(R1.1)"], "points": 9}}
{"command": "project.save", "args": {"path": "rc.snx"}}
```

The run reports V(OUT) at 100 Hz with a mean of 2.5 V. Open `rc.snx` in
SimNodus to see the circuit.

## Interactive runs and timing

- An interactive run without a window goes as fast as the computer allows
  (`speed` 0) unless `speed` is given.
- `sim.advance` and timed `events` stop the run at the first accepted time
  point at or after the asked time. With the default 100 µs step a button
  pressed "at 10 ms" acts at 10.0–10.1 ms; the reply says when (`applied_at`).
- Interactive runs are approximate in the same way as in the window:
  segments, the maximum step, step-averaged microcontroller edges unless
  *exact edges* is on. Transient runs are one deterministic analysis and do
  not run microcontrollers.
- Samples older than `history` (100 000 per net by default) are dropped in long
  interactive runs; a signal then reports `history_from`.

## Controlling the window

The SimNodus window accepts the same commands when its external control is
on: **Tools > Allow External Control**, or start it with
`simnodus.exe --control`. It is off by default and not remembered, so every
session starts without it. The status bar shows *External control on*, and
*Controlled by an agent* while a program is connected. Only programs of the
same Windows user on the same computer can connect (a named pipe; a local
socket on Linux); nothing is opened to the network.

`simnodus-cli connect` passes requests to the window: from a file
(`connect build.jsonl`, stopping at the first failure unless `--keep-going`)
or, without a file, from standard input, one line per request and one reply
line each, as `serve` does. Relative paths start from the folder where
`connect` runs.

In the window:

- Every edit is one step of **Edit > Undo**, is selected on the canvas and
  scrolled into view. A change of the circuit's structure stops a running
  simulation, as it does when you edit by hand; a parameter change goes to
  the running simulation where the part allows it.
- Runs are the window's own: the canvas animates, the Signal Analyzer shows
  the probes, and runs keep the project's speed (real time by default)
  instead of going as fast as possible. A run you start with Run (F5)
  replaces the agent's run.
- `project.new` and `project.open` refuse (`blocked`) to drop unsaved changes
  unless the request says `"discard": true`.
- Closing the window or turning external control off ends a waiting command
  with `cancelled`.

Commands of the window only:

| Command | What it does |
|---|---|
| `view.pace` | `{"ms": 600}`: wait after each edit, so that a recording can follow; 0 (default) for none |
| `view.caption` | `{"text": "Now the capacitor", "seconds": 5}`: a caption at the bottom of the canvas; empty text hides it; without `seconds` it stays until replaced; `"position": "top"` (or `center`, `bottom`, a corner such as `top-left`) and `"size": 1.5` place and enlarge it (YouTube covers the bottom of Shorts) |
| `view.keys` | `{"keys": "Ctrl+R", "seconds": 1.5, "position": "top-right", "size": 1}`: a key badge on the canvas, like a keystroke overlay |
| `view.pointer` | `{"to": "R1.2", "ms": 600, "click": 1}`: a drawn pointer glides to a target and shows a click (2: a double-click); nothing is clicked; `{"hide": true}` hides it |
| `view.highlight` | `{"target": {"widget": "components"}, "seconds": 2}`: a pulsing outline around a target, without selecting it (`0`: until replaced); `{"hide": true}` |
| `view.locate` | `{"targets": ["R1.2", {"widget": "canvas"}]}`: screen rectangles and points of targets in logical pixels and in the screen's physical pixels (`physical_rect`, `physical_point`, `scale`: 1.25 at 125 %) |
| `ui.move` | `{"to": [30, 10], "ms": 600}`: the pointer glides there and the real mouse moves over the widget (a part being placed follows it) |
| `ui.click` | `{"target": {"action": "Wire"}, "ms": 600, "count": 1, "button": "left"}`: the pointer glides there and clicks through the real widget: a toolbar button, a Components entry (`"count": 2` starts placing it), a menu or one of its entries, a tab, a point of the canvas (drops a part, starts or ends a wire) |
| `ui.drag` | `{"from": "R1", "to": [20, 10], "ms": 900}`: press, glide with the button held, release |
| `ui.key` | `{"keys": "R"}`: keys as a hand presses them, with a key badge (`"show": false` without): a menu shortcut runs its command (R places a resistor, W draws a wire, Ctrl+R rotates), others go to the focused widget (Escape ends a tool) |
| `ui.type` | `{"target": {"widget": "search"}, "text": "capacitor", "ms": 90, "enter": false}`: the pointer clicks the field and types letter by letter; `{"field": "Resistance"}` with `"enter": true` sets a value in Properties |
| `view.show` | `{"items": ["R1", 12]}`: select components or elements and scroll them into view; `"ms": 800` pans smoothly |
| `view.fit` | Zoom to show the whole circuit; `{"area": [x1, y1, x2, y2]}` frames a grid area instead (a circuit's place before it is built), `{"region": [0, 0, 0.55, 1]}` uses only part of the canvas (here the left 55%, leaving room for the Signal Analyzer); `"ms": 1200` zooms and pans smoothly over that time instead of jumping |
| `view.panels` | `{"components": false, "properties": true, "messages": false}` shows or hides the panels (Ctrl+1/2/3); `{"focus": true}` leaves the canvas alone (F11) |
| `view.font` | `{"scale": 1.4}`: larger interface text for a recording; 1 is the normal size. `simnodus.exe --scale 1.5` enlarges everything, icons included |
| `view.dialog` | `{"name": "shortcuts"}` (or `about`) opens a read-only dialog for a tour; `{"show": false}` closes it |
| `view.wait` | `{"ms": 1500}`: wait while the window stays live (narration) |
| `view.analyzer` | `{"show": true}`: show or hide the Signal Analyzer |
| `analyzer.set` | Change the Signal Analyzer's settings (all optional; see below) and answer with all of them |
| `analyzer.autoscale` | Autoscale: every channel's scale fitted and made Auto again |

`analyzer.set` takes:

| Argument | Values |
|---|---|
| `tab` | `scope`, `logic` or `uart` |
| `timebase` | Seconds per division, `"2m"` or `0.002` (the nearest step) |
| `mode` | `roll` or `triggered` |
| `trigger` | `{"source": "V(OUT)", "level": 2.5, "slope": "rising" or "falling", "sweep": "auto", "normal" or "single"}`, any part |
| `cursors` | `true`, `false`, or `[a, b]` in seconds (turns them on there) |
| `select` | The channel whose measurements show below the plot |
| `channels` | `{"V(OUT)": {"visible": true, "scale": 2, "offset": 0}}`: per channel; `scale` in V/div or A/div, or `"auto"`; `offset` is the value at the center line |
| `lanes` | `["U1.PA5", "V(OUT)"]`: exactly these lanes in the Logic tab, voltage probes too; board pins no longer appear by themselves; `"auto"` goes back to that |
| `threshold` | The Logic tab's probe threshold in volts |
| `panel` | `false` hides the side panel (channels, timebase, trigger) so that the plot fills the window |
| `display` | `stop` freezes the display (Stop display), `run` lets it follow again |
| `window` | `[from, to]` in seconds: the time window, while the run is paused, the display is stopped or a transient record is shown (`blocked` otherwise) |

Channels are named as in the analyzer (`V(OUT)`, `I(R1.1)`) and exist once a
run with probes has started; the reply lists them with their scales, the
lanes shown and the lanes available. A wrong argument changes nothing.

A tutorial script might start with `view.pace` 800, frame the circuit's future
place with `view.fit` and an `area`, give each step a caption,
place the parts, wire them, add probes, open the analyzer and run with
`"keep": true` so the last state stays on screen.

**Targets** of `view.pointer`, `view.highlight`, `view.locate` and `ui.*`:

| Target | Where |
|---|---|
| `[30, 10]` | A grid point of the canvas |
| `"R1.2"`, `"R1"` | A pin, a part |
| `{"area": [x1, y1, x2, y2]}` | A grid area of the canvas |
| `{"widget": "components"}` | `components`, `search`, `tree`, `properties`, `messages`, `canvas`, `toolbar`, `statusbar`, `menubar`, `window`, `analyzer`, `scope`, `logic`, `analyzer-panel` |
| `{"action": "Wire"}` | A toolbar button by its command's name (the main window's or the analyzer's: `Run`, `Probe`, `Autoscale`) |
| `{"part": "Resistor"}` | An entry of the Components tree (as shown: a search may hide it) |
| `{"field": "Resistance"}` | A field of Properties, by its label (select the part first) |
| `{"menu": "Simulate"}`, `{"menu": "Simulate", "item": "Settings"}` | A menu of the menu bar; an entry of an open menu |
| `{"tab": "Logic"}` | A tab of the Signal Analyzer |
| `{"name": "speedFactor"}` | Any widget by its object name |

The pointer and the outline are small windows above every window, so a
recording of the screen shows them over menus and the Signal Analyzer; a
recording of one window does not. A hand-drawn tutorial then reads like a
person using SimNodus:

```text
{"command": "view.highlight", "args": {"target": {"widget": "components"}, "seconds": 2}}
{"command": "ui.type", "args": {"target": {"widget": "search"}, "text": "resistor"}}
{"command": "ui.click", "args": {"target": {"part": "Resistor"}, "count": 2}}
{"command": "ui.move", "args": {"to": [10, 0]}}
{"command": "ui.click", "args": {"target": [10, 0]}}
{"command": "ui.key", "args": {"keys": "W"}}
{"command": "ui.click", "args": {"target": "R1.2"}}
{"command": "ui.click", "args": {"target": "C1.1"}}
```

## MCP server

`simnodus-cli mcp` is a Model Context Protocol server on standard input and
output, so agents that support MCP (Claude Code, Codex and others) see every
command as a tool, with its description and arguments. Tool names use `_`
for `.`: `component_add`, `wire_connect`, `sim_run`. A call returns the
command's JSON result as text, marked as an error when the command failed
(the text then holds `code` and `message`).

`simnodus-cli mcp` works without a window, like `serve`.
`simnodus-cli mcp --window` sends the calls to the SimNodus window instead
(turn on Tools > Allow External Control, or start SimNodus with `--control`),
and adds the `view_*` tools. The window may be started after the agent: each
call connects when needed. Relative paths start from the folder the agent
starts the server in.

Claude Code (use the folder where SimNodus is installed):

```text
claude mcp add simnodus -- "C:\path\to\SimNodus\simnodus-cli.exe" mcp --window
```

Codex, in `~/.codex/config.toml`:

```toml
[mcp_servers.simnodus]
command = "C:\\path\\to\\SimNodus\\simnodus-cli.exe"
args = ["mcp", "--window"]
```

Leave out `--window` to simulate without the window.
