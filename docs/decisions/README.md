# Architecture decision records

An ADR records context, decision, consequences, and revisit criteria. Do not rewrite history to hide a changed decision: add a new ADR and mark the old one superseded.

| ADR | Decision | Status |
|---|---|---|
| [0001](0001-core-and-backends.md) | Headless C++ core, independent graph, ngspice/Renode, Qt presentation | Accepted direction; implementation details proposed |
| [0002](0002-time-and-debugging.md) | One virtual-time authority and explicit debugging coordination | Accepted invariant; algorithm pending experiments |
| [0003](0003-components-and-projects.md) | Separate models/symbols; versioned text projects and reusable subcircuits | Accepted direction; schema proposed |
| [0004](0004-license-language-platform.md) | MIT, English repository, Windows first | Accepted under owner authorization |
| [0005](0005-classroom-scope.md) | February 2027 teaching target with a January readiness gate | Accepted target; delivery scope conditional |
| [0006](0006-windows-backend-baseline.md) | Exact Windows experiment packages and native client prerequisite | Accepted for experiments; distribution/integration unproven |

Use the [template](TEMPLATE.md) for new decisions.
