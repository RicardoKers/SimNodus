# Joint pause after an already-observed ADC breakpoint

This separate ordered experiment starts an unpaced 100000 us grant after GPIO
high. The actual IDE must observe PC 0x08000134, virtual time 4010750 ns and DSF
BREAKPOINT before requesting a joint pause through its owned host handshake.
There is no MI interrupt in this ordering. The host cancels the outstanding
grant and requires exact endpoint, remaining-time and sink agreement.

Advance the persistent circuit once to the verified ADC boundary, require 1 ps
clock agreement and 10 microvolt analytical RC accuracy, then transfer its
sample-and-hold voltage once. Publish the joint acknowledgement without invoking
NotifyCancellationProbe or fabricating a SIGNAL stop. Inspect CPU and analog
state for 100 ms, then skip the already-processed ADC stop and resume directly
to the committed-result breakpoint at 4027000 ns. Require final ADC code 3541.

Require three fresh actual-IDE sessions, no intercepted raw interrupt, four
accepted checkpoints, one ADC transfer, two-second request-to-acknowledgement,
zero IDE/backend/analog exits, accepted IDE closure and removed owned listeners.
Run paced pause-first and default lifecycle controls after the shared change.

This verifies breakpoint-before-request ordering only. It does not resolve
an interrupt already in flight when a breakpoint arrives. The strict unpaced
pause profile and its earlier failures remain unchanged; simultaneous arbitration
and fault teardown for this new ordering remain separate gates.

```powershell
python tests/experiments/debugging/cubeide.py --ide C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE --output build/sn016/joint-breakpoint-first-next --profile joint --guarded --joint-wall-pace-ms 0 --joint-breakpoint-first --experimental-renode build/sn016/source-notification
```

The shared evidence field `interrupt_host_elapsed_ns` records host request
detection in this ordered branch; it does not indicate a raw interrupt. The
`breakpoint_first` flag, runner marker and relay packet records distinguish the
request type. No GDB SIGINT notification is issued in this branch.

## Measured ordered result (2026-09-09)

Three fresh sessions (joint-breakpoint-first-02 through 04) completed with the
same sources/binaries. Each preserved DSF BREAKPOINT at 4010750 ns, synchronized
the persistent circuit, performed one ADC boundary transfer and resumed directly
to 4027000 ns with firmware code 3541. The four accepted boundaries were 2010875,
2011375, 4010750 and 4027000 ns. All inspection checks passed; no raw interrupt
or verified SIGINT notification appeared. Host acknowledgement times were
129.6832, 127.8487 and 121.3612 ms. Each IDE independently checked its two-second
deadline, accepted closure and exited zero; analog/backend cleanup passed.

The fresh paced pause-first control also passed. The initial development run
joint-breakpoint-first-01 passed before final driver assertions and Java format
cleanup; repetitions 02-04 provide the matching-hash set.

The default guarded lifecycle regression retained the exact nine stop/circuit
records and reset/reconnect cleanup. [Consolidated evidence](../../../docs/experiments/evidence/E-05-joint-ide-breakpoint-first-summary.json)
records the matching-hash runs and controls. This closes the ordered
breakpoint-before-request case only; in-flight collision and fault handling for
this branch remain open, and SN-016 stays in progress.
