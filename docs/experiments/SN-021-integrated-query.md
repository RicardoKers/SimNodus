# SN-021: integrated read-only query slice

Date: 2026-09-24. Status: **18 physical case criteria observed; limited slice**.
This combines the [startup prerequisite](SN-021-store-startup.md) and
[pipe identity prerequisite](SN-021-pipe-identity.md) for a bounded query from the
[request contract](../architecture/AUTHORITY_REQUEST_EXPERIMENT.md). It does not
complete that contract, adopt managed storage, implement saving or close SN-021.

## Method and boundaries

The owner ran the [exact embedded guest source](../../tests/resources/windows_integrated_query.ps1)
in the disposable VMware Windows 11 Enterprise Evaluation build 26200 VM, on NTFS.
The existing operator snapshot was verified, not restored. Setup created three
ordinary local accounts and fresh protected fixtures; previous fixtures remained
in place. No service or host permissions changed. No project resource executed.

Each server validates its store using retained volume/parent/file handles before
creating the pipe. Those handles survive request processing. A protected 224-byte
experimental manifest contains the SN21ST02 marker, root/data identities, digest,
16-byte document/run IDs and writer/client SID slots. Each 64-byte SID slot starts
with binary length (8-60), followed by the SID and zero padding. This is fixture
setup metadata, not a production store or project format. The server checks its
actual SID and the protected allowed-client mapping, not request identity claims.

The 64-byte request contains fixed magic/version/operation/body length, a correlation
ID and document/run IDs. A fixed 65-byte buffer rejects oversize input; no allocation
uses the declared size. The server obtains the real client token at identification
level, verifies reversion, checks authorization and framing, then returns a 64-byte
response containing the digest read through startup-retained handles. The client
checks response size, header, correlation and digest. No path, native handle or
project bytes are accepted as a request operand or returned as a result.

Client pipe rights are now 0x120183: explicit write-attributes permits selecting
message read mode. Neither FILE_CREATE_PIPE_INSTANCE nor security-changing rights
are granted. Both sides verify exact owner/ACE rights. This changed mask was measured
in this run; earlier identity evidence alone did not validate it. Tokens remained
ordinary, without the enumerated bypass privileges or SeImpersonatePrivilege.

## Results

| Cases | Observed result |
|---|---|
| Two allowed client processes | Query served and 64-byte reply verified |
| Connected but unauthorized SID | Refused at authorization; no reply |
| Anonymous client | Token query error 1347; reverted; no reply |
| SID excluded by pipe ACL | Client open denied (5); server connection wait cancelled |
| Wrong magic/version/declared length/Save operation | Corresponding frame refusal; no reply |
| Wrong document/run and path-shaped document ID | Corresponding ID refusal; no reply |
| 63-byte request | Size refusal; no reply |
| 66-byte request | ERROR_MORE_DATA (234); no reply |
| Extra message | First valid reply received; extra request refused; no second reply path |
| Slow sender | Read deadline expired; no reply |
| Disconnect before request | Read failed with broken pipe (109); no dispatch |
| Changed startup bytes | Refused before readiness; no client launched |

All 18 case reports record unchanged data/manifest hashes relative to their
post-setup inputs. The startup corruption was deliberately applied before that
baseline in its separate negative fixture. Reports retain before hashes, expected
response digest, token identity, security descriptors, stages and error details.
The two good processes use the same authorized SID but separate fresh fixtures/IDs;
this is not a restart/replay or simultaneous-client measurement.

Connection establishment allows 15 seconds for child startup; connected I/O shares
a 5-second monotonic deadline. Each pending timeout requested cancellation and
recorded `cancel=0;completion=995` before releasing buffers. Cancellation draining
has a further 5-second bound. These two observations do not prove every cancellation
race. The read-only reply can precede detection of a later extra message: there is
no all-or-nothing multi-request transaction. Client closure replaces the earlier
preflight acknowledgement; no unbounded flush is used.

The [audit](evidence/SN-021-integrated-query-summary.json) retains exact source/raw
hashes, 130 prior evidence hashes and twelve preserved local overlays. Raw OS
diagnostic localization remains unchanged. One invocation was recorded for this
slice. Compilation and 81 local frame checks supported preparation; they are not
substitutes for this physical run. Private host credential helpers are not published.

## Reproduction and remaining acceptance

Run only in an explicitly disposable VMware guest with a recovery snapshot and
an elevated setup identity; this provisions accounts and fixture permissions:

```powershell
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File windows_integrated_query.ps1 -Mode Probe -Tag <fresh-12-hex-tag> -OutputPath <guest-report.json>
```

No automatic cleanup, retry, snapshot restore, dependency download or service
installation occurs. A timed-out child remains for inspection. The manufacturer
guard is operational, not a security proof against a hostile host.

Still unmeasured: hostile server/endpoint squatting, competing server instances,
actual stale-run replay across restart, forged responses/duplicates, dribbling
messages, simultaneous hostile namespace changes, and the complete startup refusal
matrix composed with IPC. This probe serves only one connection per server process.
Store-directory identities are inspected but not individually bound in the fixture
manifest. Successful hashes do not establish compatibility, origin, redistribution
rights or permission to execute resources.

Next measure endpoint authenticity and replay across a real server restart before
claiming the client boundary accepted. No save/version/ABA or crash recovery is
implemented. Preserve ADR 0064, create-only support, exclusion of TxF, SN-017,
SN-044, MCU independence, profiles/tolerances and PDF/PID fixes. No engine behavior
changed or engine rerun is claimed. The final next-cycle prompt awaits SN-021 closure.
