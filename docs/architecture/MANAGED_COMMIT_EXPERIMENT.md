# Managed commit experiment: bounded immutable revision chain

Date: 2026-09-24; updated 2026-09-25. Status: **inert codec/chain and fixture-only
physical store candidate implemented; bounded VM batch passed, full acceptance pending**.
The [canonical format](MANAGED_RECORD_FORMAT.md), [parser evidence](../experiments/SN-021-managed-record-codec.md)
and [chain tests](../experiments/SN-021-managed-chain.md) cover supplied bytes/observations;
the [physical store report](../experiments/SN-021-managed-store-candidate.md) records
separate Windows 11/NTFS process and filesystem observations.
This implements no service and grants no new runtime authority. It refines
[managed saving](MANAGED_DOCUMENT_SAVE.md) under ADRs 0076/0077.

## Selection and limits

Use one provisioned document in one fresh local NTFS store generation, one trusted
ordinary writer identity and independently authenticated ordinary clients. A commit
is a complete immutable-by-algorithm record, not a mutable data/catalog pair.
There is no mutable head pointer. Current means the last record of the unique,
validated contiguous chain. A hash alone never selects an untrusted branch.

Candidate limits (not extensions to the project profile):

| Item | Bound |
|---|---|
| Documents in the physical probe | 1 |
| Committed revisions, including import | 64; refuse before staging revision 65 |
| Document bytes | Existing 1 MiB limit and unchanged native validation |
| Resource context | One explicitly selected external root; at most 16 KiB encoded context |
| Complete record | At most 2 MiB including all headers/metadata; document limit still applies |
| Committed bytes scanned at startup | At most 128 MiB and 64 records |
| Directory entries | At most 128, including setup objects, records and staging orphans |
| Staging allocation | One per save, at most eight name collisions, fixed 64 KiB I/O chunks |
| In-flight publication | One; second writer fails acquisition rather than waiting indefinitely |

No pruning, compaction, migration or automatic orphan cleanup. Capacity exhaustion
is a definite refusal preserving all committed history/receipts. Exceeding startup
bounds or encountering unknown objects fails closed; it is not permission to delete.
The initial probe does not claim a practical unlimited document history.

## Provisioned layout and ownership

The trusted setup creates a root with a sealed generation manifest, one document
directory and an existing `writer.lock`. No ordinary client can create, modify,
replace, rename, link or change security on these objects. Provision all relevant
ancestors and object descriptors before exposing the store; never adopt an existing
client-writable directory. The document directory admits only the trusted writer's
required add-file/read/traverse access, without client mutation or writer delete-child
authority over the admin-owned lock. Its exact rights, inheritance and owners must
be pinned in the implementation and verified physically, including newly created
objects. Do not reuse read-only-fixture success as write-enabled-store acceptance.

The lock is setup-owned, cannot be replaced/deleted by the writer or clients, and
grants the writer only the access required to open it. The writer retains its handle
with sharing disabled for its entire lifetime. This serializes trusted writer
processes inside the ACL-protected namespace; it is not an advisory lock offered as
protection from unrelated writers in an arbitrary external directory. Prove second
writer refusal, lock identity preservation and release after process termination.

Retain validated root/document/lock handles. Create and acquire every child using
the existing handle-relative, no-reparse/no-alias contract. Inspect ownership,
exact DACL, regular-file type, identity, single-link count and size. Reject unknown
ownership, hardlinks, case aliases, gaps, duplicates and unexpected subdirectories.
Normal code never edits or deletes a committed record, even if the trusted writer
has creation-derived authority. Malicious writer/admin/kernel behavior is outside
the ordinary-client trust boundary, not prevented by the word immutable.

## Record and version model

Committed names are eight lowercase hex revision digits followed by `.commit`:
`00000001.commit` through `00000040.commit`. Staging names are exclusively created
`stage-<32 lowercase hex>.tmp` leaves. A name alone proves neither validity nor
ownership. No pathname supplied by a client selects any internal object.

Each self-contained record binds:

- Format/version, exact lengths and zero reserved fields.
- Store generation and managed document ID from trusted setup.
- Strictly increasing revision and fresh commit ID.
- Previous commit digest and physical identity; revision 1 has an explicit null parent.
- Authenticated principal SID, operation ID, expected predecessor token and request
  digest. Requests bind exact document/context bytes with unambiguous length framing.
- The exact validated project bytes and explicit resource context, plus an integrity
  digest over all preceding canonical record bytes.

The [byte encoding and independent parser tests](MANAGED_RECORD_FORMAT.md) precede
the physical write probe. Fixed-width integers use explicit endianness; check every arithmetic
operation before allocation/read. No optional extensibility or generic archive.
The total record and individual payload bounds both apply. A record digest is an
integrity check, not origin or execution approval.

The returned version token binds generation, document, revision, commit ID, record
digest and observed volume/file identity. A-to-B-to-A content creates three different
revisions. Reject overflow, wrong generation, identity mismatch and stale predecessors.
Administrative snapshot rollback within a generation is unsupported. Explicit restore
requires fresh trusted provisioning/new generation before accepting old clients;
the candidate does not claim to detect hostile administrative rollback automatically.

Resource context records one root-binding ID, its selected locator and observed
identity/policy under existing root syntax limits. This is trusted operation input,
never an internal authority path supplied by project metadata. Save carries or
explicitly revises that context; a rebind changes the revision. Later explicit resource
capture must revalidate it and refuse changed identity rather than rebase silently.
No root lease survives process lifetime. Export remains create-only; callers must
explicitly retain/rebind context, without automatic resource copying or path rewriting.

## State machine and publication

1. **Acquire:** exclusively open the provisioned lock; independently validate setup
   and bounded committed chain. Hold the lock until all operations have stopped.
2. **Authorize:** authenticate outside the payload, check document access and operation
   ID. Resolve an existing receipt before evaluating a stale expected token.
3. **Prepare:** validate owned bytes/context and exact current predecessor; refuse
   conflict/capacity before creating a temporary. Allocate fallible response state.
4. **Stage:** exclusively create the fresh temporary with verified security, bounded
   writes and flush. Read/verify complete record bytes and metadata through the owned
   handle. No resource execution or dependency access follows from document validation.
5. **Publish:** one handle-relative `FileRenameInformation` to the next revision name,
   with replacement disabled. Successful namespace insertion is the candidate
   linearization point. A collision does not trigger a retry with a higher revision.
6. **Receipt:** verify the published object and return its token/receipt. A failure
   after publication is uncertain until reconciliation; it is never rolled back by
   deleting the committed record. Release per-operation handles, retaining writer lock.

The concrete native rename primitive is reused from create-only publication, but its
managed-store atomicity/concurrency claim must be physically tested. No separate
receipt/catalog write participates in commit: the committed record itself is the
receipt. Two saves from the same expected predecessor can produce at most one commit.
The loser sees a stale token; no last-writer-wins behavior is permitted.

## Reconciliation and recovery

All committed operation receipts remain in the bounded chain. Identical operation
ID plus identical authenticated request returns its original committed result;
different principal/document/expected token/bytes/context rejects. Never disclose
another principal's receipt merely because its operation ID matches.

Recovery holds the exclusive lock and validates the whole bounded chain. Only final
names count as commits. A valid temporary is never promoted automatically. Every
non-first record must match its predecessor's identity/digest/revision; the next
record cannot repair a missing/corrupt predecessor. Reject conflicting IDs, duplicates,
gaps, unexpected objects or malformed data, preserving everything for inspection.
Known staging-pattern files are bounded, validated as regular owned objects and left
in place as uncommitted; exceeding the directory budget disables further work.

Absence of a receipt is not proof while an old request can still publish. A missing
operation may be reported definitely not committed only after the previous writer
has exited, recovery has acquired the lock, and that prior execution run is fenced.
Mint a new unguessable run ID at each launch and reject submissions for old runs.
There is no cross-run pending queue. A request's execution-run ID is distinct from
its stable operation ID and request digest. Within the active run, absent/in-flight
operations remain unknown/busy unless serialization establishes their final outcome;
do not use a timeout or disconnected pipe as a cancellation certificate. Reconciliation
does not itself submit a new save. Retrying after a definite outcome still requires
explicit authorization, the current run and the expected-version check.

Claims are process-crash recovery and atomic visibility on the tested local NTFS
configuration only. Flush and namespace insertion do not establish power-loss
durability. If final-chain completeness cannot be established, return unavailable/
indeterminate and preserve evidence; never fabricate the prior or latest version.

## Predeclared physical acceptance

One consolidated write-enabled VM batch must cover:

- Fresh import, Save, reopen and exact bytes/context/token/receipt correspondence.
- Ordinary-client mutation denial on directory, lock, staging and committed objects;
  a second writer cannot replace the lock or publish concurrently.
- Same-predecessor competitors, stale token, A-B-A content, operation-ID collision,
  identical retry, wrong SID/document/generation/run and capacity refusal.
- Ordinary failures at create, partial write, flush, verification and rename; prior
  committed state unchanged and no false success. Use controlled injection where real
  disk-full exhaustion is impractical and label injected failures as such.
- Kill before staging, during partial write, after flush, before rename, immediately
  after rename and before/after reply. Include a lost reply with later commits, so
  reconciliation must find an older retained receipt, not only the current head.
- Recovery with valid orphan, partial orphan, malformed final record, gap, duplicate
  operation, altered ACL/owner/identity and bounded-scan exhaustion. No auto-deletion.
- Readers see only the complete old/new committed record through the serialized
  authority. Independent namespace/byte observations must support the linearization
  claim; a mock filesystem or eventual final hash alone is insufficient.

Record all failed/inconclusive attempts and injection barriers. The published
24-store batch passed its bounded criteria, including 64 revisions, writer/client
exclusion, injected and termination recovery, and startup corruption refusal.
The report identifies remaining criteria: authenticated Save requests, complete
wrong-document/generation and competitor coverage at that boundary, live serialized
reader visibility, full alias/reparse coverage and composition with the accepted
real RC lifecycle. Do not infer those results from the current batch.
After storage acceptance, compose the managed lifecycle
with the existing bounded RC profile and real consumption where the path changes.
Do not add service installation, UI, new engines/profiles or garbage collection.
