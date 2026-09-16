# ADR 0069: Keep passive source correspondence separate from numerical readiness

- Status: Accepted for a bounded SN-021 slice
- Date: 2026-09-15

## Context

ADR 0068 recognizes a source grammar but does not connect declared maps to actual
interfaces. Hashes alone cannot prove this correspondence, and a partial match
must not make a project ready for simulation.

## Decision

Add a pure selected-descriptor operation under the
[correspondence contract](../architecture/PASSIVE_INTERFACE.md). Revalidate the
entire metadata document, recognize owned source bytes and match exact resource
size/hash, entrypoint, explicit ordered terminal/parameter maps and R/C unit.
Retain metadata, source, limits and provenance without changing readiness flags.

## Consequences

The result supplies explicit evidence for a later numerical binder without
guessing formal order or accepting unrelated bytes. Physical containment remains
a separate acquisition result. Defaults, effective-value conversion/ranges,
occurrence binding and analysis authority remain pending. No engine invocation,
UI, profile expansion, SN-017 ownership change or safe overwrite is introduced.
