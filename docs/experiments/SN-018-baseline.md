# SN-018: first local reproducibility/performance baseline

Date: 2026-09-12. Scope: the accepted ADR 0014/0043 Windows Debug headless
composition. SN-018 remains **in_progress**; this first slice measures one fixed
example and does not close broader reference-example or performance coverage.

Subsequent [observer control](SN-018-observer-control.md) exposed an ancestry
defect in the unchanged memory sampler. No unexpected process names were found
in this baseline's samples, but names do not establish ownership; its memory
figures remain provisional. Preserve this record and correct/test process
identity before further measurements. Numerical/functional results are unchanged.

## Contract and reproduction

The [measurement contract](../../tests/headless/BASELINE.md) was written before
execution. The wrapper freezes its source/contract, cycle 27 reference inputs,
commands and hashes in `build/sn018/baseline-01/manifest.json` before starting
engines. Three sequential batches each execute three initial/recreated pairs:
18 fresh sessions, no warm-up, no discarded repetitions. The original harness,
engines, Debug binaries, firmware, timing policy and numerical limits are unchanged.

```powershell
python tests/headless/measure_baseline.py --output build/sn018/baseline-01
python tests/headless/collect_baseline.py --input build/sn018/baseline-01 --output docs/experiments/evidence/SN-018-baseline-summary.json --host-cpu "11th Gen Intel Core i7-11370H @ 3.30 GHz; 8 logical processors"
python tools/check_repository.py
git diff --check
```

Use unused output paths for reproduction. The compact evidence preserves the
manifest, every batch/session metric, per-process observed memory maxima and
hashes of the raw logs/reports/memory samples. Existing evidence is not rewritten.
No build occurs during timed execution. Exact real-engine commands are arrays
in the manifest, including all native flags through `--native-inspection-time`
and `--steps --lifecycle --pause --guarded`.

The runtime is patched Renode 1.16.1, ngspice 47 and bundled GDB
15.2.90.20241229; existing native binaries use MSVC 19.51.36246.0 Debug.
Recorded Python is 3.14.4, Windows 11 build 26200, i7-11370H with eight logical
processors and the Balanced power scheme. The CIM processor query was denied;
the registry/environment supplied the CPU model/count. Native command output
retains the host's original localization in raw evidence.

## Measured results

All three batches and 18 sessions passed. The
[compact evidence](evidence/SN-018-baseline-summary.json) records 606 raw-file
hashes. All frozen inputs and earlier compact evidence hashes remained unchanged.
There were 162 matching fixed checkpoints and 18 additional asynchronous pauses;
all sessions ended at 4027000 ns with ADC 3541. Observed pause virtual time was
2014000 ns in all 18 sessions, without making it a future fixed-boundary promise.

| Metric | Batch 1 | Batch 2 | Batch 3 |
|---|---|---|---|
| Total wall time (s) | 57.472 | 62.118 | 63.004 |
| Peak sampled summed working set (MiB) | 950.707 | 949.121 | 953.250 |
| Peak sampled summed private bytes (MiB) | 827.738 | 826.293 | 826.426 |
| Memory query errors | 1 | 0 | 0 |

Pause wall latency across 18 sessions: minimum **45.089 ms**, median
**68.609 ms**, maximum **93.517 ms**, sample standard deviation **11.718 ms**.
Batch wall median is 62.118 s and sample standard deviation is 2.971 s (n=3).
Maximum absolute RC error is **1.005721e-9 V** against the unchanged 1e-5 V
limit. Maximum analog endpoint error is **8.673618e-19 s** against 1e-12 s.
ADC code error is zero. All comparisons include the original harness checks.

Validation: `python tools/check_repository.py` passed (392 text files) and
`git diff --check` passed. The new wrapper and collector were exercised on the
actual measurements. Native code and the existing harness were unchanged; no
fresh CTest, protocol fault matrix or CubeIDE recertification is claimed.

Actual sample gaps ranged from 105.737 to 292.078 ms, so nominal 100 ms must
not be treated as an achieved sampling guarantee. One failed process-memory
query is retained; these measurements do not establish an exact memory peak.
No failed engine session or outlier was removed. Compact evidence collection
initially encountered a filesystem permission error, then succeeded with the
authorized local write; raw engine measurements were neither changed nor rerun.

## Measurement interpretation

Batch wall time includes startup, preparation, GDB transport, deliberate waits,
teardown and up to one sampling interval of exit detection delay. It is not
simulation throughput. Pause latency starts immediately before retained MI
interrupt and ends after cancellation, analog agreement, notification and stop
readback, before the 100 ms stability wait and joint commit. The 2 s bound is
unchanged. Virtual time remains integer nanoseconds; final virtual time is
4027000 ns per session, independent of wall duration.

Memory uses Toolhelp process-tree discovery and GetProcessMemoryInfo at nominal
100 ms intervals. Simultaneous summed working sets can double-count shared pages;
PrivateUsage is private committed memory, not physical RAM. Sampled peaks are
lower bounds and can miss short-lived helpers. Actual sample gaps and failed
queries are retained. The monitor is outside the measured tree but its host
overhead remains. No affinity, priority, cache flushing or load isolation is used.

RC errors are independently calculated against the analytical 3.3 V / 1 ms
trajectory at all nine fixed checkpoints plus each asynchronous pause. The
limits remain 10 microvolts for voltage and 1 ps for analog endpoint agreement.
Fixed CPU/time/GPIO/mailbox/register records are compared with cycle 27;
asynchronous pause times and polling counts are allowed to vary with the host.

## Limits and next step

This is one local Debug example with 18 sessions nested in three batches.
It provides no tail-latency estimate, clean-machine reproducibility, Release
comparison, scaling curve, memory-leak conclusion or classroom readiness claim.
Full transitive runtime closure and observer-overhead controls remain future
measurement refinements; the manifest freezes the reference's recorded binary
inputs and additional fixture/configuration sources, not an OS image.

Python still prepares dependencies, transports GDB and selects the fixture
sequence. No autonomous all-C++ simulator, new MCU/peripheral, physical ADC,
arbitrary circuit, general causal feedback, unpaced execution, prediction or
rollback is approved. No new CubeIDE/fault matrix is claimed. PDF suppression,
PID retry, historical failures and ADRs 0027/0028 remain unchanged. The sampler
is experiment evidence collection, not the future shared instrumentation layer.

Next select a bounded SN-018 measurement refinement: assess observer overhead
with a predeclared unsampled control of this same fixture before interpreting
performance differences or adding reference examples. Keep SN-018 in progress
until its intended coverage and acceptance are explicitly assessed. No commit,
issue synchronization or remote publication was performed.
