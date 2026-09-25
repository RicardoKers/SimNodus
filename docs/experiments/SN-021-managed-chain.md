# SN-021: bounded chain and commit preparation

Date: 2026-09-25. Status: inert application logic, using synthetic observations.
This implements part of the [ADR 0077 candidate](../architecture/MANAGED_COMMIT_EXPERIMENT.md),
building on the [canonical record codec](../architecture/MANAGED_RECORD_FORMAT.md).
It is not physical recovery, authentication, publication or a managed-save service.

## Implemented boundary

[The native API](../../src/application/managed_chain.hpp) validates at most 64
supplied committed-record observations and 128 MiB of encoded bytes. It checks
each record with the existing codec, requires an exact canonical revision leaf,
scope/volume match, non-null unique file identities, contiguous revisions and the
exact previously observed token. Repeated commit/operation IDs reject, including
nonadjacent duplicates. Enumeration order does not select the current revision.
Results own their decoded records and expose only const access.

These observations are declarations supplied by a future platform adapter. They
cannot prove their own filesystem identity, completeness, ACL, owner, link count,
absence of reparse points or exclusivity. A valid prefix remains structurally valid;
the platform must prove complete bounded enumeration while retaining the writer lock
and handles. The 128-entry namespace budget, setup/staging classification and orphan
preservation belong to that physical acquisition layer, not this list validator.

Receipt lookup requires scope and operation identity, then compares exact principal
SID, expected token, context and project bytes with the recorded request. A collision
rejects without returning another principal's receipt. A matching old receipt stays
available after later commits and at capacity. Unknown operations return only
`ManagedReceiptNotObserved`: this cannot certify not committed while an old writer
or request may still publish. Run authentication/fencing remains outside this API.
Cheap input bounds precede lookup; an absent result does not validate the whole request.

Explicit commit preparation resolves an existing receipt first, then checks the
current expected token, capacity and a caller-provided non-null unused commit ID.
The codec validates fresh request metadata and produces owned canonical bytes, the
next leaf and digests. No temporary, directory, file or endpoint is created. Import
requires an empty supplied chain and null expected token. Refusals do not mutate
inputs or delete records. No pruning, catalog update or last-writer-wins path exists.

Two calls on the same snapshot can both prepare bytes. Preparation is deliberately
not publication authority: a future serialized writer must recheck against its
complete current physical snapshot before staging/rename. An older immutable object
does not automatically become current because the caller retained it. Caller-supplied
principal/context must come from authenticated and authorized operation input, not
project metadata. No pure byte API can establish that trust boundary.

## Tests and limits

[Native adversarial tests](../../tests/resources/native_managed_chain.cpp) exercise
53 assertions including owned observations, reordered enumeration, exact request
conflicts, stale/changed physical-token declarations, A-B-A byte revisions, retained
older/middle receipts, gap/duplicate/corrupt/rehashed records, reused file/commit/
operation identities, invalid names, capacity and prefix incompleteness. The same
test builds all 64 revisions and checks refusal of 65 without discarding old receipts.

The first targeted local run passed the new 53 checks plus the existing 17 native
record checks and 370 independent Python codec cases. These are synthetic observations;
no concurrent filesystem publication, real lock, lost pipe response, process crash,
or resource consumption is simulated and reported as physical evidence.

Final local MSVC Debug build: **53/53 CTests passed** in 60.34 seconds. The repository
checker passed 768 workspace text files. Staged/worktree diff checks passed; prior
evidence hashes and all twelve preexisting local overlays were preserved.

```powershell
cmake --build build/sn021-managed-codec --config Debug
ctest --test-dir build/sn021-managed-codec -C Debug --output-on-failure
python tools/check_repository.py
```

The PR records final full local/hosted checks and source/squash identities. Preserve
158 historical evidence JSON files and twelve unrelated local overlays. SN-021
remains in_progress. Next is the handle-retaining write-enabled platform candidate,
exact provisioning/security checks and the consolidated physical interruption batch.
No service installation, new runtime profile, UI or final next-cycle prompt yet.
