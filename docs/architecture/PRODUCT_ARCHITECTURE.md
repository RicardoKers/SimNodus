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
|             SPICE generation      |       |  RenodeEngine (phase 2) + Coupler       |
+-----------------------------------+       +-----------------------------------------+
```

Rules:

- `src/model`, `src/library`, `src/netlist`, `src/io` and `src/sim` are plain
  C++20 with no Qt dependency, so they run in tests, a future CLI and the engine
  process. JSON uses the vendored single-header nlohmann/json (MIT).
- `apps/simnodus` contains all Qt code. Qt Widgets only (ADR 0081).
- No GUI code calls ngspice or Renode directly; it talks to `SimulationSession`
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
- **RenodeEngine + Coupler** (phase 2): Renode leads by quantum Q; GPIO outputs
  become external sources; analog crossings feed GPIO inputs at the end of the
  quantum (E-03 sampled exchange, labeled approximate). Reused knowledge: pinned
  Renode 1.16.1, STM32F103 platform description, owned ADC model, cancellable
  run patch for debugging.

## Threads and processes

- Phase 1–3: engine runs in a worker thread of the application process.
- Phase 4 (SIM-08): engine moves to a child process (`simnodus-engine`) with the
  same `SimulationSession` API over a local socket, so a crash in a backend does
  not affect the editor (ADR 0078's containment goal).

## Repository layout

```text
apps/simnodus/        Qt application (editor, analyzer)
src/model, src/io, src/library, src/netlist, src/sim, src/mcu (phase 2)
resources/library/    component JSON files
examples/             example projects (.snx) and firmware
tests/unit/           C++ unit and simulation tests (ctest)
tests/experiments/    E-01..E-05 backend experiments (kept as reference)
legacy/               previous product code, not built by default
third_party/          vendored single-header libraries with licenses
```
