# ADR 0030: Bounded native final mailbox verification

Date: 2026-09-11. Status: accepted for the SN-017 fixture slice.

## Decision

Add opt-in `--native-readback`, requiring native ADC preparation/confirmation.
The host sends `enable-readback` before initial worker consumption. Duplicate,
late or parameterized activation is rejected. Existing callers remain unchanged.

The host retains GDB ownership, the existing read allowlist, and before/after
inspection stability checks. After its final inspection it sends `readback`
with the observed time and eight unsigned mailbox words. The native validator
requires time 4027000 ns and `[0x534E3035, 4, 1, 1, expected_code, 1, 0, 0]`,
where `expected_code` comes from native ADC preparation, never a caller-supplied
expected value. It requires analog-ready at that same CPU boundary, a prepared
ADC input and exactly one completed ADC confirmation. Wrong counts, values,
parse overflow, negative fields, wrong order and repeated verification abort.

With this option enabled, commit at or beyond the final boundary is refused
until readback has been verified. Earlier commits retain their existing gates.
Readback verification does not itself commit or grant time. Snapshots expose
`native_readback` and `readback_verified`. Full session recreation clears both.
The existing session ordering still rejects commits after failure or duplicate
commits without a new grant.

## Evidence and limits

See [protocol](../../tests/headless/READBACK.md) and
[evidence](../experiments/evidence/SN-017-readback-summary.json). Analytical tests
vary all eight words, time and ADC-code bounds. Actual Windows pipe tests cover
ordering, missing confirmation, malformed fields and refusal to commit after
failure. Real Renode/ngspice/GDB runs verify the firmware mailbox with ADC 3541.

This validates host-normalized observations; it does not authenticate their GDB
origin or prove that a caller supplied fresh data. GDB transport, raw reply
ingress, stable register inspection and scheduling remain host-owned. The Python
mailbox assertion remains an independent integration oracle. No new debugger
operation, peripheral capability, rollback or general causal feedback is added.
The mailbox is an owned firmware-fixture contract, not an MCU-family API.

Next extract bounded raw readback ingress while retaining host GDB ownership and
the existing allowlist. Keep direct GDB orchestration and joint commit scheduling
separate. SN-017 remains in progress under ADR 0014; ADRs 0027/0028 are unchanged.
No commit or publication is authorized.
