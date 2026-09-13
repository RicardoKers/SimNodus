# ADR 0023: exclusive native analog reply ingress

Date: 2026-09-10. Status: accepted for the Windows SN-017 fixture slice.

## Decision

Add opt-in `--native-analog-results`. The session helper receives the worker's
stdout through a third explicitly inherited handle; the Python analog reader
thread is not started on this path. There is exactly one reader. Startup and
inspection/exchange replies also go through the native helper to avoid competing
readers. The host retains command scheduling and stores the validated raw payload
returned by the helper in the existing analog log.

`advance-native` arms the native reader before command emission and rejects any
already-buffered reply. `poll-worker` uses Windows `PeekNamedPipe` and reads only
available bytes, accumulating one newline-delimited frame under a fixed 1900 ms
deadline. LF and CRLF are accepted. Incomplete frames remain pending; frames over
4096 bytes, multiple coalesced frames, malformed values, pipe loss and expiry
abort without joint commit. No poll renews the deadline.

Parse the owned worker's exact ordered four-field wire format (`time_ns`,
`actual_s`, `output_v`, `samples`), not a general JSON object API. Reject unknown,
missing or duplicate fields, invalid JSON number syntax, integer overflow and
nonfinite values. Existing 1 ps endpoint checks apply. Initial state must be all
zero; advance replies must match acknowledged CPU time and increase sample count.
Inspection/exchange replies must exactly preserve the last accepted snapshot.

A successful advance reply moves the native session to analog-ready. No host
`analog` attestation is accepted on this path. Other transitions are blocked
while a reply is pending, except polling and abort. Abort permits clean shutdown
without draining the worker. Native agreement does not imply analytical RC
validation, GPIO/ADC exchange, GDB inspection or commit; these remain host-owned.

## Validation and remaining scope

See the [protocol](../../tests/headless/ANALOG_INGRESS.md) and
[evidence](../experiments/evidence/SN-017-analog-ingress-summary.json).
Actual fragmented pipes and real Renode/ngspice/GDB runs validate the slice.
Early test failures reflected closed-pipe diagnostic expectations and an oversized
test producer blocking before the reader; all attempts are retained. The final
producer streams bounded chunks. Pending-read abort cleanup was also tightened.

The protocol has no transaction identifiers; it relies on one outstanding request
and the owned worker's one-response behavior. Identical delayed inspection replies
cannot independently prove request identity. This is not arbitrary worker support.
Linux ingress, general debugging and full native orchestration remain pending.
The previous PID retry/PDF fixes, all evidence and ADR 0014 capabilities remain
unchanged. SN-017 remains in progress; no commit or publication is authorized.
