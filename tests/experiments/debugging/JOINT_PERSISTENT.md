# Bounded persistent CPU/analog coordination

Predeclare a plain-GDB probe with the experimental cancellable Renode build,
one persistent ngspice RC circuit, and the unchanged E-05 firmware. At each
held firmware breakpoint, cancel the CPU grant and verify its acknowledged
sink/global time before advancing the analog circuit to that time. Publish a
joint checkpoint only after both clocks agree within 1 ps and 100 ms of
inspection leaves CPU PC/mailbox/time and analog copied samples unchanged.
Use a fresh 5000 us CPU authorization for every interval; no abandoned remainder
is reused. Require expected stops at 2010875, 2011375, 4010750, 4027000 ns.

Start GPIO low. At the after-write breakpoint read ODR and apply the observed
high at that accepted boundary. At `adc_read_marker`, take the RC voltage,
round to integer microvolts, and set the owned ADC channel through external
control. Hold this value through conversion. Require firmware to store the
owned peripheral's quantization: min(4095, microvolts * 4096 // 3300000).
This is an explicit boundary sample-and-hold experiment, not a claim of exact
physical ADC aperture timing. Do not allow an unobserved GPIO transition in
this bounded sequence. Compare analog values with the analytical RC trajectory
within 10 microvolts. Three repetitions must have identical deterministic
checkpoint data. Preserve errors and raw logs, including failed candidates.

This is not yet an IDE asynchronous joint-pause test, a general event scheduler,
or validation of fault/reset/reconnect behavior. The default backend and relay
policy remain unchanged. SN-016 remains open until those gates are satisfied.

## Analog-loss injection

With `--fault analog`, terminate the actual analog helper after the third CPU
cancellation and before analog advancement/acknowledgement. Require an error,
nonzero helper exit, only the first two joint checkpoints, and removal of the
Renode listeners. No ADC exchange or third joint checkpoint may be published.
A fresh normal three-repetition run must recover the complete sequence. This
covers this single plain-GDB fault boundary, not the complete IDE fault matrix.

## Measured result (2026-09-08)

Three fresh normal repetitions passed with identical checkpoints. CPU and analog
time agreed at all four boundaries, and 100 ms inspection preserved PC,
mailbox, virtual time, and copied analog samples. At the ADC marker, the
persistent circuit produced 2.8531143502144816 V. External control supplied
2853114 microvolts, and the unchanged firmware stored code 3541. At the final
checkpoint the same circuit produced 2.860317557432128 V. Both processes exited
with status zero, and Renode listeners were removed.

The analog-loss injection passed: the actual helper exited with status 1,
the pending operation failed, and only the first two joint checkpoints remained.
No ADC exchange was performed. The host closed Renode and removed its listeners.
Three subsequent normal runs recovered the full sequence with the same source
and binary hashes. Raw Windows pipe failure text is retained in the evidence.

See [full observations and hashes](../../../docs/experiments/evidence/E-05-joint-persistent-summary.json).
The Renode build provenance remains in [the source-build notes](SOURCE_BUILD.md).
The successful experiment establishes persistent coordination at these held
breakpoints. It does not close the asynchronous CubeIDE joint-pause gate.

Build `e05_joint_circuit` and `e05_control` using the E-05 CMake project, then run:

```powershell
python tests/experiments/debugging/joint_persistent.py --renode build/sn016/source-notification --ide C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE --native build/sn016/persistent-native/Debug --output build/sn016/joint-persistent-next
```

The output directory must be new. Add `--fault analog` for the single real
process-loss case, then run a fresh normal recovery. The `--ide` path supplies
the GDB executable only; this probe does not launch the IDE.
