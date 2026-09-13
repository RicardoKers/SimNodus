# ADR 0051: Project declaration baseline and SN-020 acceptance boundary

Date: 2026-09-13. Status: accepted declaration specification; runtime implementation
and supported executable project profiles remain separate gates.

## Decision

Compose the preserved schemas into [project declarations 0.1](../architecture/PROJECT_SCHEMA.md).
Specify stable project identity, explicit typed resources, board/MCU distinction,
per-occurrence firmware/platform targets, complete pin maps and requested temporal/
fidelity policy. Reject unknown fields/versions, ambiguous associations, invalid
ranges and unsupported policies. Preserve the earlier drafts without rewriting
their acceptance history or hashes.

Use the reference validator and adversarial/round-trip fixtures to accept the
SN-020 schema/validation task at this bounded declaration level. This is a
development specification, not native project loading, schema migration, model
execution, GUI implementation or a promise of executable arbitrary projects.
The acceptance audit must identify every remaining runtime and product gate.

## Rationale and consequences

Board identity is not MCU identity; IDE/toolchain choices do not belong in core
data. Concrete instance paths prevent shared-definition execution state. A target
cannot simultaneously select a SPICE model and an MCU owner. Firmware/resource
metadata and architecture agreement remain declarations requiring independent
verification. No physical platform support is inferred from synthetic test data.

Unconfigured documents are explicitly editable and non-executable. Configured
requests preserve the existing 10 microvolt, 1 ps and 2 s bounds and distinguish
virtual nanoseconds from wall deadlines. Approximate sampled behavior is labelled;
runtime capability rejection is mandatory. No additional timing controls or
backend profiles are established by passing schema checks.

Keep SN-044/ADR 0049 unchanged. Layout, measurements, capture retention and Qt
choices stay with their existing tasks; the baseline reserves no executable
extension hook and does not turn presentation state into simulation state.

## Revisit criteria

SN-021 must validate actual resources, source interfaces, ELF/boot/device details,
physical containment, atomic saving and compilation before executable project
acceptance. New formats, model categories, richer pin semantics, migrations,
layout/probe data and portable import packaging require explicit versioned
contracts and evidence. Never relabel declared validity as runtime readiness.
