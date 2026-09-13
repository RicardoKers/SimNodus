# ADR 0025: bounded ADC input preparation and confirmation

Date: 2026-09-11. Status: accepted for the SN-017 fixture slice.

## Decision

Add opt-in `--native-adc` after native exchange coordination. `prepare-adc` takes
no voltage argument. It derives integer microvolts from the last native analog
sample only at 4010750 ns, after CPU/analog agreement and the measured native GPIO
high. Accept only finite input in [0, 3.3] V; round to nearest microvolt, with
half-up ties. Calculate an expected 12-bit code using the existing 3.3 V reference
and saturation at 4095. This expected code is validation metadata; the backend
still receives microvolts and performs its own single ADC quantization.

Preparation is allowed once per fresh session. It opens a 1900 ms confirmation
window and blocks other transitions, including commit, except confirmation or
abort. `adc-applied channel microvolts before_us after_us` must match channel 0,
the prepared value, and unchanged 4010 us helper timestamps. Duplicate, late or
mismatched confirmation is terminal and preserves prior commits. Confirmation
itself does not commit.

The host still invokes the existing measured `e05_control` helper and checks its
successful ADC command response, under a shared 2000 ms host budget covering
preparation, invocation and confirmation. It supplies the confirmation fields;
the native runner does not independently read this helper's response or perform
the ADC write. The helper reports only microsecond timestamps, not exact
nanosecond time or channel/value readback. Existing native CPU/analog state and
actual GDB inspection provide the separate nanosecond boundary checks. This is
not cryptographic or untrusted-host attestation.

## Evidence and remaining scope

See the [protocol](../../tests/headless/ADC_COORDINATION.md) and
[evidence](../experiments/evidence/SN-017-adc-coordination-summary.json).
Pure numerical tests cover rounding, reference levels, saturation and invalid
values; actual pipe tests cover pending-state and confirmation failures. Real
Renode/ngspice/GDB runs validate submitted microvolts and firmware code 3541,
with loss/fresh-recovery and previous-path controls. Engine loss tests precede
ADC preparation; they do not establish live ADC acknowledgement fault coverage.

SN-017 remains in progress. Next extract ownership of the bounded ADC helper
invocation/result ingress. Analytical/GDB checks and final scheduling remain in
the host. Preserve ADR 0014 capabilities, PID retry, PDF suppression and all
historical evidence. No commit or publication is authorized.
