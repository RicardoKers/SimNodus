# Qt-free libraries

Plain C++20 libraries used by the application, tests and spikes
([product architecture](../docs/architecture/PRODUCT_ARCHITECTURE.md)).

| Module | Content | Status |
|---|---|---|
| `sim/` | ngspice run-time loading and callbacks (`ngspice_runtime`), engine check, bounded sample ring, process memory | phase 0 |
| `model/` | Document (stable IDs), geometry, engineering notation, undoable edits | SN-110 |
| `library/` | Component library loader and SPICE templates ([format](../docs/architecture/COMPONENT_LIBRARY.md)) | SN-111 |
| `io/` | `.snx` project files and clipboard ([format](../docs/architecture/PROJECT_FORMAT.md)) | SN-115 (files) |
| `editing/` | Editor operations: place, wire, move with attached wires, rotate, mirror, delete, parameters, copy/paste | SN-110/112 |
| `netlist/` | Connectivity, pre-run checks, SPICE generation | SN-116 |

No module includes Qt headers. Backends are loaded only when a caller asks
for them, never while a project is opened.
