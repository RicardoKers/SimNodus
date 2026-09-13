# SN-018: corrected process identity and observer control

Date: 2026-09-12. This slice corrects experimental memory attribution after the
[inconclusive PID-only campaign](SN-018-observer-control.md). Historical code,
raw results and numerical acceptance remain unchanged. SN-018 stays in progress
pending its bounded coverage/acceptance review.

## Identity correction and verification

The new [sampler](../../tests/headless/process_memory.py) keeps process handles
for the entire batch and queries creation/exit times and memory through the
retained identities. A new child must be newer than its verified parent, created
no later than snapshot start and, if known, no later than its parent's exit.
Descendants of rejected candidates are not followed. Process names never grant
membership. Handles close between batches, including error paths.

Nine [tests](../../tests/headless/test_process_memory.py) passed before engine
execution: stale parent PID with unrelated descendants, PID reuse after snapshot,
equal creation time, exit/access failure during open, child discovery after a
known parent's exit, child creation after parent exit, handle cleanup, name
independence and a real Windows child process's identity and exit. Synthetic
identity tests establish rejection behavior; the real-child case exercises the
Windows APIs. Neither substitutes for the subsequent real-engine campaign.

Creation FILETIME is host wall-clock metadata, separate from virtual simulation
time. Handles preserve known identity across samples; conservative chronology
checks can omit ambiguous/short-lived processes. This is experiment attribution,
not security isolation or a complete process census. The original production
supervisor and startup PID retry are unchanged.

## Predeclared campaign and reproduction

The [new contract](../../tests/headless/OBSERVER_IDENTITY.md) was recorded after
tests and before engine execution. It preserves U,S / S,U / U,S: three adjacent
pairs, six batches, six fresh sessions per batch. U does not sample memory; S
uses the corrected sampler. Both retain 100 ms outer poll sleeps. No warm-up,
discarded outliers, engine retries, builds, downloads or priority changes occur.

```powershell
python -m unittest discover -s tests/headless -p test_process_memory.py -v
python tests/headless/measure_identity.py --output build/sn018/identity-01
python tests/headless/collect_identity.py --input build/sn018/identity-01 --output docs/experiments/evidence/SN-018-identity-summary.json
python tools/check_repository.py
git diff --check
```

Use fresh output paths. The manifest freezes versions, commands and source/binary
hashes before running. Engine command arrays are inherited from baseline-01;
only output paths change. Patched Renode 1.16.1, ngspice 47, GDB
15.2.90.20241229 and the MSVC 19.51.36246.0 Debug binaries remain fixed. Python
still controls preparation, GDB transport and fixture sequence. The historical
baseline and unsuccessful observer raw files are checked before and after.

The collector separately checks every sampled process's recorded ancestry back
to that batch's root identity. Every sample retains snapshot time, PID, creation
time and parent PID/creation; query errors and rejected candidates are retained.
No name audit is used as a substitute for identity. Sampled peaks remain lower
bounds, and summed working sets may double-count shared pages. U memory is
unmeasured. Retained handles and identity checks add costs specific to this new
observer; its timings cannot retroactively validate the original sampler.

## Scope and next step

### Measured results

[Compact evidence](evidence/SN-018-identity-summary.json) records a **passed**
functional campaign and a **passed** independent ancestry audit. All 36 sessions
passed 324 fixed checkpoints and 36 asynchronous pauses. Final virtual time
remained 4027000 ns, ADC 3541, maximum RC error 1.005721e-9 V and endpoint error
8.673618e-19 s. Frozen inputs and all prior evidence checks passed; 1492 new
raw-file hashes are retained. No engine batch was retried or discarded.

Validation: nine sampler tests passed, the independent recorded-ancestry audit
passed, `python tools/check_repository.py` passed (404 text files), and
`git diff --check` passed. Native code/harness behavior was unchanged; no fresh
CTest or full E-05 recertification is claimed.

| Pair | U wall (s) | S wall (s) | S minus U (s) | Relative difference | Median pause difference (ms) |
|---|---|---|---|---|---|
| 1: U,S | 48.291 | 47.596 | -0.695 | -1.439% | +6.554 |
| 2: S,U | 53.155 | 52.292 | -0.863 | -1.624% | -8.764 |
| 3: U,S | 53.710 | 54.268 | +0.558 | +1.039% | +2.641 |

Signs vary in both wall and pause comparisons. This control establishes usable
recorded ancestry and descriptive timing, **not a stable causal slowdown or
zero-overhead claim**. U pause latency min/median/max was 48.337/69.610/84.409 ms,
sample standard deviation 12.159 ms; S was 46.175/66.007/87.806 ms, sample standard
deviation 10.638 ms (18 sessions per condition, nested in three batches).

| Sampled batch | Peak working-set sum (MiB) | Peak private-commit sum (MiB) | Query errors | Rejected candidate observations |
|---|---|---|---|---|
| 2 | 949.137 | 823.750 | 0 | 268 |
| 3 | 953.242 | 826.875 | 0 | 6 |
| 6 | 944.797 | 821.438 | 0 | 7 |

All 14122 process-memory rows passed ancestry checks, covering 83/79/80 distinct
recorded identities in the sampled batches. Of 281 rejected candidate
observations, 259 were not newer than their alleged parent and 22 were created
after snapshot start. Counts include repeat observations of a rejected identity;
they are not counts of unique rejected processes. The rejection records preserve
the identifying timestamps. Conservative rejection may reduce coverage; no
rejected observation is silently added to memory totals.

Observed sample gaps were 104.777-157.330 ms. Sampler calls consumed
2.870/4.090/5.062 s of elapsed observer wall time across batches 2/3/6. Working
set/private-byte figures remain sampled lower bounds; U has no memory measurement.
The original historical attribution failure is not relabelled as passed.

Preparation encountered a local file-write denial before the new runner existed;
the runner was then created with the workspace patch tool. This did not start an
engine batch or alter the campaign. Historical source files were not modified.

The same 10 microvolt, 1 ps and 2 s limits apply. Nine fixed checkpoints and final
virtual 4027000 ns/ADC 3541 are required per session; variable pause boundaries
remain host-sensitive. Pause wall timing excludes later stability wait/commit;
batch wall includes setup, GDB, waits, teardown and exit-detection delay.

Three pairs on one uncontrolled Windows Debug host support descriptive
comparisons, not a universal overhead percentage, p99 or equivalence claim.
No Release, scaling, leak, clean-machine, additional firmware/platform, physical
ADC, unpaced or general autonomous C++ capability is validated. No new CubeIDE
or fault matrix is claimed. ADRs 0014/0027/0028/0043, PDF suppression and PID
retry are preserved. The collector is not the future shared instrumentation.

Next perform an explicit SN-018 coverage review: determine the bounded reference
examples and measurement evidence needed for closure, using the recorded gaps
rather than automatically adding new capabilities or more repetitions. No commit,
issue synchronization or remote publication is authorized.
