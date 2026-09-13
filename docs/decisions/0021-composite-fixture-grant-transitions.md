# ADR 0021: composite fixture grant transitions

Date: 2026-09-10. Status: accepted for the bounded SN-017 fixture.

## Decision

Consolidate two host-driven transition pairs behind opt-in `--native-grants`:
`grant-native duration paced "path"` performs begin and native start;
`stop-native observed_ns` performs observation validation and native cancellation.
They reuse the existing command channel, ready/result readers and phase deadlines.
Neither command polls to completion or commits. The host polls ready/results and
continues to own GDB, analog advancement, exchange, inspection and scheduling.

Validate complete input before beginning a grant. Grant capabilities stay at
5 ms fixed or explicitly paced 100 ms; arbitrary monitor commands are not exposed.
The compound stop rejects pending progress, unavailable ready, duplicate pending
cancellation and invalid observation before writing a cancellation command.
Backend acknowledgement remains distinct from observation and joint commit.

This removes host IPC gaps between the paired state transitions. It is not an
atomic backend transaction: a pipe/write/start failure can occur after native
state advances. Such failure is terminal, preserves prior commits and requires
fresh recovery. No rollback is introduced. The old commands remain available as
an explicit differential reference, not a second production API commitment.

## Validation and next boundary

See the [protocol](../../tests/headless/GRANT_TRANSITIONS.md) and
[evidence](../experiments/evidence/SN-017-grant-transitions-summary.json).
Actual pipes test wire bytes, ordering and failure; real Renode/ngspice/GDB runs
test both fixed checkpoints and paced pauses. Capabilities, PDF suppression and
historical evidence are unchanged. SN-017 remains in progress; next consolidate
bounded analog catch-up after CPU acknowledgement, retaining separate exchange,
inspection and commit checks. A complete native orchestrator is still pending.
