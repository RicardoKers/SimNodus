# Product architecture (rebuild)

Revision 1 — 2026-10-14, under
[ADR 0079](../decisions/0079-product-rebuild-and-lightweight-process.md).
It describes the application being built now. Backend findings from E-01..E-05
remain authoritative for engine behavior and are referenced, not repeated.

## Overview

```text
+----------------------------- apps/simnodus (Qt Widgets) ----------------------------+
|  MainWindow: ComponentTree | SchematicView (QGraphicsView) | PropertiesPanel        |
|  AnalyzerWindow: Scope / Logic / UART           Messages, StatusBar                 |
|  UndoStack (QUndoStack) of document commands                                        |
+-------------------+---------------------------------------+-------------------------+
                    | document edits                        | run / controls / probes
+-------------------v---------------+       +---------------v-------------------------+
| src/model   Schematic document    |       | src/sim   SimulationSession              |
|  components, pins, wires, labels, |       |  - InteractiveRun (paced, unbounded)    |
|  probes, settings, stable IDs     |       |  - TransientRun (deterministic .tran)   |
| src/io      .snx JSON read/write  |       |  - control inputs (external sources)    |
| src/library component definitions |       |  - acquisition ring buffers             |
| src/netlist connectivity + checks |------>|  NgspiceEngine (shared lib, runtime load)|
|             SPICE generation      |       |  NativeMcuRunner (src/emu) + Coupler    |
+-----------------------------------+       +-----------------------------------------+
```

Rules:

- `src/model`, `src/library`, `src/netlist`, `src/io` and `src/sim` are plain
  C++20 with no Qt dependency, so they run in tests, a future CLI and the engine
  process. JSON uses the vendored single-header nlohmann/json (MIT).
- `apps/simnodus` contains all Qt code. Qt Widgets only (ADR 0081).
- No GUI code calls ngspice or the microcontroller emulator directly; it talks to `SimulationSession`
  through a thread-safe command queue and reads snapshots of acquired data.
- Backends are loaded at run time (`LoadLibrary`/`dlopen`), never when a project
  is opened.

## Document model (`src/model`)

- `Schematic`: list of `ComponentInstance`, `Wire`, `NetLabel`, `Probe`, sheet
  settings and simulation settings.
- Every element has a stable 64-bit ID that survives save/load and rename.
- `ComponentInstance`: library type key, designator, position (grid units),
  rotation (0/90/180/270), mirror flag, parameter map (string values with
  engineering notation), label offsets.
- `Wire`: polyline of grid points. Connectivity is geometric: a wire end or
  vertex touching a pin or another wire connects; junctions are derived.
- Editing happens through command objects (add, delete, move, rotate, set
  parameter, add wire, split wire...) applied by the app's `QUndoStack`.

## Component library

One JSON file per component type under `resources/library/<category>/`.

```json
{
  "type": "resistor",
  "name": "Resistor", "category": "Passive", "prefix": "R",
  "description": "Linear resistor",
  "symbol": { "graphics": [ {"rect": [-1, -4, 2, 8]} ],
              "pins": [ {"name": "1", "at": [0, -6], "dir": "up"},
                        {"name": "2", "at": [0,  6], "dir": "down"} ] },
  "parameters": [ {"name": "R", "unit": "Ohm", "default": "1k", "min": "1m"} ],
  "spice": "R{ref} {1} {2} {R}",
  "interactive": null
}
```

- `spice` is a template: `{ref}` designator, `{pin}` net names, `{param}` values.
  Components that need several elements or `.model` lines use a list.
- `interactive` declares controls (switch state, button, potentiometer
  position) that map to external sources at run time.
- The future Symbol Editor writes the `symbol` section; nothing in C++ is
  specific to one component except behaviors that cannot be expressed in data
  (MCU, instruments).

## Netlist generation (`src/netlist`)

1. Union-find over pins, wire points and labels gives nets; ground symbols and
   label `0`/`GND` map to SPICE node `0`.
2. Checks (SIM-05) produce diagnostics referencing entity IDs.
3. Each instance expands its template. Interactive controls become sources with
   the ngspice `external` option driving switches (`S` elements with a
   voltage-controlled switch model) or behavioral elements.
4. A name map links SPICE vectors back to net/terminal IDs for probes.

Golden-file tests cover the generated netlists; simulation tests compare
results with analytical references (RC, RL, divider, diode clamp).

## Simulation session (`src/sim`)

- **NgspiceEngine** wraps the shared library: initialization callbacks,
  `ngSpice_Circ`, background run, `GetVSRCData` for external sources and
  `SendData` for accepted time points. One engine instance per run thread.
- **InteractiveRun** (ADR 0080): transient with a very long stop time, the run
  thread paced against wall clock × speed factor inside the data callback,
  control changes read from an atomic input table, accepted points pushed to
  per-probe ring buffers. Memory is bounded by segmented runs (state carried by
  initial conditions) unless the spike finds a better option.
- **TransientRun**: plain `.tran`, all requested vectors stored, then handed to
  the analyzer as one dataset.
- **Acquisition**: ring buffers per channel with min/max decimation for display;
  analog samples are irregular in time (SPICE steps) and are stored with their
  timestamps.
- **McuBackend** (`src/mcu/mcu_backend.hpp`, SN-171): the engine-independent
  interface the session uses for a running board (start, time limit and wait,
  inputs, UART, pin states and drives, logic logs), with times in virtual
  nanoseconds and pins named as on the board. `NativeMcuRunner` implements it.
- **NativeMcuRunner** (`src/mcu`, [ADR 0090](../decisions/0090-own-emulator-default.md)):
  one thread per board owning an emulated STM32F103 (`src/emu`);
  `SimulationSession` starts it before the circuit and keeps firmware and
  circuit within one coupling window (100 µs) of each other
  ([ADR 0083](../decisions/0083-microcontroller-lockstep.md)); pin levels and
  modes feed the board symbol, the reset button writes SCB AIRCR. Renode, the
  earlier engine ([ADR 0082](../decisions/0082-renode-engine-integration.md)),
  was removed by [ADR 0094](../decisions/0094-remove-renode.md).
- **Coupler** (phase 2): GPIO outputs drive the circuit (SN-132,
  [ADR 0084](../decisions/0084-gpio-output-coupling.md)): two external sources
  per pin feed a behavioral current source, set from the runner's pin history
  at the circuit's time, which the firmware has always reached. Next (SN-133): analog crossings feed GPIO inputs at the end of the
  coupling window (E-03 sampled exchange, labeled approximate).

- **Own STM32 emulator** ([ADR 0089](../decisions/0089-own-stm32-emulator.md),
  [ADR 0090](../decisions/0090-own-emulator-default.md)): `src/emu`, Qt-free,
  in-process (core, bus, STM32F103 peripherals, GDB server); design in
  [MCU_EMULATOR.md](MCU_EMULATOR.md), plan and gates in
  [MCU_EMULATOR_PLAN.md](../planning/MCU_EMULATOR_PLAN.md).

## Threads and processes

- Phase 1–3: engine runs in a worker thread of the application process.
- Since SN-151 (SIM-08, [ADR 0101](../decisions/0101-engine-process.md)): the
  engine runs in a child process (`simnodus-engine`) behind the same
  `sim::Session` interface; `RemoteSession` mirrors its state in the editor
  over the child's standard streams, so a crash in a backend does not affect
  the editor (ADR 0078's containment goal).

## Repository layout

```text
apps/simnodus/        Qt application (editor, analyzer)
src/model, src/io, src/library, src/netlist, src/sim, src/mcu (runner),
src/emu (own STM32 emulator)
resources/library/    component JSON files
examples/             example projects (.snx) and firmware
tests/unit/           C++ unit and simulation tests (ctest)
tests/experiments/    E-01..E-05 backend experiments (kept as reference)
legacy/               previous product code, not built by default
third_party/          vendored single-header libraries with licenses
```
