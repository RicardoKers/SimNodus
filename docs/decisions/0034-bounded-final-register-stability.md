# ADR 0034: Bounded native final register stability

Date: 2026-09-12. Status: accepted for the SN-017 fixture slice.

## Decision

Add opt-in `--native-inspection`, requiring `--native-readback-time`.
`enable-inspection` selects the mode before worker initialization and retains the
existing timed mailbox request. After successful native timed readback, the host
submits `verify-inspection` with the two unchanged final register MI results
already collected around its 100 ms observation interval.

The GDB adapter accepts the measured token-93 result containing exactly register
numbers 0, 13 and 14 in that order, with one-to-eight hexadecimal digits per
32-bit value. It stages parsed values and leaves output unchanged on failure.
The runner requires identical parsed r0/sp/lr values, final analog-ready state,
completed native readback and the original mailbox deadline still unexpired.
Missing, malformed, changed, duplicate or late verification cannot create a new
joint commit. Verification sets `inspection_verified` without committing. In this
mode the final commit requires both readback and inspection verification.
Earlier modes retain their existing requirements.

## Evidence and limits

See [protocol](../../tests/headless/INSPECTION.md) and
[evidence](../experiments/evidence/SN-017-inspection-summary.json). Tests cover all
truncation points, invalid/oversized values, modification of each register,
malformed before/after frames, missing verification, duplicate/early/late
submission and final commit refusal. Real GDB inputs are compared with the final
two register records in each log and fixed checkpoints with the preceding cycle.

Native equality is semantic; the host retains its existing stricter raw-response
comparison. Register collection, token-93 request association, the 100 ms interval
and PC/mailbox/time/analog stability checks remain host-owned. Supplying the same
record twice cannot independently prove two reads or an elapsed interval; this
slice does not authenticate the host. The reused token does not correlate the
two individual reads natively. The deadline bounds acceptance after arming, not
the earlier observation interval. This is an owned ARM fixture contract, not a
generic MCU register API or broader debugger capability.

Next extract bounded ownership/correlation of the final register pair while
retaining GDB transport in the host, the allowlist and separate joint commit
scheduling. SN-017 remains in progress; ADRs 0014, 0027 and 0028 are unchanged.
No commit or publication is authorized.
