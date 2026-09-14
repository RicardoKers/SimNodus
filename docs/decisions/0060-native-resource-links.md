# ADR 0060: Native resource links over shared captured syntax

Date: 2026-09-14. Status: accepted for the bounded SN-021 resource-link slice.

## Decision

Implement [typed resource links](../architecture/NATIVE_RESOURCE_LINKS.md) by
composing existing validators over token subtree roots in one immutable capture.
The internal entry points require valid native ingress/source indices and may
throw structured errors; only fully validated immutable results are published.
All public standalone version gates remain unchanged.

Validate complete resource-role associations, explicit nulls, entrypoints and
bijective source mappings with the existing budgets. Retain original bytes and
absolute source offsets throughout nested validation and parameter inspection.

## Alternatives and consequences

Reparsing serialized subdocuments would duplicate captures and shift diagnostics.
Dropping fields during projection could bypass nested rules. Shared trusted token
roots preserve one ownership boundary without creating an editable graph or ABI.
The internal helpers do not accept arbitrary caller-built syntax as a public API.

All resource/interface/trust/execution flags remain unverified. Declared tokens
never authorize resource I/O, model execution or rendering. No supported backend,
MCU, temporal profile, UI or instrumentation behavior changes.

## Revisit criteria

Next compose project identity/platform/firmware/temporal declarations, then native
source graph loading, safe persistence and compilation under explicit acceptance.
Actual source interfaces and executable profile evidence stay separate. SN-021
is not complete after this metadata slice.
