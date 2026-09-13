# SN-018 bounded baseline acceptance

Date: 2026-09-12. Result: **done for the local Windows Debug baseline** under
[ADR 0044](../decisions/0044-bounded-baseline-acceptance.md). This audit closes
the recorded task, "Establish reproducibility/performance baseline", against
"Fixed reference examples, latency/error/memory measurements". It does not
establish product performance targets or a general simulator.

## Coverage decision

The reference examples are fixed execution scenarios of the accepted ADR
0014/0043 composition: (1) GPIO/RC/ADC stepping, (2) paced cooperative pause,
stable inspection and resume, and (3) a fresh recreated session with stopped
reconnect. They share **one circuit and firmware fixture**, not three distinct
circuits or independent devices. The user-directed measurement scope retained
that profile; requiring a new circuit merely to multiply examples would expand
the measured scope. This decision explicitly accepts those bounded scenarios
for the initial baseline, not for the later three classroom lesson projects.

| Acceptance area | Evidence | Practical limit |
|---|---|---|
| Fixed inputs and scenarios | Predeclared contracts, command arrays, firmware/platform and source/binary hashes | One prepared local Windows Debug environment |
| Reproducibility | Repeated initial/recreated sessions; 324 fixed checkpoints in the corrected campaign; RNF-02's same-environment discrete comparison and published tolerances | No clean-machine or complete transitive-runtime reproduction claim |
| Latency | Batch wall and pause acknowledgement distributions; three contemporaneous sampled/unsampled pairs | Host-sensitive; not solver throughput or full interrupt-to-commit latency |
| Error | 360 analytical RC/endpoint comparisons; final ADC 3541 in all 36 sessions | Same 10 microvolt/1 ps bounds; focused boundary-sampled ADC only |
| Memory | Corrected process identities; 14122 ancestry-verified rows, sampled working-set/private-commit peaks | Conservative lower bounds; no exact peaks, leak or scaling claim |
| Observer influence | Predeclared U,S / S,U / U,S control; all raw paired differences retained | Mixed signs leave stable causal slowdown unresolved; no zero-overhead claim |
| Preservation | 3793 file hashes revalidated; failed predecessor explicitly retained | Historical PID-only memory is not retroactively certified |

The [corrected campaign](SN-018-identity-control.md) supplies the accepted memory
and timing reference. The [initial baseline](SN-018-baseline.md) remains historical
context and the [inconclusive campaign](SN-018-observer-control.md) remains
inconclusive. Successful engine checks are not used to hide invalid attribution.
No additional identical runs are required to establish a descriptive baseline.

## Audit execution

```powershell
python tests/headless/audit_baseline.py --output build/sn018/acceptance-audit-01.json
python tools/check_repository.py
git diff --check
```

Use a new output path. [Audit evidence](evidence/SN-018-acceptance-summary.json)
records matching hashes and freshly recalculated metrics/ancestry from existing
raw reports. The audit did **not** start engines, rerun CTest or recertify CubeIDE.
The prior nine sampler tests and actual engine runs remain their original evidence.
The repository checker passed (408 text files), and `git diff --check` passed.

An initial audit invocation in a different execution context reported some runtime
files unavailable. Read-only inspection and the audit in the normal workspace
context found all 3793 files present and matching; no file was regenerated or
excluded. The successful local JSON was copied with the workspace patch tool.
This environment-visibility discrepancy is not a new engine failure or proof of
portable file access. No prior evidence was rewritten.

## Remaining work and next task

RNF-02 is evaluated here for the same declared environment. Portable projects,
clean installation, supported hardware/OS specifications, Release measurements,
performance budgets, GUI responsiveness and classroom rehearsal remain separate
requirements under SN-020, SN-023/024, SN-026/027/028 and the release gates.
Scaling, exact peaks and leaks have not been measured. They may be selected when
a concrete product decision needs them; they are not silently declared complete
or added retrospectively as blockers to this initial baseline.

SN-017 still accepts native contracts/adapters/session plus Python/C# fixture
orchestration. Python owns preparation, GDB transport and action selection.
Preserve all capability restrictions, PDF suppression, PID retry and ADRs
0014/0027/0028/0043. No new MCU, physical ADC, unpaced feedback, arbitrary circuit,
instrumentation or autonomous all-C++ simulator is approved.

Next select **SN-020**, beginning with the smallest circuit/component schema and
validation specification. Review stable IDs, connectivity versus symbols/models,
hierarchy and untrusted project input before implementation. SN-020 remains
planned until selected; this audit starts no GUI or project loader. January
stabilization and February 2027 classroom targets remain unchanged. No commit,
issue synchronization or remote publication was performed.
