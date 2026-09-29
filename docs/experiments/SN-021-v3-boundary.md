# SN-021: bounded v3 request-boundary matrix

Date: 2026-09-29. Status: **bounded v3 physical matrix and independent byte audit passed; final bounded SN-021 acceptance audited**.

This is the smallest remaining request-boundary test for the private v3 managed
Open/Save/Reconcile fixture after the [managed RC lifecycle](SN-021-managed-lifecycle.md)
passed. It does not install a service, change the accepted project/runtime
profile, overwrite an external file or authorize project execution. Earlier
[v2 endpoint](SN-021-endpoint-restart.md) and
[v2 request-matrix](SN-021-request-matrix.md) evidence informs the cases but
cannot establish native v3 behavior by itself. The prior
[v3 request batch](SN-021-managed-request.md) already measured 19 authenticated
sequential requests, stale/A-B-A, operation collision, lost-reply reconciliation,
SID identification/reversion and exact persisted records. Retain that evidence
without relabeling it as a concurrent or hostile-endpoint test.

## Acceptance before execution

Use a fresh, protected local NTFS managed store in the disposable Windows VM,
with distinct administrator/provisioner, writer, allowed client and unauthorized
client identities. Snapshot existence, exact staged source/binary hashes, ACLs,
physical file IDs, raw pipe frames, complete child reports and record hashes
must be retained. No automatic retry of a submitted Save, cleanup, snapshot
revert or service installation is allowed. A bounded wait for a pipe instance
before sending any bytes may be used to ensure both competing clients reach the
server; it is not an operation retry.

| Case | Required physical outcome |
|---|---|
| Competing Save | Two independent allowed clients submit different operation IDs and project bytes against the same expected token while their lifetimes overlap. Exactly one authenticated Save commits, the other receives stale-version refusal, and exactly one new complete record is independently matched to the winner. The predecessor, manifest and external source remain unchanged. |
| Wrong-owner listener | The allowed client rejects the pipe owner before sending request bytes; the listener observes zero request bytes. Retain the handle's actual owner and DACL. |
| Wrong-rights listener | A same-writer-SID pipe with a changed client ACE is rejected by handle before request bytes; the listener observes zero request bytes. Retain the handle's actual owner and ACE masks. |
| Occupied pipe name | The first listener holds its name until the second writer has failed before readiness, then receives an explicit release signal. No client is launched against this occupied name; the case tests writer readiness refusal only. |
| Committed Save, closed terminal | A real Save publishes one complete record, then the terminal exchange closes before the success marker. Retain the received reply bytes; the client does not report success. An explicit authenticated Reconcile returns the same physical receipt without repeating Save. |
| Committed Save, stalled terminal | A second real Save publishes one complete record, then the terminal marker stalls through the deadline. Retain the received reply bytes; the client remains uncertain. Reconcile returns that receipt without repeating Save. Pending-I/O cancellation must complete before overlapped storage is released. |
| Silent connected client | A client connects but sends no message until the single monotonic deadline expires. The writer refuses without dispatch or publication and observes pending-read cancellation completion. This case does not submit a short complete frame. |

Keep the same five-second connected pipe-I/O deadline and the documented
limitation that synchronous store calls are not preempted. The client may report
an uncertain outcome after sending bytes; a computed server decision is not a
delivered reply. Hostile response injection under the trusted writer SID does
not prove a principal bypassed endpoint authentication. Exact request/response
and record bytes must be independently audited before promoting a VM report to
acceptance. If a case cannot establish its predeclared physical oracle, record
it as failed or inconclusive; do not infer success from a process exit alone.

Short complete-frame refusal is a separate framing property: local v3 parser
tests reject truncated Open prefixes and the earlier physical v2 matrix
measured a one-byte message. The present VM batch does not physically establish
that result for v3.

This matrix can close only the remaining applicable v3 request-boundary gates.
It does not prove power-loss durability, malicious administrator resistance,
production installation, general runtime readiness, trusted origin or permission
to redistribute dependencies. A final bounded SN-021 audit is still required.

## Historical local preparation before the physical runs

The test-only native probe adds a pre-send gated pipe wait, owner/rights/occupied
listener injections, an explicit occupied-name release gate, two post-commit
terminal injections and a silent client. The listener reports the actual handle
descriptor, and a validated reply is retained even if the terminal exchange
fails; neither a reply file nor a computed server decision implies client success.
The full 56 Release CTests passed after the first native change, and the two
native request tests passed again after the final listener changes. The guest
PowerShell parses. The repository checker passes on 824 text files. The VIX
host-only preflight connects, and a local-only preparation invoked from
`C:\Windows\System32` retained and verified 34 source snapshots in
`build/sn021-vm-v3-boundary-runs/20260929T104700Z-85f4ea43ce5b`. This is not
a guest result. The independent inert-byte auditor expects 19 child processes,
60 exact artifacts and four records; at that preparation point it had no
physical result to audit.
The earlier ngspice result is unchanged; this request-only matrix does not
start an engine.

## First physical attempt: endpoint diagnostic refused

The first manual VM run `20260929T113518Z-0699d0688ebd` reached the
occupied-name endpoint case, then stopped with `PROBE:occupied-endpoint-exposed`.
The second writer exited with `pipe-create` and did not expose its ready file,
but the probe reported `system: 0`: its `need` helper discarded the Windows
error from the failed `CreateNamedPipeW`. The exact error cannot be recovered
from this attempt. The guest report had already recorded a seed and one of two
competing Saves at revision 2, with the other request refused; final record
bytes were not copied before the abort, so the independent complete audit does
not promote these partial observations. Terminal and silent-client cases did
not run. The first listener's explicit release was not reached; its eventual
exit and the VM's later state are not inferred.

The [failed-attempt summary](evidence/SN-021-v3-boundary-attempt1-summary.json)
records exact raw result/report hashes, 34 verified source snapshots, 27 copied
artifact hashes and the 11 launched child processes. The original ignored run
directory is retained unchanged. No automatic retry, cleanup or snapshot revert
occurred.

The test-only probe now captures `GetLastError` immediately after
`CreateNamedPipeW` on an invalid handle. A same-identity local transport check
measured duplicate-name refusal with Windows error 231 and verifies that the
error remains nonzero; both native request CTests pass. This local check does
not substitute for a fresh VM run. A new local-only preparation from
`C:\Windows\System32` verified 34 corrected source/binary snapshots in
`build/sn021-vm-v3-boundary-runs/20260929T113926Z-0d491b89ac8b`. At that
point the physical v3 boundary and complete byte audit remained pending.

## Second physical attempt: cancellation observed, collection failed

The fresh manual VM run `20260929T121744Z-138f3ba362db` passed the occupied
pipe-name case: the second writer returned Windows error 231 while the first
listener was alive, no second ready file appeared, and the listener observed
the explicit release. The closed-terminal Save remained uncertain to its
client and an authenticated Reconcile reported revision 3. The stalled-terminal
client then recorded pending-I/O cancellation with `cancel_error: 0` and
completion 995, but the coordinator failed its `terminal-cancellation` check.
It looked for `terminal-stall-save.jsonl` while `Start-Child` had written
`16-terminal-stall-save.jsonl`. This is a report lookup error, not a missing
client cancellation. The stalled Save's Reconcile and silent-client case did
not run, and final raw record bytes were not copied.

The [second failed-attempt summary](evidence/SN-021-v3-boundary-attempt2-summary.json)
records exact host/inventory/guest hashes, all 34 source snapshots, 44 copied
artifact hashes and 16 launched processes. The raw run remains unchanged and
does not establish complete boundary acceptance. The coordinator now reads
the cancellation event from the exact child observation retained by `Finish`,
avoiding a reconstructed output path. A fresh VM run and independent complete
byte audit were required at that point. Local-only preparation after this fix verified
34 current source/binary snapshots from `C:\Windows\System32` in
`build/sn021-vm-v3-boundary-runs/20260929T221817Z-a86c377d5b60`. The exact
guest script digest is
`87d0a832c12e1d80e39a09197c709ee6e9d2212d652e9d90b1170d6800ee4d85`.

## Complete physical candidate and independent audit

The fresh manual VM run `20260929T222514Z-a45b647a5b8d` completed all cases
in 16,457 ms. The host retained 34 exact source snapshots and 60 copied
artifacts from 19 child processes. The separate
[byte audit](../../tests/resources/audit_managed_v3_boundary.py) passed against
the retained raw host and guest reports, frames, pipe descriptors, replies,
manifest, four complete records and their file identities. The
[success summary](evidence/SN-021-v3-boundary-success-summary.json) records
their SHA-256 values and the auditor-result hash. Both earlier failed runs
remain separate, unchanged negative evidence.

Two allowed clients submitted distinct Save operations and project bytes
against one token. Exactly one committed revision 2; the other received the
stale-version refusal. The audited final records match the winning bytes and
preserve the predecessor and manifest. Wrong-owner and changed-rights
listeners were rejected before request bytes; their actual handle owners and
ACE masks were retained. A second writer failed to create the occupied name
with Windows error 231 while the first listener was alive, then the explicit
release was observed. The occupied-name case measures writer readiness
refusal; it launches no client against that name.

A real Save committed revision 3 before its terminal marker was closed, and
another committed revision 4 before the terminal marker stalled past the
deadline. Neither client reported Save success. Their received reply bytes
matched the committed records, and explicit authenticated Reconcile returned
the exact respective receipts without repeating Save. The stalled client's
pending read was cancelled and completed with Windows code 995. A final
connected client sent no message; the writer cancelled its pending read at
the deadline, refused before dispatch and published no fifth record.

This closes the predeclared request-boundary matrix only within the private
test fixture and measured Windows/NTFS configuration. It does not establish a
production endpoint, installation, power-loss durability, external overwrite,
general simulation readiness, trusted origin or redistribution rights. The
final bounded [SN-021 acceptance audit](SN-021-final-acceptance.md) composes this
evidence with the earlier managed lifecycle and real E-01 replay.

An operator can run the ignored local runner interactively with
`python build/sn021-vm-v3-boundary-run.py --existing-snapshot SN021-authority-baseline`.
It prompts for the VMware and Windows guest credentials without writing them.
Retain every result, including failure or indeterminacy, without automatic
retry or snapshot revert. Only after a complete physical candidate, run
`python tests/resources/audit_managed_v3_boundary.py <run-directory> --output <fresh-summary-path>`.
The audit must verify the exact guest report, 34 source snapshots, all copied
artifact bytes, authenticated server decisions, complete records and every
negative case before this matrix can be accepted.
