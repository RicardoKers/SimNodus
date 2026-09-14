# Native resource lock metadata

Status: bounded SN-021 contract under [ADR 0059](../decisions/0059-native-resource-lock.md).
Implement the complete preserved [resource lock 0.1](RESOURCE_LOCK_DRAFT.md) over
one owned native syntax capture. Accept caller bytes only; keep provenance/license
text, original spelling and token positions. Never infer a root or open resources.

## Acceptance criteria

- Require exact fields/version, nonempty inventories, stable IDs, literal numeric
  dependency versions, bounded printable origin/license declarations and a notice
  referring to a file in its own dependency. Validate unused records too.
- Preserve lexical path rules: 240 characters, 16 segments, 80 characters per
  segment, ASCII alphabet, Windows reserved names, global case collisions and
  file/directory conflicts. Do not repair paths or claim physical containment.
- Enforce 32 dependencies, 256 files, 16 MiB per file and 64 MiB total. Counts
  are exact nonnegative JSON integers, excluding booleans/fractions/exponents.
  Integer `-0` equals zero. Reject overflow without floating conversion.
- Validate lowercase SHA-256 and inventory fingerprints: original ASCII path
  sorting, bytes/path/sha256 only, sorted-key compact ASCII JSON, no newline.
  IDs/provenance do not enter this fingerprint; actual files are not hashed here.
- Return immutable typed requests in source order, file source offsets, owned
  syntax and statistics. All resource/containment/redistribution/readiness flags
  remain false. A later explicit operation may consume these declared requests;
  this validator never calls physical verification or publishes resource bytes.
- Compare unchanged Python tests, rejection categories, full requests and stats.
  Add exact count/byte/integer/path/text limits, canonical ordering, ID/metadata
  independence and unknown nested fields. Verify immutable source ownership,
  error offsets and an inert missing DLL declaration.
- Check the internal digest against known empty/abc vectors and Python hashlib
  over every padding position, binary/multiblock data, 1 MiB and overflow.
  Regress all earlier declaration and physical-resource/native tests.

The portable inventory SHA-256 uses original project code following
[FIPS 180-4](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.180-4.pdf), sections
4.2.2, 5.3.3 and 6.2. Fixed block/schedule storage and a 1 MiB input cap keep this
helper bounded. It is not authentication, FIPS certification or a replacement
for the existing Windows BCrypt physical-byte verifier. No dependency is added.

## Independent gates and limitations

Physical traversal remains the explicit Windows/NTFS snapshot operation with
parent-relative handles, reparse/alias rejection, same-handle size/hash checks
and immutable bytes. Symlinks, junctions, missing files, replacement races and
actual byte mismatch are not metadata properties. A textual prefix or
resolve-then-open check is not introduced as physical proof.

Declared hashes prove neither real contents nor source compatibility, origin,
redistribution permission or execution approval. No download, DLL/model/firmware
loading, rendering, compilation or UI follows. Typed links, project semantics and
native graph loading remain pending; actual source/ELF/boot interfaces and atomic
saving stay separate. Preserve SN-044, local SN-045 documentation, instrumentation,
MCU/toolchain independence, numerical profiles, PDF/PID and SN-017 Python/GDB.

Native ingress retains stricter lone-surrogate rejection. Python's host digit cap
rejects a 5000-digit root integer as input before schema validation; native bounded
syntax rejects its root shape. Both reject; no host digit cap becomes a format
rule. Allocations are input bounded, not a hard OS memory/wall quota. Allocation
failure is structured but not fault-injected.
