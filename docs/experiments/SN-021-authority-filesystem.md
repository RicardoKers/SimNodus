# SN-021: distinct-principal filesystem authority

Date: 2026-09-23. Status: **bounded physical observation; partial feasibility only**.
The owner supplied a disposable VMware Workstation Pro 26H1u1 VM and authorized
this account/ACL fixture experiment, without service installation. ADR 0064 and
create-only publication remain unchanged. No production project resource runs.

## Acceptance and environment

The [manual guest probe](../../tests/resources/windows_authority_filesystem.ps1)
runs under Windows PowerShell in the disposable VMware guest only. Administrative
setup creates two fresh ordinary local accounts and a fresh NTFS directory tree.
The measured writer and client processes must have distinct SIDs, no administrator
membership and none of the enumerated bypass privileges. Root and harness are
protected from ordinary writers; only the writer receives store write authority.
The client receives read/execute access. Setup records physical identity, owners,
DACLs and effective token groups/privileges. Unexpected principal grants reject.
This is a fixed fixture, not a production path resolver or provisioning policy.

Measured environment: Windows 11 Enterprise Evaluation, build 26200, NTFS. The
operator-created `SN021-authority-baseline` snapshot was located by VIX before
provisioning. Its application consistency and memory capture were not measured.
Local VIX orchestration used hidden credential prompts and copied only the probe;
private host paths and credential helpers are not distributed. Guest invocation:

```powershell
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File <probe.ps1> -Mode Probe -Tag <fresh-12-hex-tag> -OutputPath <guest-report.json>
```

Use only an explicitly disposable VMware Windows guest with a recovery snapshot
and an elevated setup identity. The probe creates accounts, profiles and fixture
ACLs there. It leaves them in place for inspection; no automatic deletion, retry,
snapshot restoration, downloads or service installation. The script rejects a
non-VMware manufacturer, non-NTFS volume, pending reboot or preexisting fixture.
Those checks are operational guards, not a cryptographic proof of VM isolation.

## Results and preserved attempts

The [audit](evidence/SN-021-authority-filesystem-summary.json) links original raw
reports with exact hashes, source hashes and 108 earlier evidence hashes. Raw OS
diagnostics retain their original localization; authored project text is English.

| Attempt (UTC) | Observation |
|---|---|
| 18:10:07 | Inventory passed; automatic snapshot request failed with VIX 35; no account provisioning |
| 18:13:16 | Manual snapshot found; writer created target; four client operations denied; untyped PowerShell null caused replacement ArgumentException 87 |
| 18:16:03 | Typed-null correction reached five denials; hardlink threw Win32Exception, but recorder omitted NativeErrorCode; classification inconclusive |
| 18:18:04 | Opening encrypted VM failed with VIX 17005; no guest command |
| 18:20:53 | Opening encrypted VM again failed with VIX 17005; no guest command |
| 18:21:11 | All four child processes completed; nine operations denied before and after authorized replacement |

The null-conversion defect was reproduced on disposable host files: untyped null
produced ArgumentException, typed NullString permitted replacement. Win32Exception
can expose generic HRESULT low bits 16389 even when NativeErrorCode is 5; the
recorder now retains both. Neither correction retroactively passes earlier runs.
All previous guest fixtures and raw reports remain intact.

In the completed run, writer PIDs 11072 and 8464 used the same ordinary writer
SID; client PIDs 5696 and 2276 used a different ordinary SID. The writer created
`original fixture`, exited, and a new writer process published `replacement fixture`
with File.Replace and a typed null backup argument. Physical identity changed.
Both target generations had the writer as owner and a single hardlink.

Each client attempted write, create, rename, delete, replace, hardlink, change ACL,
change owner and rename store. All **18 attempts returned access denied (5)**.
After every attempt, exact bytes, descriptor, physical identity and single-link
count remained unchanged. These observations establish the measured filesystem
exclusion for these ordinary accounts across closed writer handles and separate
writer processes, including a successful authorized replacement.

## Limits and next gate

This does not complete the proposal's six feasibility gates. Provisioning captured
the principal objects, not every profile, temporary or result object with exhaustive
rights-mask assertions. Independent client exclusion passed only the listed attacks.
Lifetime covers process exit and a new writer process, not broker/OS restart or
startup validation. Authorized writer access passed; IPC impersonation, leaked
writable handles, unauthorized IDs/tokens/paths and service deployment are untested.
The audit preserves repository history; it does not invent an external-fixture audit.

No adversarial concurrent namespace replacement, continuous reader history,
expected-version/ABA protocol, commit record, crash recovery, disk-full handling or
power-loss durability was tested. Path inspection in this fixed protected fixture
is not resolve-then-open containment proof for untrusted projects. No hash proves
interface compatibility, execution authorization, origin or redistribution rights.
No engine behavior changed, so no engine integration rerun is claimed.

Next specify bounded client requests and authentication/authorization under these
distinct identities, including startup validation, before another physical probe.
Do not install a service or change host permissions. Managed-store adoption and
any replacement of arbitrary-directory overwrite acceptance need a separate scope
decision. SN-021 remains open; retain SN-017, SN-044, MCU independence, numerical
profiles, PDF suppression and PID retry fixes. The final next-cycle prompt awaits
actual SN-021 completion.
