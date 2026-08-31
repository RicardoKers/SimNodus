# Requirements

Sources: the current request and owner approvals summarized in [SOURCES](research/SOURCES.md). Milestones are defined in the [roadmap](planning/ROADMAP.md). No simulator capability below is implemented in this foundation.

## Functional requirements

| ID | Requirement | Milestone | Acceptance evidence |
|---|---|---|---|
| RF-01 | Run real ELF firmware on STM32F103C8/Blue Pill | M1 | Rebuildable firmware; observed boot and GPIO in Renode |
| RF-02 | Couple MCU outputs to analog circuits | M2 | GPIO to RC/LED traces compared against a reference |
| RF-03 | Deliver digital input and EXTI to firmware | M2 | Electrical transition causes an input read/interrupt at the modeled time |
| RF-04 | Read analog voltage through ADC | M2 | Applied values, sampling time, units, and readback documented |
| RF-05 | Debug through STM32CubeIDE/GDB | M2/M3 | Breakpoint, continue, step, and reset without silent domain divergence |
| RF-06 | Edit, save, reopen, and run a small circuit | M3 | Round-trip preserves IDs, connectivity, parameters, and firmware reference |
| RF-07 | Inspect voltage, branch current, digital signals, and UART | M2/M3 | Exportable traces first; graphical instruments later |
| RF-08 | Separate symbol, pins, and simulation model | M1/M3 | Headless model; replacing a symbol does not change connectivity |
| RF-09 | Reuse a subcircuit with explicit ports | M4 | Save locally and instantiate twice with independent state |
| RF-10 | Discover project and user-library subcircuits | M4 | Predictable imports, conflicts, updates, and dependencies |
| RF-11 | Explain circuit mistakes | M3/M5 | Diagnostics identify entity, time, cause, and modeling limits |
| RF-12 | Author components through manifests and external models | M4/M6 | Validated package and example; explicit code-execution policy |
| RF-13 | Support WASM components | M6 | Versioned API with tested memory/execution limits |
| RF-14 | Integrate Verilog/Verilator | M7 | One time owner per model and a reproducible co-simulation example |
| RF-15 | Inspect node/pin/peripheral/firmware relationships | M5+ | Traceable information with unavailable fields clearly identified |

## Nonfunctional requirements and constraints

| ID | Requirement | Evaluation |
|---|---|---|
| RNF-01 | Explicit virtual-time causality | No silently applied late events; approximate modes labeled |
| RNF-02 | Reproducibility | Stable discrete traces in the same environment; published analog tolerances |
| RNF-03 | Stability and recovery | Backend failure/timeout preserves the user's circuit document |
| RNF-04 | Platform strategy | Windows first, Linux later; portable C++20 domain/kernel |
| RNF-05 | Responsive interface | Solver outside the GUI thread; bounded queues and decoupled refresh |
| RNF-06 | Portable, readable projects | Versioned format, no required personal absolute paths or implicit downloads |
| RNF-07 | Safe model handling | Path/size limits; no automatic native code loading during project opening |
| RNF-08 | Honest fidelity claims | Explicit board/peripheral coverage and approximations; no cycle-accuracy promise |
| RNF-09 | Extensibility | Independent adapters; no public plugin ABI tied to C++ |
| RNF-10 | Auditable redistribution | Origin, revision, license, and notices for dependencies and models |
| RNF-11 | Shareable repository | All committed project text in English; original material under MIT |
| RNF-12 | Classroom readiness | Windows rehearsal and known-limitations review before February 2027 |

## MVP boundary

M2 is the technical proof. M3 is the first teaching MVP: MCU, R/C/LED/switch/source/GND, basic observation, and debugging. M4 adds reusable subcircuits; earlier formats must preserve hierarchy as a design concern. M4 is a stretch goal for the first classroom release, not a prerequisite to proving co-simulation.

Performance, circuit-size limits, minimum PC specifications, and supported Windows versions will be measured and agreed before the classroom freeze. Calendar targets must not overrule correctness gates.
