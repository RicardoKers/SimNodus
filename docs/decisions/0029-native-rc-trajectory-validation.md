# ADR 0029: Native bounded RC trajectory validation

Date: 2026-09-11. Status: accepted for the SN-017 fixture slice.

## Decision

Add opt-in `--native-rc` to the experiment harness. It requires native exchange
and enables `enable-rc` before reading initial worker state. The runner rejects
repeated, late or parameterized activation. Existing callers retain their
reference behavior without activation.

For each native advance reply, validate the owned RC trajectory before calling
`JointSession::analog`. The expected output is zero through the single GPIO high
at 2011375 ns and then `3.3 * -expm1(-(t - rise) / 1 ms)`. Use the existing
10 microvolt inclusive absolute tolerance and 1 ps endpoint tolerance, reject
nonfinite values, and reject times inconsistent with whether the native high
command has been sent. Subtract unsigned time only after checking its order.
The existing worker reply fence prevents progress until the high reply matches.

A rejected trajectory aborts without accepting the analog boundary or permitting
a new joint commit. No extra command, host-supplied voltage, tolerance or edge
configuration can replace the native observation. Snapshots expose `native_rc`
and `rc_checks`; the latter counts successful analytical checks on advance
replies, not inspections or joint commits. The original Python analytical checks
remain independent integration assertions. `expm1` avoids cancellation near the
edge; it does not change the model or acceptance tolerance.

## Evidence and limits

See the [protocol](../../tests/headless/RC_TRAJECTORY.md) and
[evidence](../experiments/evidence/SN-017-rc-trajectory-summary.json). Tests cover
both sides of the voltage tolerance, nonfinite values, endpoint disagreement,
edge ordering, activation ordering and refusal to commit after rejection.
Real Renode/ngspice/GDB sessions retain the fixed trajectory and ADC code 3541.
The first pipe-test attempt used whitespace outside the worker's existing exact
frame format; its failed report is preserved. Only the producer was corrected.

This is an analytical oracle for one owned fixture, not a general circuit solver,
event predictor, peripheral model or new temporal capability. ADR 0014 restrictions
and the architecture principles in ADRs 0027/0028 remain unchanged. GDB readback
verification and joint commit scheduling remain host-owned. Next select the
smallest bounded native readback-verification step; do not expand the debugger
allowlist. SN-017 remains in progress. No commit or publication is authorized.
