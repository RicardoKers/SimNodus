# SN-021: managed-save isolation prerequisites

Date: 2026-09-24. Status: **bounded prerequisites observed; no save acceptance**.
This closes the selected prerequisite batch in the
[managed saving contract](../architecture/MANAGED_DOCUMENT_SAVE.md), using the
[unchanged private-v2 protocol](SN-021-request-matrix.md).

## Physical result

The successful disposable VMware Windows 11 build 26200 / NTFS run took 52,701 ms.
Three fresh ordinary identities were used; no service or host permissions changed.
The [measured guest source](../../tests/resources/windows_isolation_prerequisites.ps1)
contains byte-for-byte unchanged protocol C#, plus a trusted fixture identity oracle
and direct native mutation probes. It does not implement managed Save.

| Coverage | Observation |
|---|---|
| Two same-SID allowed clients | Complete v2 reply/terminal/closure acceptance |
| Connected unauthorized principal | Actual SID identified, reverted, rejected at document authorization |
| Anonymous / pipe-ACL denial | Native 1347 / client open 5; no accepted reply; pipe wait cancellation observed |
| Wrong document/run, path-shaped ID, Save operation | Specific frame refusal; no accepted reply |
| Lifetime baseline query | Exact 64-byte request captured at both endpoints |
| Ten mutation sequences while writer active | All failed with native/recorded access-denied code 5 |
| Forced writer termination, then ten sequences | Exit confirmed; all denied with code 5; snapshots unchanged |
| Fresh writer startup, then ten sequences | Writer alive before/after probes; all denied with code 5 |
| Exact prior request after administrative run-ID rotation | Refused specifically as request-run, with original document/root/data identities |
| Fresh request after restart | Valid v2 result through newly retained startup handles |

There are twelve query records and three attack-phase records, containing thirty
mutation attempts. Mutations cover data write, manifest write, file creation,
rename, delete, replacement, hardlink, ACL/owner change sequences and store rename.
ACL/owner sequences may fail at their initial descriptor read; do not describe them
as independent successful low-level WRITE_DAC/WRITE_OWNER access probes.

All fifteen records preserve their before/after snapshots: data/manifest hashes,
root/store/file identities, link counts and security descriptors. The terminated
writer (PID 3880) was an ordinary process launched and retained by this harness.
It was killed only during read-only connection waiting, not during publication.
Its earlier token report and confirmed exit are retained; no final report from the
killed process is invented. The administrative run-slot rotation is explicit and
separate from snapshot preservation. No OS restart, power loss or commit recovery
is measured. Store writer permissions in this read-only fixture do not demonstrate
write-enabled save-layout isolation; that must be measured with the commit candidate.

## Attempts and corrected harness

Four attempts are retained. The first two stopped before guest execution; the owner
reported credential mistakes. Their raw stages/errors remain authoritative. The third
(`20260924T192756Z-3d38d8b1c366`) remains **failed**: ten queries completed, but the
first attack child reported three access-denied writes followed by managed rename
FileNotFoundException (2). That is not permission-denial evidence. No termination or
restart acceptance follows from that aborted attempt.

A separate read-only collection retrieved that child's report and the waiting writer's
subsequent connection timeout. Those reports and collection hashes are preserved.
The coordinator now embeds failed child reports before refusing their status/SID.
Rename/delete/replace/link/store-rename probes use direct Win32 calls, avoiding the
managed rename precheck as a measurement obstacle. Missing-file error 2 is still not
accepted as denial. The [failed guest source](../../tests/resources/windows_isolation_attempt3.ps1)
is retained as historical evidence, not the recommended runner.

The fourth attempt (`20260924T231823Z-0261f4884c44`) produced the result above.
The [audit](evidence/SN-021-isolation-summary.json) records exact source/report hashes,
the collected diagnostics, 146 earlier evidence hashes and twelve preserved local
overlays. No secret is present; private host credential helpers remain unpublished.

Preparation passed parsing, C# compilation, 81 frame checks, argument forwarding,
VIX host preflight and five native mutation-API checks on fresh local files, including
missing-source error 2. They do not replace the distinct-principal physical result.

## Reproduction and next gate

Run only inside an explicitly disposable VMware guest, with an existing recovery
snapshot and elevated setup identity. This creates accounts/fixtures and terminates
only the child process owned by the test:

```powershell
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File windows_isolation_prerequisites.ps1 -Mode Probe -Tag <fresh-12-hex-tag> -OutputPath <guest-report.json>
```

No automatic retry, cleanup, snapshot restore, service installation or download.
Opening project resources never executes them. The fixture snapshot oracle uses
known provisioned paths; it is not a substitute for handle-relative containment.

The selected read-only identity/replay and listed exclusion-through-termination
prerequisites now permit designing the concrete commit candidate. Stop adding IPC
features. Next specify one atomic commit layout, state machine, limits and failure
injection points under ADR 0076 before implementing writes. The write-enabled store
must retain its own measured authority/exclusion properties; this result is not a
blanket proof for any future layout, token configuration or concurrency model.

SN-021 remains in_progress. Expected-version/ABA conflicts, simultaneous saves,
ordinary failures, lost replies, atomic publication and bounded recovery remain
pending. External overwrite is unsupported under ADR 0064. Preserve SN-017/SN-044,
MCU independence, profiles/tolerances and PDF/PID behavior. No engine change or new
readiness/compatibility/origin/redistribution claim; no next-cycle prompt yet.
