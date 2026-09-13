# Experimental asynchronous joint pause in CubeIDE

Before execution require the actual IDE DSF session to pass held GPIO boundaries
at 2010875 and 2011375 ns, then resume a fresh 100000 us authorization and issue
MI interrupt after 10 ms of wall time. Require cancellation strictly after the
GPIO high boundary and before the ADC marker at 4010750 ns. Preserve failures
if the wall-triggered interrupt escapes this interval; do not claim repeatable
virtual interrupt timing.

Intercept the actual interrupt at the protected relay. Cancel Renode, verify
unused time and all sinks, then advance the same ngspice circuit to the verified
CPU stop. Require 1 ps clock agreement and analytical RC voltage within 10
microvolts. Only then request the verified backend GDB notification and publish
the joint pause acknowledgement. Require real DSF SIGNAL state and total IDE
interrupt acknowledgement within two seconds. CPU PC/mailbox/time and copied
analog samples must remain stable through 100 ms of IDE inspection.

Resume with separate 5000 us authorizations through the ADC marker and committed
result at 4027000 ns. Transfer the persistent circuit voltage using the existing
boundary sample-and-hold contract; require firmware ADC code 3541. Repeat three
fresh IDE sessions. The asynchronous pause time may vary; the known firmware
stops and ADC result must not. Require normal IDE/analog/backend cleanup and
no remaining owned relay/backend listeners. Run the default guarded lifecycle
regression after the new Java/driver branch. This is DSF/console-model evidence,
not mouse-driven UI verification. Full cooperative fault/lifecycle coverage
remains open, and the pinned default backend/interrupt policy is unchanged.

## Injection handshake refinement

The first candidate exposed stale DSF suspension immediately after continue;
known stops now wait for the expected PC. The next candidate's 10 ms timer
interrupted GDB's internal breakpoint step before CPU time advanced. Preserve
both failures. Before the next run, replace that timer with a read-only monitor
progress handshake: observe global time strictly after the GPIO edge, then
issue actual IDE interrupt. The 2 second acknowledgement limit, strict GPIO/ADC
interval, and numerical tolerances remain fixed. This measures an asynchronous
interrupt after observed execution progress, not a deterministic timed trigger.

## Paced scheduling candidate

The progress-handshake run reached the ADC breakpoint before the IDE interrupt
completed its command path. Keep that failed unpaced run. For the next bounded
candidate, add a separate bridge with 1 ms of wall sleep per TimePassed callback
only during the asynchronous interval. This callback issues no engine command
and changes no virtual time, quantum, firmware, or numerical tolerance. Remove
the pacing subscription when that grant ends. Keep normal intervals unpaced.
Require all earlier clock/state/ADC checks and the two-second pause deadline.
Report this as paced scheduling evidence; an unpaced GPIO-to-ADC asynchronous
pause and general workload/performance coverage are not established by it.

## Measured paced result (2026-09-08)

Three complete sessions passed with matching source and binary hashes:

| Run | Joint pause (ns) | Host acknowledgement (ms) | Capacitor voltage (V) |
| --- | ---: | ---: | ---: |
| joint-ide-04 | 2014000 | 69.2592 | 0.008651140420889714 |
| joint-ide-06 | 2015000 | 50.2238 | 0.011940844158738532 |
| joint-ide-07 | 2012000 | 72.7154 | 0.002061855604771572 |

Each IDE also enforced the full interrupt-to-acknowledgement two-second limit.
Each actual interrupt was intercepted once and never forwarded as a raw CPU
interrupt. Renode cancellation preceded persistent analog advancement and the
verified GDB stop notification. DSF reported SIGNAL, the console model contained
the joint diagnostic, and CPU/analog inspection stayed stable. New grants reached
the unchanged ADC and committed-result times; firmware stored code 3541. All
three IDE and analog processes exited zero, with backend/relay listener cleanup.

Run `joint-ide-05` completed the simulation sequence but failed the full gate:
Eclipse reported a NullPointerException in PerspectiveManager during shutdown
and exceeded the 150-second IDE deadline. Retain it as a failed run alongside
unpaced/development runs 01-03. No IDE preferences or timeout limits were changed.

The default guarded lifecycle regression passed, including recreation/reconnect
at zero; all nine stop and circuit records exactly matched `resume-normal-04`.
All 12 existing transport/authorization tests passed.
[Full observations and hashes](../../../docs/experiments/evidence/E-05-joint-ide-summary.json)
include the failed candidates. General unpaced behavior, cooperative fault/reset/
reconnect coverage, and the intermittent IDE shutdown issue remain open.

Run with a new output directory after preparing the experimental backend and
building `e05_joint_circuit` plus `e05_control` in `build/sn016/persistent-native`:

```powershell
python tests/experiments/debugging/cubeide.py --ide C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE --output build/sn016/joint-ide-next --profile joint --guarded --experimental-renode build/sn016/source-notification
```

The pacing is confined to `cancel_joint_bridge.cs`; the earlier cooperative
bridge and default relay policy are unchanged. This paced result does not
complete SN-016 or authorize production adapter extraction.
