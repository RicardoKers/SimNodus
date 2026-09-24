# SN-021: protected startup prerequisite

Date: 2026-09-24. Status: **two accepted startups and nine expected refusals**.
This is a prerequisite for the [request experiment](../architecture/AUTHORITY_REQUEST_EXPERIMENT.md),
separate from the [pipe identity observations](SN-021-pipe-identity.md). No endpoint,
64-byte query, production manifest or save API is implemented in this slice.

## Scope and acquisition

The owner ran the [exact guest probe](../../tests/resources/windows_store_startup.ps1)
in the disposable VMware Windows 11 Enterprise Evaluation build 26200 VM, on NTFS.
VIX verified the operator snapshot's existence without restoring it. Administrator
setup created one fresh ordinary writer account, a harness and ten fresh fixture
roots. Negative mutations were confined to those new fixtures. No service or host
account/ACL change occurred. No project, firmware, model or dependency was loaded.

The writer captures the direct local C: device mapping and pins the NTFS volume.
It opens each fixed component relative to its retained parent via NtCreateFile,
with no write/delete sharing. It rejects reparse/offline/type mismatches, unsupported
directory case policy, alternate basename spellings and multiple hardlinks. This
follows the existing resource-handle approach; it is not a textual prefix check or
a resolve-then-open proof. Handles remain alive through successful result capture.

The volume ancestor's owner and dangerous direct grants are checked. Fresh root,
store, manifest and data objects have exact expected owner, ACE masks and inheritance
checks; directories also require protected DACLs. The writer has read/execute rights
in these query-only fixtures. This does not validate a writable production store or
extend the earlier authorized-publication result to a new save protocol.

Administrative setup seals a fixed 64-byte fixture manifest: eight-byte SN21ST01
marker, 12-byte root identity, 12-byte data identity and 32-byte SHA-256. It contains
no credentials, client document mapping or run ID; it is not the full proposed
manifest. Trusted setup supplies the expected writer SID, checked against the actual
ordinary token. Manifest/data reads use the same retained handles, 64/4096-byte
limits and before/after identity/size checks. The manifest binds exact data identity
and digest, but does not record every ancestor or store-directory identity.

## Physical results

| Case | Result |
|---|---|
| Good fixture, process 8684 | Validated with handles retained |
| Same good fixture, fresh process 1960 | Independently validated; same identity and digest |
| Changed bytes | Refused at data: file-bytes |
| Same bytes in replacement object | Refused at data: file-identity |
| Missing data | Refused at data: NTSTATUS c0000034 |
| Extra hardlink | Refused at data: hardlink-alias |
| Unexpected file owner | Refused at data: wrong-owner |
| Unexpected file grant | Refused at data: wrong-ace-count |
| Manifest extended to 65 bytes | Refused at manifest: byte-limit |
| Store replaced by junction | Refused at store: reparse-or-offline |
| Unexpected root grant | Refused at root: wrong-ace-count |

All eleven child processes used the expected ordinary SID, with administrator
membership and the enumerated bypass privileges absent. The successful fixture's
data and manifest hashes remained unchanged after each startup. The common data
digest was `935bfe78ee5543c48bb16d7cdb82f05cac5ee86c0de7fd30ea44507f1cbff7e2`.
Negative acceptance requires the expected refusal stage and reason; an unrelated
harness exception is not a pass. No endpoint is created by any case.

The [audit](evidence/SN-021-store-startup-summary.json) preserves exact guest source,
raw result hashes, 126 earlier evidence hashes and twelve local overlays. All raw
OS diagnostic localization remains unchanged. This experiment recorded one run.
Private host credential helpers are not published. Local compile checks supported
preparation; the VM observations are the physical evidence.

## Reproduction and remaining gates

Only run in an explicitly disposable VMware guest with a recovery snapshot and
an elevated setup identity; the command provisions an account and fixture ACLs:

```powershell
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File windows_store_startup.ps1 -Mode Probe -Tag <fresh-12-hex-tag> -OutputPath <guest-report.json>
```

No automatic retry, cleanup, snapshot restore or service installation occurs.
Child runs have a 30-second wait bound; a timed-out process is left for inspection.
The manufacturer check is an operational guard, not a hostile-host security claim.

Not measured: all volume ACL policies and aliases, simultaneous hostile namespace
mutation, broker/OS crash or restart, complete setup-manifest semantics, integration
with the pipe, document-level authorization, framing, extra/slow messages, hostile
endpoints, save/version/ABA and recovery. Sequential fresh processes establish only
the recorded startup behavior. The two prerequisites do not compose into full
request-contract acceptance without an integrated physical experiment.

Next implement the 64-byte query with protected document/run mapping, retaining
startup handles through service of each request; test the remaining rejection matrix.
No service installation or host permission change is needed. Keep ADR 0064 and
create-only support; managed storage remains unadopted and arbitrary-directory
overwrite unsupported. SN-021 stays open. No engine behavior changed or engine
rerun is claimed. Preserve SN-017, SN-044, MCU independence, profiles/tolerances
and PDF/PID fixes. The next-cycle prompt awaits actual SN-021 completion.
