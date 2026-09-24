# SN-021: consolidated request matrix

Date: 2026-09-24. Status: **26 physical case criteria observed; bounded experiment**.
The [measured guest script](../../tests/resources/windows_request_matrix.ps1) extends
the [endpoint/restart experiment](SN-021-endpoint-restart.md). It is not a production
protocol, save implementation or completion of SN-021.

## Protocol and physical result

Private wire version 2 keeps the fixed 64-byte request/reply layout. After checking
the reply, the client sends one byte 0x7e. The server requires that acknowledgement,
sends a one-byte 0x7f success terminal and requires a matching 0x7f acknowledgement.
The server then closes. The client accepts only after validating the terminal and
observing closure without extra data. A disconnect before the terminal is failure,
not success. Startup handles remain held through the exchange; no unbounded flush
or reset of the connected five-second deadline is introduced.

Extra requests before the first acknowledgement reject. Bytes after the final
acknowledgement are not dispatched; this does not establish a multi-request atomic
transaction. The version-1 scripts and their evidence remain unchanged.

The owner ran the helper in the disposable VMware Windows 11 build 26200 / NTFS
guest, with three fresh ordinary accounts and protected fixtures. The snapshot was
verified, not restored. The physical run completed in 103,224 ms, within its
180-second orchestration budget.

| Cases | Required and observed outcome |
|---|---|
| Two valid clients | Valid query, terminal exchange and clean closure before acceptance |
| Wrong response magic/version/length | response-header refusal |
| Wrong correlation/digest | Corresponding response refusal |
| Short/oversized response | response-size / native 234 refusal |
| Duplicate reply / invalid terminal | extra-response-or-invalid-terminal refusal |
| Data after terminal | extra-response refusal at response-end |
| Closure before terminal | Native 109 at terminal, no acceptance |
| Terminal stall | Client cancelled at its deadline, cancel=0;completion=995; no acceptance |
| Small request/reply messages | First one-byte message refused for size; no accumulation |
| Extra request | Server refused invalid acknowledgement; client did not accept |
| Startup bytes/identity/missing/hardlink/owner/rights/manifest/junction/root rights | Specific refusal before endpoint readiness; no client launched |

All 26 records preserve their post-injection snapshots: content hashes, existence,
attributes and security descriptors. Separate pre-injection snapshots distinguish
intentional fixture changes from changes during processing. Missing data is moved
to a preserved fixture file; the junction points only to a renamed owned fixture.
These snapshots are a trusted harness oracle, not the resource containment algorithm.

Faulty responses are deliberate injections under the trusted writer identity;
they do not claim a malicious principal bypassed endpoint authentication. In the
post-terminal-extra case the injected server reports query-served while the client
correctly refuses. Overall acceptance depends on the expected client outcome, not
every process reporting success. Small-message tests cover message-mode framing,
not arbitrary byte-stream fragmentation. Cancellation covers the measured pending
read only, not every scheduling race.

## Attempts and reproduction

Attempt `20260924T123006Z-c1ee8153557d` remains failed-or-indeterminate at
open-encrypted-vm, VIX error 17005. The operator reported entering the password
incorrectly. No guest inventory or physical report exists for that attempt; do not
infer a simulator failure or invent guest observations. Attempt
`20260924T123021Z-7dd9f5466240` produced the 26-case result. Both attempts used the
same guest-source hash. No password is retained in the reports.

The [audit](evidence/SN-021-request-matrix-summary.json) preserves both result
records, the successful inventory/probe, exact source hashes, 141 earlier evidence
hashes and twelve local overlays. Private host credential helpers are not published.
Preparation passed PowerShell parsing, C# compilation, 81 framing checks and child
argument wiring; those local checks are not substitutes for physical evidence.

Run only in an explicitly disposable VMware guest with a recovery snapshot and an
elevated setup identity, since this creates accounts and fixture ACLs:

```powershell
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File windows_request_matrix.ps1 -Mode Probe -Tag <fresh-12-hex-tag> -OutputPath <guest-report.json>
```

No retry, cleanup, snapshot restore, service installation, dependency download or
project-resource execution occurs. Timed-out processes remain for inspection.

## Consolidated feasibility assessment

Read this with the [six gates](../architecture/PUBLICATION_AUTHORITY_PROPOSAL.md).
These are coverage judgments, not a new completion criterion or product decision.

| Gate | Current evidence and remaining limit |
|---|---|
| Provisioning | Fresh protected fixtures and exact rights checked; nine startup refusals now composed with the query implementation. No installer or production ownership lifecycle |
| Exclusion | Earlier filesystem probe measured ordinary-client mutation denial. This batch adds no concurrent namespace or hostile administrator coverage |
| Lifetime | Startup retention and orderly restart with administrative run rotation measured; no service stop/crash recovery or complete exclusion matrix across restart |
| Authorized access | Earlier dedicated-writer publication and wrong-owner endpoint refusal measured; no production transaction or executable attestation |
| Client boundary | v1 measured real identities, request refusals and replay; v2 now measures terminal/response handling and startup composition. Historical v1 results are not automatic v2 regression coverage |
| Preservation | Original evidence retained; measured post-injection snapshots unchanged. Not power-loss durability or preservation under arbitrary privileged interference |

Stop expanding the test protocol by default. The next step is to review this
coverage and identify only acceptance-critical gaps before selecting any further
physical batch or product direction. No claim is made that all six gates passed.
Safe saving, version/ABA, crash recovery, managed-storage adoption and the original
arbitrary-directory overwrite acceptance remain unresolved. A production service
or change to ADR 0064 requires its own decision; this report authorizes neither.

SN-021 remains in_progress. Preserve SN-017/SN-044, MCU independence, accepted
profiles/tolerances and PDF/PID fixes. No engine behavior changed or rerun is claimed.
Valid bytes do not establish compatibility, trusted origin, redistribution rights
or execution permission. The next-cycle prompt awaits actual SN-021 completion.
