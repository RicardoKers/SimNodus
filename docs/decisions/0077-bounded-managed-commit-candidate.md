# ADR 0077: Evaluate a bounded immutable revision chain

Date: 2026-09-24; updated 2026-09-25. Status: **selected experimental candidate;
bounded physical store batch passed, full acceptance pending**.

Under [ADR 0076](0076-managed-document-saving.md), evaluate the
[bounded commit design](../architecture/MANAGED_COMMIT_EXPERIMENT.md): one complete
record per revision, published by create-only handle-relative rename inside a
separately provisioned, exclusive-writer namespace. Do not update data, version and
receipt as independent authoritative objects. There is no mutable head pointer.

Records bind exact document/context bytes, predecessor identity/version and the
authenticated operation receipt. A bounded validated chain establishes current
state; retain all committed receipts and refuse at capacity rather than pruning.
The initial experiment supports one document and at most 64 committed revisions.
This is a test/storage limit, not expansion of the accepted project/runtime profile.

An admin-owned, nonreplaceable lock serializes trusted writers; ACLs exclude ordinary
clients from the namespace. That combination must be proven for the write-enabled
candidate and does not justify advisory locks in external user-writable directories.
All existing physical containment, alias/reparse and byte-validation rules remain.

The [codec](../experiments/SN-021-managed-record-codec.md),
[chain](../experiments/SN-021-managed-chain.md) and
[fixture-only Windows store batch](../experiments/SN-021-managed-store-candidate.md)
are implemented and measured within their stated bounds. No service or production
Save API is selected. The batch supports process-interruption recovery on the
measured local NTFS configuration. It does not establish power-loss durability,
authenticated Save transport or complete physical acceptance below.

The design explicitly fences prior execution runs before interpreting an absent
receipt as not committed. Snapshot rollback within a store generation is unsupported.
External overwrite stays unsupported under ADR 0064. SN-021 remains in_progress;
preserve all earlier evidence, SN-017/SN-044, MCU independence, tolerances and PDF/PID.
