# ADR 0053: Native resource verification before project loading

Date: 2026-09-13. Status: accepted for the bounded SN-021 native snapshot API;
project loading, saving, compilation and simulation readiness remain pending.

## Decision

Extract [resource verification](../architecture/NATIVE_RESOURCE_VERIFICATION.md)
into C++20 with an application-facing typed request/result API and Windows-only
filesystem implementation. Return shared ownership of a const complete snapshot
collection or a structured error. No Windows types enter the public API, and
no Qt, engine, MCU or toolchain dependency is introduced.

The caller still validates the full declaration and lock. Native validation
independently enforces all safety-relevant request bounds, IDs and lexical paths;
it does not parse JSON, validate origin/license metadata or recompute the declared
inventory fingerprint. This is an explicit partial extraction, not an alternative
way to declare a complete project valid.

Pin and inspect handle-relative ancestors, reject reparse/offline objects, aliases
and hard links, and hash the same captured bytes with the Windows SHA-256 API.
Require queryable case-insensitive directories. Reject case-sensitive directories
even though the historical Python reference does not enforce that newer gate.
Native root limits count UTF-16 code units, a documented conservative difference.

## Rationale and alternatives

Porting JSON/graph loading, source parsers and saving simultaneously would hide
the filesystem boundary behind unrelated functionality. Filename-only approval
would reintroduce substitution races; snapshots preserve the bytes actually
verified. Following links, supporting additional filesystems or accepting unknown
directory case policies would expand the unvalidated profile.

Use a separate test-only probe for typed transport and deterministic pauses.
Pause hooks are compiled out of the normal library. The fixture protocol is not
an application IPC contract and never carries executable instructions from a
project. Normal and instrumented builds are both exercised against real files.

## Evidence, consequences and revisit criteria

The [report](../experiments/SN-021-native-resources.md) records typed adversarial
tests, real Windows comparisons/substitution attempts and remaining privilege
skips. The prior Python implementation and all historical evidence are preserved,
including initial failed build/test attempts in this extraction.

SN-021 remains in progress. Next specify bounded native declaration ingress and
graph construction, retaining the metadata/bytes/interfaces/execution gates.
Atomic saving, source mapping/compilation, SVG/SPICE/ELF compatibility and runtime
negotiation need later acceptance. Preserve SN-044, ADRs 0027/0028/0043/0044/0049,
Python/GDB fixture ownership, tolerances, PDF suppression and PID retry.
