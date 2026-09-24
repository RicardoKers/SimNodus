# SN-021: endpoint and orderly restart evidence

Date: 2026-09-24. Status: **seven limited physical scenarios observed**.
This extends the [integrated query](SN-021-integrated-query.md), not production
saving or the complete [request contract](../architecture/AUTHORITY_REQUEST_EXPERIMENT.md).

## Method and result

The owner ran two attempts in the disposable VMware Windows 11 build 26200 guest
on NTFS. The existing snapshot was verified, never restored. Fresh fixtures and
three ordinary accounts were created per attempt; no service or host ACL changed.
The [measured source](../../tests/resources/windows_endpoint_restart.ps1) preserves
the retained-handle startup and fixed read-only protocol from the prior probe.
It adds controlled hostile listeners, exact request capture and orderly restart
orchestration. No project resource is executed.

| Scenario | Observation |
|---|---|
| Wrong-owner squatter | Legitimate writer refused at creation (5); client rejected owner before sending bytes; listener observed disconnect (109), zero request bytes |
| Unexpected rights | Same trusted writer SID, but extra client read-EA right (0x12018b rather than 0x120183); client rejected rights before sending bytes; listener observed disconnect |
| Competing same-SID writer | Creation refused (231), no readiness, original still alive; original then served a valid query |
| Initial query | Valid reply; exact 64-byte request captured at both endpoints |
| Old request after restart | Same endpoint/document/data identities, new ordinary server process; exact old request rejected specifically as request-run, no reply |
| Fresh request after restart | Valid reply under the new run ID and independently retained startup handles |
| Changed bytes before another start | Refused as file-bytes before endpoint readiness; no client launched |

There are nine observation records: seven scenarios plus two intermediate
occupied-name refusals. All seven scenario records preserve data/manifest hashes
relative to their post-setup inputs. The complete second attempt took 35,530 ms.

After the first restart-scenario process exited, trusted administrative setup changed
only the manifest's 16-byte run slot. The report records old/new IDs, exact captured
request, unchanged data hash and changed manifest hash. Old/new server PIDs were
4784/7024; the subsequent valid query used PID 4104. Root and data identities stayed
the same. The later negative startup deliberately changed fixture bytes before its
preservation baseline. This is orderly restart with operator-controlled run rotation,
not crash recovery, production session management, versioning or ABA protection.

## Failed attempt retained

Attempt `20260924T120311Z-89a9e7e04d5c` remains **failed**. Wrong-owner and
wrong-rights checks completed, but the harness rejected the competitor's 231
because its assertion required 5. Restart cases were not reached. The first server's
final report was not collected by the aborted orchestration; do not infer its final
state or promote that attempt to a pass.

The corrected assertion admits 231 only for the same-SID competitor at creation,
with no ready endpoint and the original process still alive. Other unexpected
errors still fail. The correction changes the harness, not the C# protocol.
Microsoft defines [231 as ERROR_PIPE_BUSY](https://learn.microsoft.com/en-us/windows/win32/debug/system-error-codes--0-499-).
The observations do not isolate which of the one-instance limit and first-instance
flag caused refusal, nor prove that either flag independently suffices.

The [audit](evidence/SN-021-endpoint-restart-summary.json) retains exact raw reports
for both attempts, both guest-source hashes, 134 earlier evidence hashes and twelve
preexisting local overlays. The [first guest source](../../tests/resources/windows_endpoint_restart_attempt1.ps1)
is retained for reproduction of the failed assertion, not recommended execution.
Private host credential helpers are not published. Raw localized diagnostics remain
unchanged. Local preparation passed parsing, C# compilation, 81 frame checks and
child-argument wiring; these checks do not replace the physical evidence.

## Reproduction and limits

Run the corrected source only in an explicitly disposable VMware guest, with an
existing recovery snapshot and an elevated setup identity:

```powershell
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File windows_endpoint_restart.ps1 -Mode Probe -Tag <fresh-12-hex-tag> -OutputPath <guest-report.json>
```

The harness uses 15-second connection waits, 5-second connected I/O deadlines,
bounded cancellation draining and a 180-second orchestration budget. Timeout leaves
processes/fixtures for inspection. No automatic retry, cleanup, restore, dependency
download or service installation occurs. The VMware manufacturer check is an
operational guard, not protection against a hostile host.

Writer identity, administrator and kernel remain trusted. Wrong-owner refusal is
not executable attestation. Matching hashes do not establish compatibility, origin,
redistribution rights or execution authorization. This result does not expand the
supported project profile or change ADR 0064.

SN-021 remains in_progress. Next consolidate the outstanding request-contract
matrix (hostile responses/duplicates, dribbling deadlines and startup refusals
composed with IPC) before proposing another physical batch. Do not add save or
recovery features to close these gaps. Managed-storage adoption and arbitrary-folder
overwrite acceptance remain separate unresolved decisions. Preserve SN-017/SN-044,
MCU independence, tolerances and PDF/PID fixes; no engine change is claimed.
