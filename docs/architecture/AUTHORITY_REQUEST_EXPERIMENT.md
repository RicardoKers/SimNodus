# SN-021 authority request experiment contract

## Measured prerequisite, not full implementation

The [pipe identity preflight](../experiments/SN-021-pipe-identity.md) on 2026-09-23
observed five real-token cases without granting SeImpersonatePrivilege. It uses a
one-byte marker and no store access. The 64-byte query, protected startup checks
and the remaining rejection matrix below are still unimplemented/unmeasured.

Date: 2026-09-23. Status: **specified, not implemented or physically validated**.
This is the next bounded experiment under the [publication authority proposal](PUBLICATION_AUTHORITY_PROPOSAL.md),
following the [filesystem observations](../experiments/SN-021-authority-filesystem.md).
It selects no production IPC, storage architecture or new completion criterion.
ADR 0064, create-only publication and the exclusion of TxF remain in force.

## Question and scope

Can an ordinary writer process in the disposable Windows VM authenticate a real
ordinary client, authorize a fixed fixture document, and reject other principals
and malformed requests without exposing filesystem authority? Measure that before
introducing a save operation, commit layout or recovery algorithm.

Use a manually launched writer, one allowed client and one distinct disallowed
ordinary account. Two independent processes under the allowed client's SID test
equivalent access; process identity must not be mistaken for a separate user.
Administrative setup is limited to the disposable VM and fresh owned fixtures.
No service, host ACL change, installation, download, UI or engine is required.
Preserve previous fixtures, snapshots, reports and negative results.

Only `InspectFixture` is recognized. It returns fixed, bounded metadata from the
validated fixture, never project bytes, credentials, a native handle or a path.
`Save`, import, export, arbitrary read and all unknown operation codes reject.
This does not test save retries, ABA handling or power-loss durability.

## Trusted setup and startup

Administrative setup records the writer SID, allowed client SID, random run ID,
opaque fixture document ID and expected root/file volume and file identities in
a manifest protected from ordinary-client modification. The manifest is fixture
setup data, never supplied by a project or request. No passwords enter it. Its
format and size must be fixed and bounded in the implementation before execution.
Do not adopt an existing client-writable directory or import external resources.

Every writer launch independently checks its ordinary token, manifest owner/DACL,
store ancestors and every fixture/metadata object it will use. Compare exact allowed
rights masks and inheritance as well as SIDs; matching a list of principals alone
is insufficient. Refuse unexpected ownership, writable ancestors, aliases, reparse
points, extra hardlinks, missing objects or identity/byte differences. Retain the
validated handles through the experiment. The existing handle-relative resource
contracts remain the reference; textual prefix checks and resolve-then-open are
not substitutes. This requirement is still unimplemented for the new experiment.

The writer must finish validation before publishing an endpoint or a ready result.
A second writer must fail to acquire the same endpoint while the first is live.
After termination and a fresh launch, repeat all checks; never trust a prior
process's cached success. Inject setup corruption only into separate disposable
negative fixtures, with precise before/after records. No repair or deletion on
validation failure. These checks test startup refusal, not crash recovery.

## Proposed local transport and identity

A Windows named pipe is the experiment candidate. Use a protected DACL supplied
at creation, `PIPE_REJECT_REMOTE_CLIENTS`, one server instance, and
`FILE_FLAG_FIRST_PIPE_INSTANCE`. If the name is occupied, fail without connecting
to the occupant or choosing a weaker fallback. A random name reduces collisions;
it is not authentication. Use overlapped operations with explicit deadlines.

Grant clients only the individual read/write-data, read-attributes, READ_CONTROL
and SYNCHRONIZE rights actually required by the implementation. Never grant them
FILE_CREATE_PIPE_INSTANCE, WRITE_DAC, WRITE_OWNER or DELETE. Do not use a default
pipe descriptor or client GENERIC_WRITE permission: its mapped append bit also
permits creating a pipe instance. Verify the resulting descriptor by handle.
These details follow [Microsoft's named-pipe security contract](https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-security-and-access-rights)
and [CreateNamedPipe](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createnamedpipew).

The client verifies the opened pipe's owner and complete DACL against the trusted
setup writer identity before sending even the fixture request. A malicious server
owned by another ordinary SID must be rejected. The writer identity and its code
are trusted in this experiment; compromise of that identity, administrators and
kernel authority remain outside the claim. Server PID or pipe name alone is not
authentication. No filesystem or process handle is exchanged through the protocol.

Read one bounded message, then use `ImpersonateNamedPipeClient` on that connection
and query the resulting thread token's TokenUser and impersonation level. The
client opens with explicit SECURITY_SQOS_PRESENT and SECURITY_IDENTIFICATION;
anonymous identity fails. No SeImpersonatePrivilege grant is proposed. The API
documents identification-level operation separately from full impersonation; its
success under the selected ordinary tokens must be physically measured, not assumed.
Reject failure instead of continuing under the writer's own identity.
[ImpersonateNamedPipeClient](https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-impersonatenamedpipeclient).

Copy the verified SID into bounded owned data, then revert on the same thread
before dispatch. Verify return to the writer context. A failed RevertToSelf ends
the experiment process; it must not continue serving. No await/thread handoff,
filesystem access or request handler runs while impersonating. This follows
[RevertToSelf](https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-reverttoself).
The thread token is closed locally and never serialized or duplicated to a client.

Authorization is the conjunction of verified client SID, the setup mapping for
the opaque document ID, and the allowed operation. Connection permission is only
the first gate. Run a separate negative fixture that admits the disallowed SID to
the pipe but excludes it from the document mapping, to test handler authorization.
Any SID, PID, writer claim, path or version in a message is untrusted data.
An opaque ID or matching digest grants no authority.

## Bounded wire contract for this probe

Use message mode with one request and one response per connection. A request is
exactly 64 bytes. Integers are little-endian; 16-byte IDs are raw bytes with no
GUID text/endian conversion. This is a private experiment format, not a project
schema revision or a published application API.

| Offset | Bytes | Meaning |
|---|---|---|
| 0 | 8 | ASCII `SN21IPC1` |
| 8 | 2 | Protocol version 1 |
| 10 | 2 | Operation 1 = InspectFixture |
| 12 | 4 | Body length, exactly 32 |
| 16 | 16 | Correlation ID; never an authorization or retry token |
| 32 | 16 | Opaque document ID |
| 48 | 16 | Run ID from trusted setup; never a document version |

Use a fixed 65-byte receive buffer to detect an oversized message; reject either
excess bytes or ERROR_MORE_DATA. Do not allocate from the declared length. Reject
short/truncated input, wrong magic/version/length/run ID, unknown operation or
document, and extra messages. There is no textual path field or extensible JSON
object into which a caller can insert one. Invalid and unauthorized requests return
a generic rejection or disconnect, with detailed reasons only in trusted reports.

A successful response is exactly 64 bytes: the same 32-byte header with operation
0x8001 and body length 32, followed by the fixture SHA-256. The digest is captured
through startup-retained handles and must be verified before serving. No request
changes fixture bytes. Denials need not echo any attacker-provided correlation ID.
Clients must also bound and validate the response and reject duplicates/extra data.

Proposed test limits: one connection at a time, at most 32 connections per writer
run, 5 seconds total per connection (including response), and 180 seconds per run.
Use a monotonic deadline; dribbling bytes must not extend it. On expiration cancel
pending I/O and observe completion before releasing handles/buffers. Do not rely
on CreateNamedPipe's default timeout as a read deadline or block on an unbounded
flush. The implementation must record actual elapsed times and cancellation state.
A disconnect cannot cause a mutation or an automatic retry.

## Required physical evidence before accepting this slice

| Case | Required observation |
|---|---|
| Allowed client and second same-SID process | Correct bounded reply, verified SID, unchanged fixture |
| Disallowed SID | Pipe denial; separately, handler denial when connection is deliberately allowed |
| Forged writer/client claim | No change in authenticated SID or mapping decision |
| Anonymous identification / impersonation failure | No handler dispatch and no writer-context fallback |
| Malformed, oversized, wrong-ID/run, path-shaped and Save requests | Reject without allocation from input or fixture access by supplied path |
| Slow sender, disconnect and extra messages | Bounded completion; no mutation and no leaked I/O |
| Endpoint squatting / second server | Writer refuses occupied name; client rejects wrong pipe owner/rights |
| Stop and fresh start | Independent startup validation; new run ID rejects prior-run requests |
| Wrong owner/rights, missing file, alias/reparse/hardlink or changed identity/bytes | Refusal before endpoint readiness, original good fixture untouched |
| Preservation | Exact source/report hashes; all original fixture and historical hashes retained |

Capture process IDs, real token SIDs/groups/privileges, pipe/store descriptors,
request byte count/class, authenticated decision, timings, response bounds and
before/after fixture identity/security/bytes. Never log secrets or infer successful
rejection from an unrelated harness exception. Keep every failed/inconclusive run.
Synthetic parser tests can support this evidence but cannot replace the VM cases.

Passing this slice would provide limited client-boundary/startup evidence only.
Do not infer production compatibility, origin, redistribution permission, execution
authorization, a durable commit protocol or arbitrary-directory overwrite. Update
the six-gate assessment with actual measured coverage before proceeding further.
