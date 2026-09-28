# SN-021: authenticated managed-document request candidate

Date: 2026-09-28. Status: **bounded authenticated-request candidate observed in a disposable VM**.
This extends the fixture-only [managed store](SN-021-managed-store-candidate.md)
under ADRs [0076](../decisions/0076-managed-document-saving.md) and
[0077](../decisions/0077-bounded-managed-commit-candidate.md). It does not implement
a production endpoint, installation, UI or a new runtime profile.
The [native test executable](../../tests/resources/native_managed_request_probe.cpp)
links the existing store directly; the historical v2 C# sources remain unchanged.

## First physical attempt: setup refused

The first disposable-VM attempt, `20260928T175302Z-44c71007f438`, retained its
host and guest reports. Inventory and Probe verified the staged executable and
project hashes. The guest then failed at its first `New-LocalUser` call because
the description was 53 characters and Windows limits it to 48. The parameter
binder rejected that call before account creation. The report contains no native
processes, managed roots, requests or commits. This is failed setup evidence,
not an authenticated-request result. The description was corrected to 31 characters
before the next manual attempt. Exact report and
input hashes are in the [attempt summary](evidence/SN-021-managed-request-attempt1-summary.json).

## Measured authenticated-request candidate

The fresh attempt `20260928T175932Z-937f055df11f` completed on Windows 11 build
26200 and NTFS in 7,261 ms. Its 32 source snapshots, 82 copied artifacts, inventory
and Probe reports match their retained SHA-256 values. The repeatable
[independent auditor](../../tests/resources/audit_managed_request.py) checked the
19 request artifacts and server decisions across three writer runs (15 + 2 + 2),
24 child processes, 55 observations, four physical records and the unchanged
manifest. The [measured summary](evidence/SN-021-managed-request-summary.json)
records exact hashes and bounds.

Three distinct ordinary SIDs were observed. Open, Save and Reconcile for the
authorized client returned exact owned bytes and receipts. The unauthorized SID
was refused before document dispatch. Wrong generation, document, run, magic,
length and oversized input were also refused before dispatch; stale token and
operation collision produced their specific store errors. An A-B-A edit did not
revive the old token. An older receipt survived a later commit and writer restart.
A committed Save whose reply was dropped remained uncertain at the client; an
explicit Reconcile returned its physical receipt without a second Save. An absent
active-run receipt returned only not-observed. Four record digests, request digests,
predecessor tokens, exact A/B project bytes, unchanged context and file identities
matched the retained files. The seeded manifest kept its hash, ACL and identity.

Pre-dispatch negative clients observed pipe error 233 and treated the outcome as
indeterminate; the corresponding server decisions establish the specific refusal
for this measured run. The computed `reply_status` in a server decision is not a
delivered reply. This slice does not demonstrate a terminal refusal UX, a persistent
service, installation, context rebinding, external overwrite, power-loss durability
or composition with the real RC lifecycle.

## First bounded request slice

Use one fresh, administrator-provisioned local NTFS store, seeded by the trusted
fixture with one validated document. An ordinary writer retains the existing
store lock and handles. Independently launched ordinary clients connect through
a protected Windows named pipe. Derive the principal from its identification-level
thread token, copy the SID, revert on the same thread, then authorize the operation
before calling the store. A request carries no SID, filesystem path or resource
context. Open requires the same authorization as Save and Reconcile.

Save retains the seeded resource context and persists exact owned document bytes.
This slice refuses rebinding rather than letting document metadata grant access
to a root. Declarative validation is reused; it neither captures resources nor
establishes compatibility, execution permission, origin or redistribution rights.
No engine, DLL, model, firmware or renderer is opened through these operations.

## Private wire version 3

The historical fixed 64-byte query-only v2 probe remains unchanged. This separate
experiment has one bounded request per connection. Integers are little-endian and
IDs are raw bytes. The 32-byte header contains ASCII `SN21SAVE`, version u16 = 3,
operation u16, body length u32 and correlation ID16. Recognized operations are
1 Open, 2 Save and 3 Reconcile. Correlation grants no authority.

All requests start their body with run ID16, generation ID16 and document ID16.
Save/Reconcile add operation ID16, expected token108, project length u32 and exact
project bytes, limited to the existing 1 MiB. They contain no extensible fields.
The token is generation16, document16, revision u32, commit16, digest32, volume u64
and file ID16. Exact message and declared lengths must agree before dispatch;
the total receive allocation is bounded independently of input.

Replies echo the validated operation with bit 0x8000 and correlation. Their body
starts with status u32: 0 success, 1 rejection, 2 busy, 3 indeterminate or
4 receipt not observed. Successful Open adds token108, context length u32,
canonical context bytes, project length u32 and owned project bytes. Successful
Save/Reconcile add operation ID16, token108 and request digest32. Other statuses
contain only their four-byte status; no detailed denial or object handle escapes.
Not observed is not a cancellation certificate or definite not-committed outcome.
The server report's `reply_status` records a computed decision even when no reply
was sent. Delivery requires the separate client receipt and terminal exchange.

Reuse the measured acknowledgement/terminal/closure rule: client ack 0x7e, server
terminal 0x7f, client ack 0x7f, then clean closure. A client accepts only after that
sequence. Losing the reply after submission remains uncertain; no automatic retry.
One server admits at most 32 connections, with a monotonic five-second connected
pipe-I/O deadline and observed completion after I/O cancellation. The existing
synchronous store calls are not preempted by this deadline; a stalled filesystem
can exceed it, leaving the result uncertain rather than permitting another Save.
The VM coordinator retains a timed-out process for inspection. No production
operation-latency guarantee follows. Pipe owner and exact DACL
are verified by handle at both endpoints. No weaker endpoint fallback is allowed.

## Predeclared acceptance and remaining gates

The [guest coordinator](../../tests/resources/windows_managed_request.ps1) must
record specific server decisions and independently preserve the committed files.

| Case | Required evidence |
|---|---|
| Authorized Open/Save/Reconcile in distinct client processes | Real authenticated SID, reversion before dispatch, exact bounded result |
| Admitted but unauthorized principal | Principal refusal before document dispatch; prior records unchanged |
| Wrong generation, document or run | Corresponding refusal, including old run after restart |
| Malformed magic/length and oversized message | Specific bounded framing refusal; no publication |
| Stale token and A-B-A | Old token never becomes current again; only fresh expected token commits |
| Reused operation ID | Identical request returns its original receipt; changed bytes reject |
| Older receipt after a later commit and restart | Authenticated reconciliation returns the original complete result |
| Dropped committed reply | Client remains uncertain; explicit Reconcile returns the receipt without repeating Save |
| Absent active-run receipt | Only not observed; no definite cancellation/not-committed claim |
| Preservation | Exact prior record hashes/security, source unchanged, artifacts and negative results retained |

Local parsing/compilation checks support preparation only. Real cross-identity
pipe/NTFS execution in the disposable VM is required for this acceptance. A native
implementation is not automatically covered by the historical C# v2 measurements.
Competing-request observations and the applicable endpoint,
identity and deadline regressions must also be assessed before broader acceptance.

The measured batch used 19 requests in three writer runs (15, 2 and 2), one managed
document and four revisions. Existing new-record handles stay exclusive. Live
independent snapshots therefore check names, sizes, identities and security only;
full record hashes and token/receipt correspondence are checked after writer exit.
This preserves the accepted handle-sharing contract instead of weakening it for a
test oracle. Initial Open independently matches the exact seeded context/project.

Local preparation passed the full MSVC static-runtime Release build, all 55 CTests,
450 adversarial parser checks, the exact 1,048,784-byte Win32 pipe transport and
specific rejection of a 1,048,785-byte message. PowerShell parsing/framing, the
physical file-identity oracle, concurrent complete-JSONL reading, host-only VIX
preflight and the repository checker (810 text files) also passed. An earlier
local transport failure and its corrected rerun remain retained separately. These
checks used the same local identity and neither authenticate a distinct client nor
prove managed publication through IPC.

The host helper prompts for credentials without saving them, verifies the existing
baseline snapshot and retains each attempt separately. Inventory and Probe compare
the two executable hashes and source-project hash against retained host copies
before account provisioning; Probe also checks its private execution copies.
Failure returns a nonzero exit code. If the VIX Probe call fails, one read-only
copy of its existing guest JSON is attempted as raw diagnostics. It performs no
automatic retry, cleanup, snapshot restore or service installation. Do not run
provisioning on the host. The physical result above is limited to the retained run.

SN-021 remains in_progress. After the request gate, compose managed import/open/
edit/Save/reopen/export with the accepted real RC lifecycle and audit bounded
acceptance. Preserve resource-root meaning, numerical tolerances, SN-017 Python
control, SN-044/SN-045 work and all historical evidence. The final next-chat prompt
is due only after actual SN-021 completion.
