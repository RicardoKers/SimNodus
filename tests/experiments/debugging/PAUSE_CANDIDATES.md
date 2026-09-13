# Public pause API investigation

Date: 2026-09-08. Exploratory backend capability probe, not a change to E-05.

Hold the owned GPIO breakpoint inside a pending 5 ms RunFor. In separate fresh
processes, issue `machine PauseAndRequestEmulationPause false` and
`emulation PauseAll` through the host monitor. Require a monitor-return marker
within three wall seconds, then record three time/grant observations separated
by 100 ms and inspect the firmware. A completed monitor command is not a
cancellation acknowledgement. A still-pending grant fails the cancellation
requirement even if inspection remains stable. A returned grant needs separate
actual-end, completion-reason and same-instance resume validation before any
capability claim. Unexpected errors fail the probe.

Destroy each experimental session and run the existing full plain-GDB normal
case with real ngspice replay in fresh processes afterward. The guarded endpoint
is unchanged; these host-monitor calls are isolated investigation only. No
persistent analog pause or joint commit is claimed by this probe.

Reproduce with `python tests/experiments/debugging/pause_candidate.py --ide
C:/path/to/STM32CubeIDE --output build/sn016/pause-candidate-new`.

## Measured result

`build/sn016/pause-candidates-02` completed both probes and fresh plain-GDB
recoveries. Both monitor commands acknowledged return within 100 ms. All three
observations stayed at 2,010,875 ns with the original 5 ms grant pending. The
firmware/register inspection did not advance time. Cleanup ended each pending
grant with failure and removed the backend listeners. Both fresh recoveries
completed the normal case, including ADC code 2048 and real ngspice replay.

This rejects the claim that either call alone cancels an outstanding RunFor.
It does not prove that every possible same-grant pause/resume protocol is
impossible: that protocol was not measured. The CPU was already held at a GDB
breakpoint, so stability alone cannot be attributed to the candidate API.
The exploratory first run had an incomplete recovery argument setup and used
command echo as its acknowledgement; it is excluded. The final probe requires
the exact executed echo output line and the complete existing recovery setup.

## Audited cause

The pinned `Machine.PauseAndRequestEmulationPause` returns immediately when
`Emulation.IsStarted` is false. `Emulation.RunFor` starts the machines through
`InnerStartAll`, but does not set that flag as `StartAll` does. This explains the
no-op path for the selected synchronous execution route. `PauseAll` stops the
master dispatcher and machines, but `MasterTimeSource.RunFor` loops while the
period remains and the source is not disposed. It has no cancellation-token
check. Its documented interrupt path is disposal of the time source. The
external-control cancellation token belongs to the request worker/waiter; it
is not passed into that synchronous time-source loop.

The three additional pinned source URLs and SHA-256 fingerprints are preserved
in [compact evidence](../../../docs/experiments/evidence/E-05-pause-candidates-summary.json).
They are revision `add012af003a0f620d3da52828262676f374d121`, matching the GDB audit.

## Proposed backend extension, not an approved capability

The next candidate is a separate cancellable advance operation in a source-built
experimental Renode. Preserve the existing RunFor behavior. A cancellation
request must wake a blocked time-source wait, finish reporting any in-flight
sink progress, and return a structured outcome with requested end, actual global
time, per-sink times, stop reason and unused grant. It must not dispose the time
source, publish success for a cancelled interval, or allow a stale grant to be
resumed automatically. Implement this where the time-source loop owns grants;
a token checked only in the external-control wrapper cannot supply this contract.

Before integration, require cancellation at a held breakpoint and while running,
repeated cancellation, a stop coincident with an endpoint, no extra ticks after
acknowledgement, and a new explicitly authorized advance in the same instance.
Repeat deterministic injection cases three times. Return failure whenever the
sink times do not establish the required boundary; do not round a sink forward
or label a CPU-only stop a global stop. Cancellation deadlines bound host waiting,
not a permitted virtual-time overshoot. Test the new source build against the
unmodified binary before attributing differences to the patch.

The analog domain is an independent gate. Existing ngspice tests measured a
100 ns foreground threshold overshoot; an integration breakpoint alone does not
halt. A persistent analog worker must reach the acknowledged MCU boundary,
confirm its accepted state and resume without replay/reset before joint pause
can be claimed. Trial callbacks cannot publish a commit. If analog state already
passed the MCU boundary, rejection remains mandatory unless coordinated rollback
is separately established. No such analog capability is implemented here.

Only after both domains meet the same boundary contract should the GDB relay
translate interrupt into the host pause protocol and emit a stopped indication.
Until then ADR 0013's interrupt rejection remains in force. This proposal does
not authorize production extraction, change tolerances, or complete SN-016.
