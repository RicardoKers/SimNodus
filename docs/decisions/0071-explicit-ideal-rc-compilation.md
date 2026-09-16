# ADR 0071: Compile only an explicitly requested standalone ideal RC profile

- Status: Accepted for bounded SN-021 implementation and acceptance
- Date: 2026-09-16

## Context

Structural connectivity and numerical bindings do not supply a reference node,
stimulus or analysis authority. General backend lowering would exceed existing
evidence; opening projects must remain inert.

## Decision

Implement the [standalone ideal RC contract](../architecture/IDEAL_RC_COMPILATION.md)
with explicit root-port selection and the existing fixed E-01 parameters. Validate
the complete project, capture resources with retained handles and consume only
owned bytes. Lower recognized single R/C wrappers to generated primitives after
exact value/wiring checks. Keep complete provenance and return an inert artifact.

The explicit analysis request is separate from an unconfigured co-simulation
policy and never rewrites it. Executing the artifact requires a separate action.
Use the real pinned engine to validate the owned fixture artifact without changing
historical fixtures, E-01 tolerances or the fixture's stricter declared tolerance.

## Consequences

No implicit ground, user-source directive injection or pathname reopening is
introduced. Other values, components, floating nodes, reversed mappings and
configured co-simulation projects reject in this operation. No general readiness,
trust, licensing or execution authority follows. Safe overwrite and remaining
project/runtime integration stay pending; SN-017, MCU/toolchain and UI stay intact.
