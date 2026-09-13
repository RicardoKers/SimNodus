# ADR 0014: bounded cooperative joint debugging and extraction gate

Date: 2026-09-10. Status: accepted for the measured single-CPU experimental
profile. SN-016's bounded E-05 gate is complete; SN-017 may begin local extraction.
This is not production compatibility or approval of general unpaced execution.

## Context

The original E-05 transport and ownership contract could not be satisfied by a
raw CPU interrupt against the unmodified Renode build: CPU halt did not cancel
the outstanding host grant. ADR 0013 correctly retained rejection as the default.
Subsequent experiments added measured cooperative cancellation and verified
debugger notification, persistent RC state, actual GDB/IDE stepping, lifecycle,
arbitration and real fault/fresh-recovery checks. No prediction or rollback was
introduced. Failed unpaced candidates remain failed.

The final direct-GDB check now also sends actual MI interruption through the
cooperative relay, rather than only requesting host cancellation. Six recreated
step/pause sequences passed with the same shared engine, guard and bridge inputs.
The IDE matrix passed 35 sessions including four fault classes and recovery.
The final startup-only change suppresses automatic release-note opening in
disposable IDE configurations and retains lifecycle regression evidence.

## Decision and command policy

Retain ADR 0013 for the unmodified/default backend. Accept a separate, explicitly
selected cooperative experiment profile with the patched managed Renode build,
one owned STM32F103C8 firmware fixture, one persistent GPIO-driven RC circuit,
the focused E-04 ADC extension and the audited loopback transport.

| Operation | Selected cooperative behavior |
| --- | --- |
| Attach, fixture registers/memory/source inspection | Allowed through the existing read allowlist; verify unchanged time and state. |
| Fixture flash breakpoints, instruction step, source step-over, continue | Require an explicit host grant; cancel/account it at the observed stop, then synchronize analog state before joint commit. |
| MI interrupt during a grant | Retain exactly one raw interrupt at the relay; do not forward it. Cancel CPU execution, validate accounting/sinks, synchronize analog state, then issue verified SIGINT notification. |
| Breakpoint concurrent with retained interrupt | Require actual IDE BREAKPOINT/PC/time evidence; synchronize once, transfer ADC once, preserve BREAKPOINT, skip the duplicate ADC grant. |
| Reset | While stopped, terminate and recreate the whole session. Verify zero CPU/mailbox/analog state. Never reset only the MCU. |
| Detach/reattach while stopped with no grant | Allowed; no independent advancement. Reattach before a new grant. |
| Active debugger disconnect, CPU loss, analog loss or missing response | Abort; publish no new joint commit. Recover only in fresh processes. |
| Raw monitor/reset/start commands, memory/register writes, reverse execution, ungranted execution or unknown packets | Reject before forwarding; no joint commit. |

The host remains the sole time authority. Normal fixture grants are 5000 us;
the asynchronous test authorization is 100000 us. Discard unused time after
cancellation; resumption requires a new authorization. Require exact CPU/sink
accounting, monotonic boundaries, analog endpoint agreement within 1 ps and RC
error within 10 microvolts. Stable inspection lasts 100 ms. Normal interrupt and
failure diagnosis are bounded by two seconds; the live analog response-timeout
case retains its two-second response wait and five-second total diagnosis limit.
A temporary unreadable CPU result remains unacknowledged under the same deadline.

CPU cancellation acknowledgement is not a joint commit. A failed eighth CPU
grant may have a valid cancellation record while only seven joint checkpoints
remain. CPU process loss before cancellation must retain only seven CPU
acknowledgements. No failed path may transfer ADC or publish a joint pause.

## Pacing, coupling and exact dependencies

Paced SIGNAL pause uses 1 ms host sleep per time callback during the asynchronous
grant. The final arbitration matrix uses initial pacing until retained MI
interruption and a read-only pre-ADC observation; it then explicitly releases
that sleep before awaiting the actual breakpoint. Host arbitration polling is
1 ms and the injected delay is at least 50 ms. These are declared experimental
scheduling constraints, not guaranteed unpaced support or virtual-time changes.

The circuit is the owned 3.3 V, 1 ms time-constant RC fixture. Its known GPIO
high boundary is 2011375 ns; persistent voltage is transferred as rounded integer
microvolts at the 4010750 ns ADC boundary, yielding firmware code 3541 at
4027000 ns. This is boundary sample-and-hold, not physical ADC acquisition or
general live causal feedback. ADRs 0010–0012 retain their coupling restrictions.

Use the measured Renode 1.16.1 source baseline plus the preserved cancellation
and notification patches, pinned native translators, ngspice 47, owned firmware
and workers. The installed IDE reports **2.2.0** despite its 2.1.1 directory;
bundled GDB reports 15.2.90.20241229. Exact source, assembly, worker, firmware,
plugin and executable hashes live in the linked evidence, not a moving package
name. The [managed-build recipe](../../tests/experiments/debugging/SOURCE_BUILD.md)
is required. Binary redistribution and packaging remain separate licensing work.

## Evidence and gate result

- [Original contract](../../tests/experiments/debugging/README.md) and
  [case-by-case gate review](../experiments/E-05-gate-review.md).
- [Final guarded GDB and IDE startup controls](../experiments/evidence/E-05-final-gate-summary.json).
- [35-session extended fault/arbitration matrix](../experiments/evidence/E-05-steps-fault-matrix-summary.json),
  including four preserved unsuccessful predecessors.
- [Direct-GDB shared-bridge regression](../experiments/evidence/E-05-steps-fault-plain-regression-summary.json)
  and the historical [E-05 report](../experiments/E-05-results.md).

The acceptance is a composition of actual measured subprofiles, not relabelling
each raw partial-probe flag as full approval. The original time, consistency,
transport, lifecycle and failure invariants remain mandatory. The final direct
MI check closes command-path reconciliation. Accept SN-016 as done for this
declared profile and SN-017 as ready. Do not mark the M2 application/runner or
the production simulator implemented merely because its experiment gate passed.

## Extraction constraints and revisit criteria

SN-017 should first produce a small headless fixture runner and explicit backend
contracts, preserving granted, observed, CPU-acknowledged and joint-committed
states. Keep Qt and third-party types out of domain/kernel interfaces. Port the
measured protocol in small increments and regress against real engines; fake
backends alone cannot validate extraction. Avoid a broad GUI or generic SDK.

Reopen the capability decision for changed engine/firmware/IDE dependencies,
arbitrary circuits or additional GPIO transitions, unpaced pause, additional
MCUs, solver nonconvergence recovery, reverse execution, in-place reset or a
classroom lesson beyond the fixture. Mouse-driven IDE behavior, authentication,
hostile-local-process isolation and the historical Eclipse log exception remain
outside this approval. Preserve January stabilization and February teaching targets.
