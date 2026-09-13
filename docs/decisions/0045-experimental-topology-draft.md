# ADR 0045: Start schema work with a bounded topology draft

Date: 2026-09-12. Status: accepted for the experimental SN-020 first slice;
complete project/component schema remains pending.

## Decision

Define [topology 0.1](../architecture/TOPOLOGY_DRAFT.md) as a small standalone JSON
document with component pin declarations, reusable circuits, instances, ports
and explicit nets. Use persistent IDs separate from display names and preserve
hierarchy. Repeated instances refer to shared definitions but have distinct
instance-ID paths. Validate all definitions, including unused ones, against
recursion, depth and expansion budgets.

Use an executable Python reference validator and valid/invalid JSON fixtures to
test the document contract before selecting native persistence APIs. This is
development-time schema validation, not runtime orchestration, a production
loader or a second simulation timeline. It reads one supplied file and has no
resource discovery, model execution, dependency downloads or backend invocation.

Reject unknown fields/versions rather than losing user data silently. The
experimental version is distinct from application versions. Limits are parser/
graph-validation budgets, not supported simulated circuit sizes. Names, including
GND, have no implicit electrical or global meaning. One-pin/unconnected topology
can be structurally valid without being electrically solvable.

## Alternatives and consequences

A complete package/project format now would require unselected model, symbol,
resource, dependency, firmware and temporal-policy contracts. Start with the
identity/connectivity boundary and make those remaining decisions explicit in
later SN-020 slices. A generated SPICE netlist would lose the source hierarchy
and couple the document to a backend; it is not the source format.

All fixtures are owned structural examples with no incorporated third-party
assets. Two RC-shaped instances prove distinct structural paths and JSON round
trip only; they are not new real-engine or electrical-support evidence.
SN-020 remains in progress and SN-021 remains planned. ADRs 0003/0027/0028 and
the accepted engine/capability restrictions remain unchanged. No GUI, loader,
compiler, autonomous simulator, commit or remote publication is implied.

## Revisit criteria

Before full SN-020 acceptance, specify unit-bearing parameters and mappings,
symbol/layout and model separation, firmware/board metadata, locked resources,
temporal configuration and safe reference handling. Add fixtures for those
contracts, decide migration/unknown-field behavior across versions, and retain
all 0.1 evidence when introducing a later draft. SN-021 owns actual loading,
atomic saving and compilation; opening a document must never execute host code.
