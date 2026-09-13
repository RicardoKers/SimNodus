# ADR 0033: Native raw stopped-time validation for mailbox readback

Date: 2026-09-11. Status: accepted for the SN-017 fixture slice.

## Decision

Add opt-in `--native-readback-time`, requiring `--native-readback-request`.
`enable-readback-time` selects this mode before startup. Arming still creates one
mailbox request and one 1900 ms deadline. Its response additionally supplies a
fixed monitor command and a time token equal to the mailbox token plus 1000000.
The host executes memory read followed by the supplied time command within the
same remaining budget. The GDB allowlist and allowed monitor operation do not
change.

The host submits `readback-timed` with three unchanged strings: memory result,
elapsed-virtual-time output line and time-command completion. There is no
host-normalized timestamp in this mode; legacy `readback-mi` submission is
rejected. Native parsing requires the exact measured output prefix/suffix,
two-digit hours, valid minutes/seconds and nine fractional digits. Integer
arithmetic converts that bounded representation to nanoseconds. Require the exact
expected time token followed by `^done`, then verify parsed time equals the
final native CPU/analog boundary and the decoded mailbox matches prepared ADC
state. Invalid input leaves parser output unchanged. Verification checks the
original deadline and still precedes a separate commit.

The Python analytical, mailbox and time checks remain independent integration
assertions. The host rejects missing or ambiguous selected time/completion lines.
Only the elapsed-time line and command completion are ingested natively; other
monitor diagnostics such as host load and state text are not interpreted.

## Evidence and limits

See [protocol](../../tests/headless/READBACK_TIME.md) and
[evidence](../experiments/evidence/SN-017-readback-time-summary.json). Pure parser
tests cover all truncation points, invalid time fields, completion mismatch and
unchanged output on failure. Process tests cover wrong/absent/error completion,
changed time, malformed stream, normalized-time bypass, pending operations and
the existing shared deadline. Real memory/time result strings are compared with
GDB logs, and fixed checkpoints are compared with the previous cycle.

MI output-stream records have no token. Their association with the monitor
request remains host-owned; the completion token does not authenticate the
stream or host. Tokens may repeat across recreated sessions. This is an exact
fixture parser, not generic MI support, new monitor semantics or proof of
independent debugger ownership. Existing native accounting and host stability
checks remain required. Next extract bounded final inspection stability checks
while keeping GDB transport in the host. SN-017 remains in progress; ADRs 0014,
0027 and 0028 are unchanged. No commit or publication is authorized.
