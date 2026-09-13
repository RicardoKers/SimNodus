# ADR 0032: Bounded mailbox request/result correlation

Date: 2026-09-11. Status: accepted for the SN-017 fixture slice.

## Decision

Add opt-in `--native-readback-request`, requiring `--native-readback-mi`.
`enable-readback-request` selects native request ownership before worker startup.
At final analog-ready time 4027000 ns, after ADC confirmation, `arm-readback`
creates one pending request. The runner returns the exact fixed memory command
and a reserved MI token equal to 1000000 plus the current session generation.
Only one request is permitted per owned runner/GDB session. Its token is distinct
from the harness's ordinary 90-series commands; it is not a globally unique ID.

The host issues the returned command unchanged through its existing GDB process.
It requires one matching result, verifies virtual time remains unchanged, and
forwards the raw result and observed time through `readback-mi`. The native
parser requires the exact expected token as well as the existing fixed mailbox
frame. Normalized and unarmed replies remain rejected. While pending, other
session commands except `readback-mi` and `abort` are rejected. Duplicate arm,
wrong token, late result or invalid mailbox fails without a new joint commit.

Arming starts a native 1900 ms steady-clock acceptance deadline. Check it both
before and after parsing; no operation renews it. The host independently starts
its 1900 ms budget before arming and passes only the remaining time to GDB read,
post-read time query and native result submission. Timeout/failure aborts; a
verified result clears pending state and still requires a separate commit.
Recreation creates fresh runner/GDB processes and resets the request state.

## Evidence and limits

See [protocol](../../tests/headless/READBACK_REQUEST.md) and
[evidence](../experiments/evidence/SN-017-readback-request-summary.json). Real
GDB results carry the native token and match the runner's raw input. Pipe tests
cover wrong/legacy tokens, missing/duplicate requests, pending operations,
expiration, delayed success, and refusal to commit after failure. Parser tests
reject token-prefix collisions, signs and leading zeros. Host tests verify the
remaining budget reaches the virtual-time query.

Correlation is scoped to one owned session. Recreated sessions may reuse a token;
this does not authenticate the host or reject a fabricated prior-session record.
GDB transport, observed-time decoding and stability proof remain host-owned.
There is no timer thread: the native deadline is evaluated on result submission,
while the host enforces the wait budget; EOF/abort cannot publish a new commit.
No debugger operation or allowlist entry is added. ADRs 0014, 0027 and 0028 remain
unchanged. Next extract bounded raw stopped-time validation for the mailbox
request, preserving GDB ownership and separate joint commit scheduling.
SN-017 remains in progress. No commit or publication is authorized.
