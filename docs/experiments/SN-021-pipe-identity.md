# SN-021: pipe identity prerequisite

Date: 2026-09-23. Status: **five physical cases observed; prerequisite only**.
This supports the [request experiment](../architecture/AUTHORITY_REQUEST_EXPERIMENT.md)
without implementing its 64-byte document query or startup validation. No production
IPC, save API, service or architecture is selected. SN-021 remains in progress.

## Scope and method

The owner ran the [embedded guest probe](../../tests/resources/windows_pipe_identity.ps1)
in the same disposable VMware Windows 11 Enterprise Evaluation build 26200 VM,
on NTFS. VIX located the operator-created baseline snapshot before provisioning;
no snapshot restoration occurred. Administrative setup created three fresh ordinary
accounts and a protected harness directory. Previous fixtures were left in place.
Measured server and client operations ran under ordinary tokens without the
enumerated bypass privileges, including SeImpersonatePrivilege.

This is a one-byte identity preflight, not InspectFixture. No document/store is
opened or changed. An explicit named-pipe descriptor grants the writer full rights
and clients only mask 0x120083. Both endpoints check owner and exact ACE masks.
The server uses first-instance creation and rejects remote clients. These options
are implemented here, but their hostile endpoint/remote rejection is not measured.

The client explicitly requests SecurityIdentification (or anonymous for one negative
case). After reading the fixed marker, the server impersonates on the same thread,
queries the actual TokenUser and identification level, then reverts. It verifies
absence of a remaining thread token before dispatch. Authorization compares that
verified SID with the setup's allowed SID. The server replies with a fixed allowed
or denied marker and waits for acknowledgement so close does not discard a reply.
Neither SID claims nor credentials appear in the message.

## Measured cases

| Case | Server observation | Client observation |
|---|---|---|
| Allowed process 1 | Identified allowed SID at level 1; reverted; authorized | Received allowed reply |
| Allowed process 2 | Same allowed SID from an independent process; reverted | Received allowed reply |
| Handler denial | Pipe admitted third SID; server identified it, reverted and denied | Received denied reply |
| Anonymous | Token query failed with native 1347; reverted; no dispatch | Broken pipe 109; no reply |
| Pipe denial | No client connection; bounded timeout after 15011 ms | Open denied with native 5 |

The writer, allowed client and disallowed client SIDs were distinct. The two allowed
client PIDs were 6520 and 2508; separate servers handled the cases. Both successful
server identification levels were 1, as was handler denial. Server descriptors
matched the expected owner and complete ACE rights. The raw report includes every
child token's groups/privileges and process IDs, not only setup's account names.

Connected server exchanges reported 8-10 ms; clients reported 4-6 ms. Their clocks
restart on connection/send, so these are not end-to-end setup times. Connection
setup allows 15 seconds; connected I/O shares a 5-second deadline. Pending I/O is
cancelled and drained before freeing buffers, with a further 5-second drain bound.
The pipe-denial timeout returned normally through that path. The report does not
retain cancellation completion codes; it must not be cited as an exhaustive
cancellation/race proof. No slow-sender, abnormal drain or process-crash case ran.

The [audit](evidence/SN-021-pipe-identity-summary.json) records exact source and raw
result hashes, 122 earlier evidence hashes and the twelve preserved local files.
The matching guest source is published unchanged; private host credential helpers
are not distributed. Raw OS privilege-state localization remains unchanged.
There was one recorded invocation in this preflight's result directory.

## Reproduction and limits

Run only in an explicitly disposable VMware guest with a recovery snapshot and
an elevated setup identity. This command creates accounts/profiles and fixture ACLs:

```powershell
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File windows_pipe_identity.ps1 -Mode Probe -Tag <fresh-12-hex-tag> -OutputPath <guest-report.json>
```

No automatic cleanup, retry, snapshot restore or service installation occurs.
All account passwords are generated in memory and excluded from reports. The
VM/manufacturer check is an operational guard, not proof against a hostile host.
The harness root is a fresh trusted fixture; it is not a new production resolver.

This establishes limited real-token identification without granting the server
SeImpersonatePrivilege, and distinguishes connection permission from handler
authorization. It does not establish server impersonation resistance, complete
64-byte framing, malformed/extra/slow-message refusal, protected manifest/store
startup validation, alias containment, document-level authorization, restart,
version/ABA handling, commit atomicity or recovery. No digest, metadata or message
grants model execution, interface compatibility, origin or redistribution rights.

Next implement the bounded query and startup checks in the request contract,
then measure its remaining rejection matrix in the VM. Do not install a service
or change host permissions. Preserve ADR 0064 and create-only support; managed
storage is not adopted and arbitrary-directory overwrite remains unsupported.
No engine behavior changed; no engine rerun is claimed. Preserve SN-017, SN-044,
MCU independence, profiles/tolerances and PDF/PID fixes. The final next-cycle
prompt remains conditional on actual SN-021 completion.
