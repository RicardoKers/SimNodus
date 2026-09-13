# ADR 0046: Exact parameter quantities and scoped overrides

Date: 2026-09-13. Status: accepted for experimental SN-020 schema 0.2;
full project/component format remains pending.

## Decision

Extend the topology draft with bounded decimal-string parameter declarations,
explicit units/inclusive bounds, literal instance overrides and direct forwarding
from containing-circuit parameters. The [0.2 contract](../architecture/PARAMETERS_DRAFT.md)
defines the closed unit vocabulary and exact powers-of-ten conversion. Reuse
0.1 topology validation through a known-field projection without modifying the
historical validator or performing migrations.

Validate forwarded dimensions and the entire source range against the target
range. Compute instance-local effective values without mutating definitions.
Apply a resolved instance/value budget before expansion. Unknown fields, units,
parameters and executable expressions are errors, never silent substitutions.

## Alternatives and consequences

Binary floats can obscure exact document bounds; bounded decimal strings make
unit conversion and round-trip spelling explicit. A general expression language
or SPICE-style unit parser would add ambiguity and execution/complexity concerns
before model mapping exists. Default-only forwarding checks could accept unsafe
future overrides. Use explicit direct references and full interval containment.

The fixed vocabulary is intentionally incomplete. It is not component/backend
support or a universal unit system. Parameter paths remain independent of MCU
family, symbols and display names. Python is a developer reference validator,
not a selected native persistence API or simulation scheduler. No new third-party
dependencies/assets are introduced.

SN-020 remains in progress; SN-021 remains planned. Preserve all 0.1 artifacts,
engine evidence, PDF suppression, PID retry and ADRs 0027/0028. No GUI, model
loading, engine execution, commit or remote publication is part of this decision.

## Revisit criteria

Select further unit dimensions, affine conversions, expressions, runtime root
overrides or parameter types only for concrete scoped requirements with new
invalid/round-trip evidence. Next specify declarative model mappings and symbol
references separately from topology; defer actual resource loading/execution to
its own safe-persistence/runtime work. Full schema acceptance also requires the
remaining project, dependency, firmware and temporal contracts.
