# ADR 0047: Separate declarative symbol and model bindings

Date: 2026-09-13. Status: accepted for experimental SN-020 schema 0.3;
complete project/component format remains pending.

## Decision

Add independent symbol/model interface catalogs and explicit component bindings
as specified in [0.3](../architecture/BINDINGS_DRAFT.md). Circuits may bind symbols
through their ports; their behavior remains the hierarchy. Visual anchors,
logical pins and model terminals retain distinct IDs and explicit maps. Parameter
maps preserve dimensions and contain the complete allowed source range.

Require complete one-to-one maps. Reject unknown/partial bindings and unsupported
resource/execution fields. Explicit null bindings never select a fallback model.
Always report simulation readiness false: declarations are not verified engines.

## Alternatives and consequences

Implicit name/order mapping would couple connectivity to visual/model editing.
Bundling graphics or native resources now would prematurely select resource,
trust and licensing policies. Test symbol replacement against unchanged topology,
model maps and parameter values before selecting those additional contracts.

Hidden pins, multi-unit symbols, many-to-one mappings, per-instance model
selection, geometry, backend invocation and resource paths remain unsupported.
Future requirements need explicit versioned evidence. No plugin ABI, additional
MCU, electrical implementation or renderer is established. Fixtures are owned
declarations with no third-party assets.

SN-020 remains in progress; 0.1/0.2 files stay intact and SN-021 stays planned.
Keep ADRs 0003/0027/0028, capability limits, PDF suppression and PID retry. This
schema slice itself performs no engine run or publication.

## Revisit criteria

Next define locked resource/dependency metadata and path containment, with
origin/license records and no automatic execution/download. Select model code
only after capability and redistribution review. Board, firmware and temporal
policies still need contracts before full project-schema acceptance.
