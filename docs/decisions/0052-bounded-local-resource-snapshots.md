# ADR 0052: Bounded local resource snapshots

Date: 2026-09-13. Status: accepted first SN-021 implementation boundary;
native loading, saving, compilation and runtime acceptance remain pending.

## Decision

Implement an explicit Windows/NTFS reference operation returning immutable bytes
for every locked dependency/resource pair. Keep existing declaration parsers
unchanged. The [contract](../architecture/LOCAL_RESOURCE_VERIFICATION.md) requires
handle-relative component traversal, pinned inspected ancestors, rejection of
reparse points and aliases, bounded same-handle reads and exact size/SHA-256.
Only a complete verified inventory is returned; consumers use snapshots directly.

Use a small Python reference backed by fixed Windows system APIs to establish
physical behavior before extracting a native project loader. This does not select
Python for the production application or alter the C++20 baseline. Fixed OS DLL
access is distinct from loading an untrusted project resource as native code.

## Alternatives and consequences

Lexical prefix checks and resolve-then-open permit substitution between checks
and consumption. Returning verified filenames would reintroduce that race.
Following internal links would require a broader alias/containment policy, so
all reparse points and multiply linked files fail closed in this first slice.
Snapshots consume bounded memory but remain valid after disk paths change.

The operation proves neither trustworthy origin, redistribution permission,
SVG/SPICE interfaces, firmware/boot compatibility nor runtime capability.
It authorizes no execution, rendering or acquisition. No simulation profile,
UI, instrumentation or MCU/toolchain support changes. Preserve ADRs 0027/0028,
0043/0044/0049/0051, historical evidence and existing numerical restrictions.

## Evidence and revisit criteria

The [report](../experiments/SN-021-local-resources.md) distinguishes actual local
filesystem passes, unavailable privileges and hosted checks. Do not infer native
loader acceptance or whole SN-021 completion from this reference operation.
Revisit the contract for native extraction, other filesystems, links, external
imports, asynchronous deadlines or a consumer that cannot use verified snapshots.
Actual source parsers and execution need their own bounded acceptance criteria.
