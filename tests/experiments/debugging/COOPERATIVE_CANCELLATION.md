# Cooperative cancellation: experimental managed Renode build

Date: 2026-09-08. This is a separate exploratory backend, not an approved
replacement for the pinned binary or a completed E-05 joint-pause profile.

## Fixed scope and checks

Use the owned single-CPU STM32 firmware and the original 1 us global quantum.
Run three fresh processes. Start a new 5 ms authorization before asking GDB to
continue; the bridge publishes readiness only after source-side activation.
Cancel at GPIO PC `0x080000f8` / 2,010,875 ns and GPIO edge PC `0x08000102` /
2,011,375 ns. Each cancellation must return within two wall seconds, retain
unused grant time, agree with every registered master time sink, and preserve
register/memory inspection and time during a 100 ms observation interval.
Repeat each cancellation request without advancing state.

In that same process, remove the breakpoints and authorize a fresh full 5 ms:
it must end at 7,011,375 ns with zero remainder. Then authorize 5 ms with a
cancellation signal at +1 ms while the CPU runs: require end 8,011,375 ns and
4 ms unused. A time-progress callback may signal the token only, never control
an engine or publish a commit. An independent host time sample 100 ms later
must match. Finally, authorize 1 ms and signal cancellation exactly at its end:
require end 9,011,375 ns, zero remainder and completion precedence even when
the token is set. Record the GDB thread state rather than fabricating a stop.
Verify listener cleanup. No analog domain participates in this cancellation
probe; the existing normal GDB/ngspice case validates source-build references.

## Implementation boundary

[The patch](renode-cancellation.patch) adds a separate cancellable operation to
Emulation and MasterTimeSource. It waits for in-flight sink reporting, checks
the token between dispatch iterations and wakes a blocked outer wait. It
returns unused time without disposing the source. The original RunFor is
unchanged. [The bridge](cancel_bridge.cs) owns one experimental worker and
combines requested/start/end/remainder with sink observations and stop reason.
[The probe](cancel_probe.py) supervises real GDB and the modified backend.

Readiness ordering is part of this candidate protocol. A run that sent debugger
continue before source activation passed once and then failed to resume; it is
excluded from passing evidence. Neither the patch nor this bridge is a general
concurrency-safe public API: competing callers, arbitrary sink hangs, multiple
machines/CPUs, tiny residual grants, disposal races, wall-triggered cancellation
and long-session robustness remain unvalidated. A two-second host deadline does
not promise an interruptible wait inside every possible peripheral or time sink.

A paused backend does not imply a GDB stopped notification. That notification
and actual guarded CubeIDE integration remain separate gates. Persistent
ngspice state must also reach and resume from the identical accepted boundary;
fresh circuit replay is not evidence for that capability. Keep ADR 0013's
interrupt rejection and SN-016 open until the complete contract is measured.

## Final measurements

`build/sn016/cancel-probe-06` passed all three repetitions. Held-stop cancellation
preserved the two expected times and returned 2,989,125 ns / 4,999,500 ns unused.
The fresh full interval ended at 7,011,375 ns; running cancellation ended at
8,011,375 ns with 4,000,000 ns unused; the endpoint race completed at 9,011,375 ns
with zero unused time. The sole registered time sink matched global time in
all outcomes. Every backend listener was removed at teardown.

After host-only running cancellation, GDB rejected `-thread-info` because it
still considered the target running. The probe records that actual response;
its successful status means the backend cancellation checks passed, not that
GDB pause integration passed. That distinction must survive future integration.

Development runs are separate evidence: `cancel-probe-01` exposed the ordering
failure; `cancel-probe-02` passed the initial three held-stop repetitions;
`cancel-probe-03` wrongly issued another debugger continue while GDB was already
running; `cancel-probe-04` passed the extended backend matrix;
`cancel-probe-05` wrongly expected thread inspection to succeed in GDB's running
state. The final probe records that rejection explicitly. None of these failures
is represented as a complete joint-pause pass.

See [the build recipe](SOURCE_BUILD.md) and [compact execution evidence](../../../docs/experiments/evidence/E-05-cooperative-cancellation-summary.json)
for source revisions, native-library and SDK provenance, source/assembly hashes,
raw-log fingerprints, exact normal-case comparison and repeated outcomes.
