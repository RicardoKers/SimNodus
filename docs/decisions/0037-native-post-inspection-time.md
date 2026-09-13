# ADR 0037: Native post-inspection time confirmation

Date: 2026-09-12. Status: accepted for the SN-017 fixture slice.

## Decision

Add opt-in `--native-inspection-time`, requiring `--native-inspection-interval`.
After accepting the unchanged second register result, the native runner enters
step 5 with inspection still unverified. It emits `inspection_time_request`
using token `5000000 + generation` and the existing fixed GDB command
`-interpreter-exec console "monitor machine ElapsedVirtualTime"`.

Only `inspection-time-result` with two quoted raw MI strings, or explicit abort,
is permitted while pending. Reuse the existing elapsed-time parser to require
the exact stream syntax, matching token and successful completion. The parsed
time must equal 4027000 ns and the acknowledged CPU boundary. Success enters
step 3 and verifies inspection; a separate commit remains mandatory.

The original 1900 ms mailbox deadline covers memory, pre-pair time, both register
reads, the 100 ms native interval and post-pair time acceptance. No transition
renews it. Missing, malformed, replayed, out-of-order or late results abort
without a new joint commit. Earlier opt-in modes retain their behavior.

The host executes the returned command, requires one elapsed stream and one
completion, independently checks time, and forwards those exact records. This
replaces the existing post-pair host query; no debugger operation or allowlist
permission is added. GDB transport and association of the untagged stream remain
host responsibilities. Tokens are scoped to the owned session, may repeat after
recreation and provide no authentication.

## Evidence and next step

See [protocol](../../tests/headless/INSPECTION_TIME.md) and
[evidence](../experiments/evidence/SN-017-inspection-time-summary.json).
Six initial and six recovery sessions passed with real Renode/ngspice/GDB; four
backend/owner-loss controls preserved the last joint commit. All six recovery
post-pair replies match GDB logs after the second register result and before
final commit. Fifteen pipe cases cover the new pending state and shared deadline.

SN-017 remains in progress. Next extract the bounded final readback/inspection
state from the CLI into a testable native coordinator, preserving this protocol,
the shared deadline and separate commit. Keep host transport and fixture limits.
ADRs 0014/0027/0028 remain unchanged; this adds no general unpaced, physical
peripheral, prediction or rollback capability.
