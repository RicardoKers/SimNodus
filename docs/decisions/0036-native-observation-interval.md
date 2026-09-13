# ADR 0036: Native minimum observation-interval gating

Date: 2026-09-12. Status: accepted for the SN-017 fixture slice.

## Decision

Add opt-in `--native-inspection-interval`, requiring `--native-inspection-request`.
`enable-inspection-interval` retains the correlated pair but enters waiting step
4 after accepting the first register result. The runner records a steady-clock
start and withholds `inspection_request` until `release-inspection` observes at
least 100 ms elapsed. An early release poll stays pending and reports a rounded-up
remaining wait; it does not authorize the second read. Successful release enters
step 2 and exposes the second token/command. `inspection_interval_ns` records the
native elapsed duration at that transition.

Waiting permits only release polling or explicit abort. An unsolicited second
result, commit, extra arguments, duplicate/out-of-order release or expired
original mailbox deadline aborts. The 1900 ms deadline is never renewed by first
acceptance, waiting, polling or release. Second-result validation and final commit
retain their existing requirements. Previous modes retain host-enforced timing.

The host polls for release and sleeps no longer than the supplied remaining wait
and its own remaining deadline. It executes the second GDB command only after
native release. This replaces the extra pair's fixed host sleep; independent
legacy inspection remains unchanged. No virtual-time advance or new debugger
operation is introduced.

## Evidence and limits

See [protocol](../../tests/headless/INSPECTION_INTERVAL.md) and
[evidence](../experiments/evidence/SN-017-inspection-interval-summary.json).
Actual pipe tests verify withheld requests, minimum native/host elapsed duration,
early second-result rejection, repeated polls and original-budget expiry.
Real GDB result pairs and native release durations are recorded and compared
with the previous cycle's fixed checkpoints.

The interval is measured from native acceptance of the first result to release
of the second command, using wall monotonic time, not simulation time. It does
not authenticate the host or prove the physical acquisition times of fabricated
records. Tokens remain scoped to one owned session. The runner has no timer
thread; the host drives release polling. GDB transport and the host post-pair
virtual-time assertion remain required. Next extract bounded raw post-pair time
confirmation before final commit, preserving the shared deadline and allowlist.
SN-017 remains in progress under ADRs 0014/0027/0028. No commit or publication is
authorized.
