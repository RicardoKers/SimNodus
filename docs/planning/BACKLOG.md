# Backlog

States: `done`, `in_progress`, `ready`, `planned`, `blocked`. Priority: P0 critical path, P1 near-term, P2 later. Dependencies are task IDs. Production kernel/application implementation has not started.

`SN-001` is the repository-foundation task. SN-010 completed dependency selection and startup checks; SN-011 passed the standalone RC/lifecycle experiment. Coupled engine and application features remain unimplemented. Keep the Markdown status and GitHub issue status consistent; do not maintain independent competing queues.

## Foundation and technical proof

| ID | Priority | State | Task | Depends on | Acceptance evidence |
|---|---|---|---|---|---|
| SN-001 | P0 | done | Establish architecture, planning, license, scaffolding, checks | — | English files and local structural verification |
| SN-010 | P0 | done | Inventory environment and pin backend revisions | SN-001 | [Version, provenance, hashes, setup and startup report](../experiments/SN-010-results.md); integration limitations explicit |
| SN-011 | P0 | done | Run E-01 standalone ngspice | SN-010 | [Eight real cases, three local runs, numeric/lifecycle evidence](../experiments/E-01-results.md); pre-init crash remains an unsupported route |
| SN-019 | P0 | done | Adapt the pinned Renode external client for native Windows | SN-010 | [Native build, verified loopback server, real time/reconnect and fault evidence](../experiments/SN-019-results.md); two complete 20-case local runs |
| SN-012 | P0 | done | Run E-02 standalone Renode | SN-010, SN-019 | [Rebuildable ELF, offline C8 profile, real timed GPIO/input/EXTI evidence and coverage audit](../experiments/E-02-results.md) in two local runs |
| SN-013 | P0 | done | Define supported temporal capability profile | SN-011, SN-012 | [Evidence-bounded units, timing, operating modes, failure semantics, and predeclared E-03 gates](../architecture/TEMPORAL_CAPABILITY_PROFILE.md) |
| SN-014 | P0 | done | Run E-03 coupled GPIO and digital feedback | SN-013 | [54 fresh real-backend cases: replay passed; sampled delays measured and explicitly approximate](../experiments/E-03-results.md) |
| SN-015 | P0 | done | Run E-04 ADC path | SN-012, SN-014 | [Focused microvolt-to-firmware path, timing, sampling, boundaries, mapping, ramp, and limitations](../experiments/E-04-results.md) |
| SN-016 | P0 | ready | Run E-05 GDB and CubeIDE coordination | SN-014 | Breakpoint, step, continue, reset, disconnect, timeout evidence |
| SN-017 | P0 | planned | Extract tested kernel/adapters from experiments | SN-014, SN-015, SN-016 | Headless runner and contract tests using real engines |
| SN-018 | P1 | planned | Establish reproducibility/performance baseline | SN-017 | Fixed reference examples, latency/error/memory measurements |

## Teaching MVP

| ID | Priority | State | Task | Depends on | Acceptance evidence |
|---|---|---|---|---|---|
| SN-020 | P1 | planned | Specify circuit/component schema and validation | SN-013 | Versioned schema; stable IDs; valid/invalid round-trip fixtures |
| SN-021 | P1 | planned | Implement project loading/saving and circuit compilation | SN-020 | Portable paths, atomic save, source mapping, independent graph |
| SN-022 | P1 | planned | Select Qt modules and worker boundary | SN-014 | Small UI experiment, licensing inventory, crash-handling decision |
| SN-023 | P1 | planned | Build minimal Windows editor | SN-017, SN-021, SN-022 | R/C/LED/switch/source/GND/MCU, wiring, properties, undo, save/open |
| SN-024 | P1 | planned | Add scope, logic view, and UART terminal | SN-018, SN-023 | Responsive plots; units/provenance; exportable full traces |
| SN-025 | P1 | planned | Add core teaching diagnostics | SN-023 | Invalid wiring, unsupported mode, floating/undefined input explained |
| SN-026 | P0 | planned | Prepare three reproducible lesson projects | SN-024, SN-025 | Firmware sources, expected traces, guides, fidelity notes |
| SN-027 | P0 | planned | Package and test Windows classroom candidate | SN-026 | Clean/offline install rehearsal; dependency notices; known limitations |
| SN-028 | P0 | planned | Conduct January classroom readiness review | SN-027 | Owner go/no-go with exact release and fallback materials |

## Reuse, extensions, and public project

| ID | Priority | State | Task | Depends on | Acceptance evidence |
|---|---|---|---|---|---|
| SN-030 | P1 | planned | Implement project-local subcircuits | SN-021, SN-023 | Two RC instances, independent state, explicit ports, recursion rejection |
| SN-031 | P2 | planned | Implement user library and dependency locking | SN-030 | Conflict/update behavior; portable exported package |
| SN-032 | P2 | planned | Expand validated peripheral and component coverage | SN-018 | Feature-specific firmware reports and licensed models |
| SN-033 | P2 | planned | Add node/peripheral/software inspection | SN-024, SN-032 | Traceable fields and explicit unavailable values |
| SN-034 | P2 | planned | Design and implement WASM SDK | SN-031 | Versioned capabilities, execution limits, malicious-package tests |
| SN-035 | P2 | planned | Evaluate HDL integration | SN-017 | ADR chooses one integration path; reproducible HDL example |
| SN-036 | P2 | planned | Ship a Linux application build | SN-027 | Linux integration tests, packaging, documentation, clean-machine run |
| SN-040 | P1 | done | Resolve GitHub owner/name and publication review | SN-001 | RicardoKers/SimNodus and Ricardo Kerschbaumer confirmed; privacy review completed |
| SN-041 | P1 | done | Publish initial public source repository | SN-040 | Public first commit; Windows/Ubuntu CI passed; issue links, branch protection, and private security reporting configured |

## GitHub issue mapping

| Task | Issue |
|---|---|
| SN-010 | [#1 — Toolchain and backend revisions](https://github.com/RicardoKers/SimNodus/issues/1) |
| SN-011 | [#2 — Standalone ngspice](https://github.com/RicardoKers/SimNodus/issues/2) |
| SN-012 | [#3 — Standalone Renode](https://github.com/RicardoKers/SimNodus/issues/3) |
| SN-013 | [#4 — Temporal capability profile](https://github.com/RicardoKers/SimNodus/issues/4) |
| SN-014 | [#5 — GPIO and feedback causality](https://github.com/RicardoKers/SimNodus/issues/5) |
| SN-015 | [#6 — ADC path](https://github.com/RicardoKers/SimNodus/issues/6) |
| SN-016 | [#7 — Coordinated debugging](https://github.com/RicardoKers/SimNodus/issues/7) |
| SN-017 | [#8 — Verified headless kernel](https://github.com/RicardoKers/SimNodus/issues/8) |
| SN-018 | [#9 — Reproducibility and performance](https://github.com/RicardoKers/SimNodus/issues/9) |
| SN-019 | [#11 — Native Windows Renode client](https://github.com/RicardoKers/SimNodus/issues/11) |

## Definition of ready

A task has bounded scope, dependencies, acceptance evidence, and required inputs. Missing laboratory details do not block standalone backend experiments.

## Definition of done

The result exists, relevant checks passed, limitations and evidence are recorded, licensing is accounted for, and status/ADRs are updated. A design document, mock, or passing repository check never counts as a working engine feature.
