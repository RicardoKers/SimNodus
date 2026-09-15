# ADR 0061: Native project declarations over shared captured sources

Date: 2026-09-15. Status: accepted for the bounded SN-021 project declaration slice.

## Decision

Implement [project 0.1 validation](../architecture/NATIVE_PROJECT_VALIDATION.md)
by composing captured resource links and validating platform, firmware, occurrence
target and temporal declarations. Keep one owned original syntax capture with
absolute diagnostic and parameter source positions. Public standalone APIs retain
their strict version gates; internal subtree entry points are not public ingress.

Virtual times use exact uint64 conversion and the existing integer grid/bounds.
Preserve independent catalog identities, nulls, combined budgets, exact tolerances
and fail-closed capability requests. Publish immutable declarations only after
all checks pass. No resources or backends are opened by this operation.

## Consequences

This completes the native composition of the declarative baseline, not SN-021.
An architecture string or requested temporal mode is not runtime compatibility.
Firmware, boot, source interfaces, origin, physical containment, byte verification,
redistribution and execution approval remain independent gates. The original
syntax retains fields needed by later source graph construction without projection
or reserialization. No editable graph or backend-specific type is introduced.

## Next acceptance

Implement bounded native source graph loading with stable IDs and source mappings,
then safe persistence and compilation with the integration evidence they require.
Do not infer generalized model/MCU support or change SN-017 fixture ownership.
