# ADR 0056: Native topology semantic validation

Date: 2026-09-14. Status: accepted bounded implementation contract.

## Decision

Implement the complete preserved topology 0.1 semantic rules as the first native
schema slice over owned syntax tokens. See the
[native contract](../architecture/NATIVE_TOPOLOGY_VALIDATION.md). This is a
version-specific validation entry point, not the project 0.1 loader. Unsupported
versions and extra fields fail; no implicit conversion drops parameters/bindings.

Use stable IDs, bounded catalogs and memoized hierarchy checks, including unused
definitions. Preserve original source and byte offsets. Return structural
statistics and immutable source ownership, with no graph mutation or I/O.
Compare against the existing Python validator and its fixtures/tests unchanged.

## Rationale and consequences

The full project composes several semantic contracts. Porting one complete
structural contract provides a reviewable reference before parameters, resource
fingerprints, bindings and execution declarations are added. It avoids claiming
the entire project valid after only checking its outer fields.

The current full declaration gate remains Python. This does not regress the
accepted SN-020 baseline, expand executable profiles or implement a public CLI.
The test probe remains a developer fixture. Shared application/domain boundaries,
SN-044, MCU/toolchain independence and SN-017 Python/GDB ownership are preserved.
Resource containment, verified bytes, source interfaces, trust and execution
authorization remain independent gates. Saving and compilation are still pending.
