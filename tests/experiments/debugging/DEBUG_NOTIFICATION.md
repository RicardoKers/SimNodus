# Verified cancellation notification to GDB

Date: 2026-09-08. This is an opt-in extension of the experimental managed build,
not a change to the pinned backend or the guarded interrupt policy.

## Why an explicit notification is needed

The time-source cancellation prototype can stop the backend while GDB still
regards the target as running. In the pinned BaseCPU code, Pause returns early
when the CPU is already paused; requesting CPU pause again therefore need not
emit a new halted event. The explicit notification must follow confirmed
cancellation, not stand in for it.

[The notification patch](renode-debug-notification.patch) adds an experimental
method to BaseCPU. Before changing execution mode or invoking the ordinary
HaltReason.Pause event, it requires a paused CPU, a terminated CPU thread and
agreement between the supplied tick, CPU handle time and source time. The normal
GDB stub converts this event into its genuine SIGINT stop reply. No RSP packet
is fabricated by the host. Single-step mode holds execution until a debugger
command follows a new active authorization.

[The opt-in bridge](cancel_notify_bridge.cs) additionally requires an acknowledged
cancelled grant with unused time, exactly one CPU and registered sink, matching
source/sink time and no previous notification for that grant. It clears prior
grant metadata before starting another worker. Premature requests, wrong times
and duplicate notifications are rejected without issuing another stop.
These are experiment invariants for a cooperating single host, not a general
security boundary or a concurrency-safe public API.

## Measured checks

[The probe](cancel_notify_probe.py) repeats the prior cancellation/resume matrix
in three fresh processes and adds these checks:

1. At each held breakpoint, notification before cancellation is rejected.
2. After running cancellation at 8,011,375 ns, GDB still reports a running target.
3. A notification for 8,011,376 ns is rejected; the correct time produces exactly
   one SIGINT stop at PC `0x080000d0`, function `Reset_Handler`.
4. Register and firmware mailbox inspection preserve 8,011,375 ns, and GDB's
   thread query confirms `state="stopped"`.
5. A repeated notification is rejected with no additional stop event.
6. A fresh active 1 ms authorization followed by GDB continue reaches 9,011,375 ns
   in the same backend. Cancellation at that endpoint retains completion
   precedence. All backend listeners are removed on teardown.

All three final repetitions passed in `build/sn016/notify-probe-03`. The original
full GDB/ngspice case on this build produced a JSON result exactly equal to the
unmodified source-build reference, including ADC code 2048 and circuit time
4,027,000 ns. Earlier `notify-probe-01` and `notify-probe-02` also passed their
respective matrices; the final bridge additionally clears previous outcome
metadata before a new grant. See [compact evidence](../../../docs/experiments/evidence/E-05-debug-notification-summary.json)
for source/assembly hashes, raw summary and build-log fingerprints and regression
comparison. Build provenance is shared with the cancellation experiment.

## Reproduction and limits

Follow [the managed-build recipe](SOURCE_BUILD.md), applying the cancellation
patch and then the notification patch from the Infrastructure checkout root.
Overlay a separate `source-notification` package; preserve both prior packages.
The dedicated bridge is compiled only by this probe, so the earlier cancellation
probe remains usable with the earlier assemblies.

```powershell
python tests/experiments/debugging/cancel_notify_probe.py --renode build/sn016/source-notification --ide C:/path/to/STM32CubeIDE --output build/sn016/notify-new
```

This launches the IDE-bundled GDB as a standalone MI process. It is not an actual
CubeIDE session or a guarded relay interrupt implementation. The host verifies
the backend cancellation before explicitly asking for notification. Multi-CPU
ordering, arbitrary concurrent callers, reconnect behavior in this new protocol,
and persistent analog pause/resume remain unvalidated. No analog domain is
active in the notification matrix, so a CPU/source time agreement is not a
joint circuit/MCU commit. ADR 0013's default rejection remains unchanged.
