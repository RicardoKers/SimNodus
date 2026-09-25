# SN-021: physical managed-store candidate

Date: 2026-09-25. Status: the bounded disposable-VM batch passed; broader acceptance
remains pending. This is an experimental portion of
[ADR 0077](../decisions/0077-bounded-managed-commit-candidate.md), not production
saving or SN-021 completion.

## Final measured batch

Run `20260925T163734Z-0bddcba4bab4` completed in 20,996 ms with 127 observations
across 24 fresh stores on Windows 11 Enterprise Evaluation 26200 / NTFS. The
[summary](evidence/SN-021-managed-store-summary.json) indexes all nine attempts,
including credential/snapshot failures, timeout, partial results and final success.
The [final raw report](evidence/SN-021-managed-store-attempt9-probe.json) records
identities, security descriptors, bytes/digests, child exit codes and injected
boundaries. [Source archive](evidence/SN-021-managed-store-sources.json) maps exact
UTF-8 source bytes by SHA-256 for every attempt; executable hashes are recorded,
but executable bytes are not published.

| Observation | Result |
|---|---|
| Revision chain / independent record digests | 64 revisions / 64 matching digests |
| Client mutation attempts, live/stopped/restarted writer | 27 denied per state |
| Lock mutation / competing writer | 6 denied / exclusive lock refusal |
| Injected failure recovery | 8 boundaries, expected old/new revision |
| Forced process termination recovery | 6 boundaries, expected old/new revision |
| Lost reply with a later commit | Older revision-2 receipt reconciled |
| Startup corruption/security/namespace cases | All 8 refused; injected state preserved |

The earlier pending notes below describe the preparation and failed attempts, not
the final batch status. The result remains `observed-managed-store-candidate-only`.
It does not cover authenticated Save transport, wrong document/generation at that
transport boundary, a complete junction/case-alias matrix, live serialized-reader
observations, real disk exhaustion, power loss or production deployment. Full ADR
0077 acceptance requires closing the applicable gaps; the candidate alone does not
close SN-021. Existing tests do not substitute for missing physical measurements.

## Scope and acceptance boundary

The [Windows adapter](../../src/platform/windows/managed_store.cpp) combines the
inert record/chain modules with retained filesystem handles. Its fixture-only root
is a direct child named `C:\SN021Managed-<12 lowercase hexadecimal characters>`.
It opens the actual local NTFS volume, then opens children relative to retained
directory handles. Textual prefix comparison and resolve-then-open are not proofs
of containment. Object identities, exact names, link counts, reparse/case-sensitive
flags, ownership and protected ACLs are checked independently of record hashes.

The writer must be an ordinary non-elevated principal without administrative group
membership or dangerous token privileges, including disabled backup/restore,
ownership, impersonation, debug and token-creation privileges. Setup objects are
administrator-owned; records are writer-owned. Each protected DACL has exactly
three non-inherited allow entries: SYSTEM and Administrators with `0x1f01ff`, and
the writer with the following mask:

| Object | Writer mask |
|---|---|
| Root directory | `0x1200a9` |
| Document directory | `0x1200ab` |
| Manifest and exclusive writer lock | `0x120089` |
| Staging and committed records | `0x13019f` |

The administrator-provisioned manifest binds generation/document identifiers,
root/document/lock physical identities and writer/client binary SIDs. Namespace
acquisition is limited to 128 entries across root and document, excluding dot
entries. Records remain bounded to 2 MiB and the chain to 64 revisions. Orphan
staging objects are retained and never promoted by name. Each operation audits the
complete namespace, retained identities/security and committed bytes. A fresh run
identifier rejects requests for a previous instance; a missing receipt remains
only not observed. Trusted-writer compromise and administrative rollback are outside
this candidate's authority model.

Publication creates a fresh staging object, writes bounded chunks, flushes and
verifies the retained object, prepares the next chain/receipt, then performs a
handle-relative rename with replacement disabled. Failures after publication are
indeterminate. No deletion, pruning or automatic repair occurs. Flush and process
termination tests do not establish power-loss durability. Windows API contracts:
[relative creation](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-ntcreatefile),
[rename](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-ntsetinformationfile),
[handle enumeration](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getfileinformationbyhandleex).

The supplied principal is fixture input, not authenticated save IPC. Valid metadata,
physical containment and matching bytes do not establish interface compatibility,
execution permission, origin or redistribution rights. No project resource is
executed, rendered, downloaded or loaded as a library by this candidate.

## Prepared physical batch

The [native probe](../../tests/resources/native_managed_store_probe.cpp) and
[VM coordinator](../../tests/resources/windows_managed_store.ps1) create two fresh
ordinary guest accounts and 24 fresh stores. They install no service and perform no
automatic retry, cleanup or snapshot revert. Planned observations are:

- Sixty-four physical revisions, A-B-A, stale/conflicting operations, capacity,
  old receipts and restart/run fencing.
- Client mutation exclusion with a live, stopped and restarted writer, lock
  mutation refusal and exclusion of a second writer.
- Eight explicitly injected failure boundaries and six held-process termination
  boundaries, including publication before receipt delivery and later reconciliation.
- Startup rejection of corrupt bytes, gaps, rehashed duplicate operations, altered
  ACL/owner, hard links, unknown entries and an excessive namespace.

The coordinator retains exact process handles before termination. Independent
snapshots record object identities, ACLs and bytes; snapshots while data handles
are held are metadata-only. Negative fixtures and orphan files are preserved.
Injected I/O failures do not demonstrate real disk exhaustion. This batch does not
claim a full junction/case-alias matrix, concurrent authenticated save requests or
complete ADR 0077 acceptance. Those claims require their own applicable evidence.

## Local verification and remaining work

The first VM attempt, `20260925T160420Z-3c45c8f98f3f`, failed at initial store
opening after successful provisioning, before the first revision. The ordinary
writer returned `child-open`, NTSTATUS `0xc000000d` (`STATUS_INVALID_PARAMETER`).
Its original source, binary and reports remain untouched under
`build/sn021-vm-managed-store-runs/20260925T160420Z-3c45c8f98f3f`.

A read-only local eight-case NT parameter diagnostic reproduced this result:
`FILE_DIRECTORY_FILE | FILE_OPEN_NO_RECALL` failed for both directory and file;
the no-recall/reparse/synchronous combination without `FILE_DIRECTORY_FILE`
opened both, and explicit non-directory mode rejected the directory. The adapter
now follows existing resource ingress: retain no-recall/reparse protection and
verify the actual type on the opened handle before enumeration or traversal.
The diagnostic and JSON are retained as `build/sn021-managed-open-options.py`
and `build/sn021-managed-open-options.json`. Both adapter configurations were
rebuilt successfully. A new VM attempt is required; no physical pass is inferred
from this correction. The original failed attempt is not replaced by a retry.

Attempts `20260925T160755Z-aa52bdc43d16` and `20260925T160818Z-45de3a9e186b`
stopped at `open-encrypted-vm` with VIX error 17005. The installed VIX library's
`Vix_GetErrorText` maps this code to `Incorrect password`. Neither attempt reached
guest login or executed the physical probe. Their reports and source/binary
snapshots remain retained; they provide no result for the corrected adapter.

Attempts `20260925T161020Z-1c635c60f965` and `20260925T161048Z-6cc8e81c8ae5`
stopped at snapshot lookup (VIX 13003) before provisioning because the supplied
snapshot name differed from the baseline. Attempt `20260925T161154Z-7e69dfdd2a43`
timed out in VIX during the probe. Read-only collection
`c7e2a73cf6c04764aeb3da1ca4e33553` retained the guest report and native output under
`build/sn021-managed-store-diagnostics`. The native probe opened revision zero,
then reported `publish`, NTSTATUS `0xc0000043` (sharing violation), followed by
`fixture-failed`; the coordinator report still said `started`. These files do not
prove that every guest process exited. No process was terminated or retried by
the collector.

A fresh local two-case NT rename experiment reproduced the sharing violation
with read-only directory sharing, and succeeded with read/write sharing. Fixtures
and results remain in `build/sn021-rename-sharing-750edf743b9a4eb39226bbb1f3c3de5c`.
The candidate now permits write sharing on the document directory, not delete
sharing. The protected ACL excludes clients; the nonreplaceable no-share lock
serializes trusted writers. Sharing flags alone are not publication authority.
This changed configuration needs its own physical isolation evidence.

The coordinator now supplies an explicit report path to the native probe instead
of redirecting its streams through PowerShell. The native process writes reports
directly. The timeout's precise coordinator cause remains unproven; avoiding pipe
capture removes a suspected blocking path without increasing deadlines or hiding
failures. A local hidden-process launch, direct report and bounded exit wait passed;
cross-identity VM execution remains pending. Release/Debug builds, script parsing,
runner compilation and repository hygiene passed. Original run snapshots remain
unchanged; subsequent runs use distinct accounts and store roots.

Attempt `20260925T162357Z-bcd1268ffdad` completed the physical 64-revision
sequence, A-B-A/capacity/old-receipt/run-fencing assertions and independent digest
checks for all 64 records. It then failed in `write-enabled-isolation`: PowerShell
could not read the held probe's JSONL report because the CRT report stream denied
concurrent reading. No isolation or interruption acceptance follows from the
completed sequence. Its source, binary, snapshots and negative outcome are retained.

The manual probe now creates report files with explicit read sharing and exclusive
creation, transferring owned handles to its output streams. Store handles and ACLs
are unchanged. An inert `report-check` mode allows local verification of concurrent
PowerShell reading while the native process remains active; that test and bounded
normal exit passed. The Release probe rebuilt successfully. Physical isolation,
interruption and corruption stages still require a new VM run.

Attempt `20260925T163359Z-f2297b44067b` completed sequence, live/stopped/restarted
client exclusion (27 denials each), six lock mutation denials, second-writer
exclusion, all eight injected-failure recoveries, all six process-termination
recoveries and reconciliation of the older lost-reply receipt. Corrupt-record and
gap startup rejection also passed. The batch then failed while constructing the
duplicate-operation fixture: PowerShell parsed `@(236,$bytes.Length-268)` as an
array subtraction. Parenthesizing the length expression fixes the coordinator.
The exact corrected mutation block was run locally on inert bytes, independently
checking operation replacement, request SHA-256 and record SHA-256 in Python.
Artifacts remain in `build/sn021-operation-injection-b567fb4ce11449729eeacf265a85eed5`.
No adapter/binary change was required. Preserve this partial result and all prior
attempts; remaining startup corruption cases and whole-batch acceptance are pending.

The Release probe uses MSVC 19.51.36246.0, Windows SDK 10.0.26100.0, C++20, strict
warnings and the static runtime. Its imports are Windows system libraries only.
The ordinary adapter is also compiled without test hooks. The final 53 CTest
tests passed in 58.31 seconds; they do not establish physical store acceptance.
The repository checker passed on 796 workspace text files (including two untracked
SN-045 documents); diff whitespace checks passed. Hosted checks gate integration.
Local invalid-root rejection, coordinator syntax, Python runner compilation and
read-only VIX host preflight were checked before the recorded guest runs.

Run the prepared local `build/sn021-vm-managed-store-run.py` helper with
`--existing-snapshot SN021-authority-baseline`, entering credentials only through
its hidden prompts. Each attempt retains source/binary hashes and raw reports.
Success is labeled `observed-managed-store-candidate-only`; inspect the actual
observations before accepting or publishing this block.

Authenticated save transport and composition with the accepted real RC lifecycle
remain pending. Existing tolerances, project bounds, resource-root meaning and
runtime profiles are unchanged. External overwrite remains unsupported. The
candidate is prepared on `codex/sn-021-managed-store`, based on `a027b46`.
The pull request records required checks and resulting integration identities.
