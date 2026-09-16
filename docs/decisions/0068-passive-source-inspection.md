# ADR 0068: Inspect a bounded passive source grammar before model binding

- Status: Accepted for the bounded SN-021 slice
- Date: 2026-09-15

## Context

Structural connectivity and declarative resource links cannot prove the content
of a SPICE subcircuit interface. Project opening must remain inert, and general
SPICE directives would exceed the accepted ideal RC experiment profile.

## Decision

Add a pure adapter reader of owned bytes with the narrow grammar and budgets in
[the contract](../architecture/PASSIVE_SPICE_SOURCE.md). Return explicit ordered
formal terminals, parameter/default spelling, R/C kind and source spans. Reject
the whole operation on unsupported syntax; do not infer models from filenames,
descriptor maps or hashes. No engine call or implicit resource acquisition occurs.

Use only the existing owned fixture for an explicitly invoked real ngspice E-01
acceptance run after native physical capture and recognition. Keep all existing
RC tolerances and authority in the test harness. Record negative attempts too.

## Consequences

Recognition is narrower than full interface compatibility. Parameter units,
ranges, descriptor binding and effective-value conversion remain separate work.
No readiness flag becomes true; source origin and redistribution remain separate
from bytes and syntax. This slice adds no general model support, UI, project
execution or safe overwrite. The native graph and SN-017 ownership are unchanged.
