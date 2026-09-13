# SN-018 first measurement contract

Declared before execution on 2026-09-12. This is a local Windows Debug baseline
of the accepted ADR 0014/0043 native/Python composition, not E-06 or a performance
target. ADRs 0027/0028 remain directions; no instrumentation architecture or
additional platform support is introduced.

Run `python tests/headless/measure_baseline.py --output build/sn018/baseline-01`
from the repository root with the already prepared SN-017 dependencies. The
output must not exist. The script freezes hashes and commands in `manifest.json`
before starting engines and rejects changed reference inputs. No build, download,
firmware change, affinity change or priority change occurs during measurement.

Inputs: cycle 27 recovery profile, patched Renode 1.16.1, ngspice 47, bundled
GDB 15.2.90.20241229, MSVC Debug native workers, owned STM32F103C8 ELF/platform,
3.3 V RC with 1 ms time constant, GPIO rise at 2011375 ns and ADC sampling at
4010750 ns, final 4027000 ns/code 3541. All native flags through inspection-time,
steps/lifecycle/pause/guarded remain enabled. Grants remain 5 ms virtual and
100 ms virtual for asynchronous pause; pacing remains 1 ms wall per callback.

Three sequential measured batches, each containing the unchanged harness's
three initial/recreated pairs (18 fresh engine sessions total). No warm-up or
discarded outliers. OS/file caches are uncontrolled; fresh process is not cold
cache. A failed batch is retained and stops the series; do not replace it with a
successful retry. Repetitions on this one host do not establish portability.

Wall measurements use Python monotonic/performance nanosecond clocks. Total
batch latency spans process launch through harness exit, including preparation,
startup, transport, deliberate inspection waits and cleanup. Pause latency reuses
the existing `acknowledgement_wall_ns`: immediately before MI interrupt through
CPU cancellation, analog agreement, notification and stopped-state readback;
it excludes the subsequent 100 ms stability wait and joint commit. Retain the
2 s bound. Report all values, min/median/max and sample standard deviation;
do not estimate tail percentiles from 18 samples or call this kernel throughput.

Memory: external Windows Toolhelp process-tree discovery and
GetProcessMemoryInfo sampling every nominal 100 ms, from harness launch to exit.
Record actual timestamps, PID/name, working set and PrivateUsage bytes. Report
maximum simultaneous sums and per-process observed maxima. Working sets can
double-count shared pages; private committed bytes are not physical RAM. These
are sampled lower bounds on peaks, not leak tests or allocator high-water marks.
Short-lived helpers may be missed; record query errors. Sampler overhead and
host load are part of the observation; record CPU model, OS, logical CPUs and
power scheme, without imposing isolation or a performance acceptance threshold.

For every checkpoint and asynchronous pause, compute absolute RC error against
3.3*(1-exp(-(t-rise)/1 ms)), or zero before rise, and analog endpoint error against
integer virtual nanoseconds. Preserve limits of 10 microvolts and 1 ps. Compare
fixed CPU/time/GPIO/mailbox/register records to cycle 27 and require ADC code
3541. Variable asynchronous pause boundaries and host polling counts are not
fixed-input nondeterminism failures. Preserve all raw logs and earlier evidence.

Retain Python preparation/GDB/fixture ownership, capability restrictions, PDF
startup suppression and PID retry. No CubeIDE run, general autonomous C++
simulator, physical ADC, arbitrary circuit, unpaced support or real-time guarantee
is claimed. Next steps must assess measurement coverage before closing SN-018.
