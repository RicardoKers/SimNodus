# Actual MI interrupt and ADC breakpoint arbitration

In a separate opt-in joint-race profile, issue an actual MI interrupt after the
existing read-only CPU-progress handshake, without first waiting for the ADC
breakpoint. Require exactly one retained raw interrupt at the protected relay.
Cancel the host grant and verify remaining time and sink agreement.

If the cancelled endpoint is strictly inside the GPIO/ADC interval, retain the
existing verified SIGINT path. If it equals 4010750 ns, require the actual IDE
to report DSF BREAKPOINT with PC 0x08000134 and the same time before the host
advances analog state. Then synchronize the persistent circuit once, transfer
ADC voltage once, retain BREAKPOINT without NotifyCancellationProbe, inspect
stable state and skip the already-handled ADC boundary before resuming.
Any other endpoint or missing IDE confirmation fails within the existing two
second deadline. Preserve 1 ps/10 microvolt tolerances and final ADC code 3541.

Record all runs, including failures and which outcome won. Seek three actual
unpaced breakpoint collisions, plus a paced SIGNAL control and default guarded
lifecycle regression. Actual MI submission and relay interception are required;
this does not establish an exact instruction-time ordering of breakpoint versus
MI submission, nor deterministic pause timing on arbitrary workloads. Fault
teardown for this new branch remains a separate gate. Original strict unpaced
profile failures remain failed.

Run with --profile joint --guarded --joint-race --joint-wall-pace-ms 0 and the
experimental backend, using a fresh output directory. A paced control uses 1.

## Controlled host-delay candidate

If natural repetitions do not exercise breakpoint coexistence, use a separate
50 ms host cancellation-delay injection after the relay has retained the actual
interrupt. Sample CPU time through the read-only bridge during that delay and
require it still strictly precedes 4010750 ns. Then cancel after the declared
host delay. The CPU runs without wall pacing; no breakpoint, firmware or
numerical threshold changes. Require the resulting endpoint to be exactly the
ADC breakpoint, real DSF BREAKPOINT, one ADC transfer and no fabricated SIGINT.
The original two-second request-to-acknowledgement limit still includes the
delay. Record this as injected host scheduling latency, not spontaneous race
frequency or a production scheduling policy. Preserve all natural runs.

## Verified injected-latency result (2026-09-09)

Three delayed-host sessions passed with matching sources/binaries. The relay
retained exactly one real interrupt. A read-only sample after interception still
placed CPU time before ADC. After at least 50 ms of injected host latency,
cancellation ended at 4010750 ns and the IDE confirmed the actual BREAKPOINT
and PC before analog advancement. Each sequence transferred ADC voltage once,
retained four checkpoints, emitted no SIGINT notification, and resumed to code
3541. Host acknowledgement took 263.6071, 265.4288 and 262.9690 ms; each IDE also
checked its two-second request deadline. Closure and listener cleanup passed.
The paced SIGNAL control and exact default lifecycle regression passed.

Natural runs 01-05 passed via SIGNAL. Run 01 preceded the final endpoint/IDE
handshake. Natural run 06 failed because the persistent analog worker missed
the requested 3140000 ns boundary; no third checkpoint or verified pause was
published. Three direct worker repetitions without IDE reproduced the same
failure using the existing binary and boundary sequence. This issue is unresolved
and becomes the next investigation before broader unpaced claims.

[Consolidated evidence](../../../docs/experiments/evidence/E-05-joint-ide-race-summary.json)
retains the injected-latency successes, natural outcomes and isolated failures.
This approves only the measured fixture arbitration under the stated injection,
not arbitrary pause timing, all natural endpoints, or the complete E-05 gate.
Fault teardown for the arbitration branch remains open.

The delayed test adds `--joint-race-delay-ms 50` to the command above. The host
waits before cancelling; the CPU has no sleep callback. The endpoint handshake
prevents stale DSF suspension from being mistaken for a new breakpoint.

## Boundary-failure follow-up

The retained natural-run failure was subsequently resolved by the
[analog boundary rounding correction](ANALOG_BOUNDARY_FIX.md), with 72 worker
cases and six IDE sessions. The original failed run remains unchanged. The
arbitration branch still needs its own fault-teardown validation.

## Delayed injector refinement during fault testing

Later analog-loss recovery showed that a fixed 50 ms delay can still end via
SIGNAL before ADC. The [fault-testing refinement](JOINT_IDE_RACE_LOSS.md) now
interprets the delayed injection as a minimum of 50 ms followed by read-only
observation of the ADC breakpoint before cancellation. The pre-ADC observation
after retained interrupt and the total two-second deadline remain mandatory.
Earlier results retain their original source hashes and timing semantics.
