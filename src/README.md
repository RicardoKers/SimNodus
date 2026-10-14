# Qt-free libraries

Plain C++20 libraries used by the application, tests and spikes
([product architecture](../docs/architecture/PRODUCT_ARCHITECTURE.md)).

| Module | Content | Status |
|---|---|---|
| `sim/` | ngspice run-time loading and callbacks (`ngspice_runtime`), engine check, bounded sample ring, process memory | phase 0 |
| `model/`, `library/`, `netlist/`, `io/` | Document model, component library, connectivity/SPICE generation, `.snx` files | SN-110..SN-117 |

No module includes Qt headers. Backends are loaded only when a caller asks
for them, never while a project is opened.
