# ADR 0067: Separate structural connectivity lowering from executable compilation

Date: 2026-09-15. Status: accepted bounded SN-021 structural stage.

## Decision

Compile the validated source graph into a [connectivity partition](../architecture/CONNECTIVITY_COMPILATION.md)
with stable occurrence/terminal identities and complete source provenance. Expand
only reachable definitions, preserving the entire original document separately.
Explicit source nets and hierarchical port identities determine equivalence;
labels and `GND` have no implicit electrical meaning. Keep disconnected terminals.

Use standard domain values for occurrences/terminals/groups, with source ownership
and provenance in application results. Add a 65536-record expansion budget without
changing declarative validity or existing limits. Return immutable results and
structured declaration/expansion errors, never partial output or runtime approval.

## Consequences

This establishes a reusable structural input for later backend lowering. It does
not assign solver nodes, ground, parameter values or models, parse resources or
emit/run a netlist. The current declared SPICE names/maps do not prove a resource
interface, and the schema gives no implicit reference-ground selection. Conflating
those gates would turn metadata acceptance into an unsupported execution claim.

Pure tests compare partitions with an independent adjacency traversal and compare
all source spans to the original declarations. They are appropriate for this
stage, not a substitute for the real-engine evidence required by the next stage.
Next establish explicit supported model/interface, numerical parameter and analysis/
reference lowering from verified captured resources under accepted profiles.
Preserve MCU/toolchain independence, instrumentation, tolerances, PDF/PID behavior
and SN-017 Python preparation/GDB/fixture ownership. Safe overwrite and executable
compilation remain pending; full SN-021 and the next-cycle prompt are not complete.
