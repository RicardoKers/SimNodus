# ADR 0059: Native inert resource lock metadata

Date: 2026-09-14. Status: accepted for the bounded SN-021 lock 0.1 slice.

## Decision

Implement [native lock metadata](../architecture/NATIVE_RESOURCE_LOCK.md) over
owned syntax, retaining exact integers, lexical paths and all original text.
Return immutable declared requests in source order, offsets and false verification
flags. Do not call the physical snapshot operation from declaration validation.

Use bounded portable SHA-256 for canonical inventory fingerprints, independently
checked against known vectors and Python hashlib. Keep Windows BCrypt and all
existing physical handle/race protections unchanged. No new dependency is needed.

## Alternatives and consequences

Calling physical verification here would cross the explicit resource boundary.
Windows-only hashing would prevent portable declaration tests. The internal
digest is not a trust/authentication API or a general cryptographic framework.
Original ASCII paths determine serialization order; case folding is solely
collision policy. IDs and provenance never authenticate the declared inventory.
Original reference validators, fixtures and historical evidence remain intact.

## Revisit criteria

Port typed resource links and project semantics before native source graph loading.
Actual content/interfaces, source trust, redistribution, saving, compilation and
execution approval remain independent gates. New formats/path policies need
explicit contracts; preserve accepted engine profiles and the Python/GDB boundary.
