# SN-018: paired memory-observer control

Date: 2026-09-12. This continues the
[first local Debug baseline](SN-018-baseline.md) without changing its real-engine
profile. SN-018 remains in progress pending reference-example/coverage review.
**Measurement outcome: inconclusive because the sampler included unrelated
processes in batch 3. Engine validation passed all 36 sessions.** Preserve this
campaign; correct and test sampler ownership before new measurements.

Follow-up: the separate [corrected identity control](SN-018-identity-control.md)
has since passed its tests and real-engine/ancestry checks. It does not change
this campaign's inconclusive status or repair its historical memory records.

## Predeclared method and reproduction

The [contract](../../tests/headless/OBSERVER_CONTROL.md) was written before
execution. Three adjacent pairs use U,S / S,U / U,S, where U has no memory
sampling and S uses the unchanged baseline sampler. Each batch executes six
fresh sessions, for 18 per condition. No warm-up, discarded samples, engine
retries, rebuilds, dependency downloads, affinity or priority changes occur.

```powershell
python tests/headless/measure_observer.py --output build/sn018/observer-01
python tests/headless/collect_observer.py --input build/sn018/observer-01 --output docs/experiments/evidence/SN-018-observer-summary.json
```

Use a new output directory when repeating. `manifest.json` records the exact
command arrays, hashes, versions, CPU, OS, Python and power scheme before the
first engine starts. `summary.json` contains all batch/session measurements,
paired differences and hashes of raw logs and sampled memory files. The
historical baseline's inputs and 606 raw files are verified before execution;
both those files and prior compact evidence are checked again afterward.

Renode remains the patched 1.16.1 build, ngspice 47, GDB 15.2.90.20241229 and
the existing MSVC 19.51.36246.0 Debug native binaries. Firmware, platform,
analog model, all native flags, steps/lifecycle/pause/guarded and 1 ms callback
pacing are unchanged. The new wrapper reads its engine command directly from
the earlier frozen manifest, replacing only the output directory.

Both conditions retain process-exit polling and 100 ms sleeps. S additionally
discovers the process tree and queries memory; U memory is unmeasured, not zero.
S records total elapsed wall time inside sampler calls, which is neither CPU
time nor a measure of simulation slowdown. Sampling extends S's polling period.
The two conditions therefore also differ in exit-detection delay, bounded by
their actual polling interval rather than an exact common 100 ms period.

Pause timing keeps the established boundary: before retained MI interrupt
through stopped-state readback, excluding the later stability wait/commit.
Wall-clock latency and host variability remain separate from virtual time.
Fixed checkpoints are compared with cycle 27, RC against its analytical
trajectory and endpoints against integer virtual nanoseconds. The same 10
microvolt, 1 ps and 2 s limits apply. Final virtual time is 4027000 ns, ADC 3541.

## Interpretation limits and next step

### Results and observer defect

[Evidence](evidence/SN-018-observer-summary.json) separates functional acceptance
from measurement validity and preserves the raw summary, manifest, 1495 raw-file
hashes and all values. Frozen inputs, historical baseline files and earlier
compact evidence remained unchanged. All 36 sessions passed 324 fixed checkpoints
and 36 pauses, ending at 4027000 ns with ADC 3541. Maximum RC error was
1.005721e-9 V and endpoint error 8.673618e-19 s, within the unchanged limits.

| Pair / execution order | U wall (s) | S wall (s) | S minus U (s) | Relative difference | S minus U median pause (ms) |
|---|---|---|---|---|---|
| 1: U,S | 53.990 | 53.550 | -0.440 | -0.815% | +5.821 |
| 2: S,U | 51.305 | 53.713 | +2.408 | +4.693% | -1.570 |
| 3: U,S | 52.038 | 53.490 | +1.452 | +2.791% | -5.684 |

These are retained raw comparisons, **not valid three-pair overhead estimates**.
Batch 3 (S in pair 2) collected unrelated desktop processes in 80 consecutive
samples, including shell, widgets and WebView processes. It recorded 12562 query
errors and an inflated working-set sum. Its memory totals and pair 2 overhead
attribution are invalid. No batch was removed, replaced or rerun; the complete
predeclared control is inconclusive. The other pairs cannot repair that design.

The raw pause medians were 66.595 ms for U and 67.024 ms for S. Ranges were
56.486-83.902 ms and 46.206-103.407 ms respectively, all within 2 s. These remain
observations of passing engine sessions, not proof that the observer has no
effect. Sampler calls consumed 3.977, 4.242 and 4.116 s of elapsed observer wall
time in batches 2/3/6; these are not CPU times or simulation slowdown measures.

Code inspection found that the unchanged sampler follows numeric parent PIDs
without checking process creation identities. PID reuse or stale parent ancestry
is a plausible mechanism, but samples did not retain parent PIDs or creation
times, so the exact chain cannot be reconstructed. The diagnostic name audit
found no unexpected names in the three earlier baseline files or batches 2/6;
names alone do not prove ownership. Earlier memory estimates remain provisional
sampled observations, not validated process-ownership measurements.

The raw harness status `passed` records engine checks only. The compact evidence
sets overall measurement status to `inconclusive`. Initial copying of the compact
report was denied by the filesystem; the annotated collector then wrote the
authorized local evidence. All raw results remain unchanged.

Validation: the repository checker passed (397 text files) and
`git diff --check` passed. The wrapper and audit collector ran against the
real-engine campaign and retained its defect. No new native CTest/protocol
matrix is claimed; adversarial sampler identity tests are the next required step.

Three pairs do not isolate background load, thermal state, caches or order
effects. The order alternates but is not randomized. Eighteen sessions per
condition are nested in three batches, not 18 independent host experiments.
Paired differences are descriptive; no p99, statistical significance,
equivalence or general overhead guarantee follows. Historical baseline timing
is context only; contemporaneous U batches are the actual controls.

The monitor remains outside the measured process tree. Sampled memory can miss
short-lived helpers and peaks; summed working sets can double-count shared
pages. No unsampled memory, leak, scaling, Release or clean-machine claim is
made. No new fault matrix or CubeIDE execution is claimed.

Python retains preparation, GDB transport and fixture selection. PDF suppression,
PID retry, historical failures and ADRs 0014/0027/0028/0043 are preserved.
This is experiment measurement, not shared-instrumentation implementation or
an autonomous all-C++ simulator. No platform/circuit/debug capability expands.

Next correct the sampler's process identity/ancestry checks in a new bounded
slice, retain parent and creation-time evidence, and add adversarial tests for
stale parent/PID reuse and process exit. Do not fix attribution with a process
name allowlist. Predeclare a new paired campaign only after those checks pass;
preserve this unsuccessful control and the original sampler as historical input.
Then review SN-018 coverage against its backlog criterion and explicitly select
the remaining fixed reference examples or measurement gaps. Avoid treating
more repetitions of this one Debug fixture as proof of broader readiness.
No commit, issue synchronization or remote publication is part of this cycle.

| Backlog measurement area | Coverage of these two slices | Remaining assessment |
|---|---|---|
| Fixed reference examples | One owned GPIO/RC/ADC fixture, with step/pause/recreation subcases | Decide whether these subcases meet the intended example set or name another existing validated reference |
| Reproducibility | Recorded source/binary inputs, commands and repeated fresh processes | Complete transitive runtime manifest and portable preparation remain unvalidated |
| Latency | Batch wall duration and bounded pause acknowledgement | Neither solver-only throughput nor full interrupt-to-commit latency is measured |
| Error | Analytical RC, endpoint tolerance and firmware ADC code | No broader electrical/firmware support is inferred |
| Memory | Sampled process-tree working set/private commit and observer control | Exact peaks, leak behavior and scaling remain unmeasured |

Missing coverage here is a disclosure, not an automatic requirement to implement
all performance categories. The next acceptance review must select a concrete,
bounded completion criterion for SN-018 without expanding supported capabilities.
