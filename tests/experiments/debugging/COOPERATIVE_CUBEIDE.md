# Actual CubeIDE with the cooperative CPU-only protocol

Date: 2026-09-08. Opt-in experiment; no joint analog pause claim.

The `cooperative` CubeIDE profile requires `--guarded` and an explicit
`--experimental-renode` package built with the cancellation and notification
patches. The default guarded profile retains the original interrupt rejection.
The Java executor runs inside the actual installed IDE's DSF session.

## Execution contract

The host authorizes an initial 100 ms virtual interval and publishes readiness
only after source activation. CubeIDE continues execution, waits 10 wall ms,
and sends a real MI interrupt. This wall delay is an injection setting, never a
virtual-time target. The dedicated relay recognizes byte 0x03 only at a valid
RSP frame boundary, retains it, blocks further execution packets, and signals
the host. It never forwards that interrupt to Renode or creates a stop reply.

The host requests cooperative cancellation, checks requested/start/end/unused
time accounting and agreement of every registered sink, then closes that
authorization. It asks the backend's verified notifier to emit the genuine CPU
halted event only after acknowledgement. Cancellation/notification and the
IDE-observed pause must each meet a two-second wall deadline. A fully consumed
initial grant is failure, not an accepted pause. The actual pause time must be
greater than zero and less than 100 ms; it may vary across repetitions.

CubeIDE must report reason SIGNAL, observe the exact acknowledged time and
preserve PC and firmware mailbox across 100 ms of inspection. The Eclipse
console explicitly identifies this as a CPU-backend result with analog pause
unvalidated. After a separate host authorization becomes active, IDE continue
must advance exactly another 5 ms in the same backend. Verify zero unused time,
source/sink agreement, successful IDE teardown and removal of both backend and
relay listeners. Publish no joint commit.

Run the scenario three times from fresh IDE/backend processes. Also run the
existing guarded lifecycle profile on the unchanged pinned backend after the
executor changes. Compare its nine stops and circuit checkpoints to the saved
normal baseline. The cooperative relay's tests exercise generation/accounting
checks, invalid evidence, unapproved durations, a retained interrupt followed by
blocked execution, and non-ASCII packet rejection on real loopback sockets.

## Reproduction

```powershell
python tests/experiments/debugging/test_cooperative_debug.py
python tests/experiments/debugging/test_gdb_guard.py
python tests/experiments/debugging/cubeide.py --ide C:/path/to/STM32CubeIDE --profile cooperative --guarded --experimental-renode build/sn016/source-notification --output build/sn016/coop-new
```

The source-build steps are in [SOURCE_BUILD](SOURCE_BUILD.md); the genuine GDB
notification is described in [DEBUG_NOTIFICATION](DEBUG_NOTIFICATION.md).
`cooperative_debug.py` is the opt-in relay/coordinator; it reuses the existing
checksum/framing and read/breakpoint allowlist. Unknown or malformed commands
still fail closed. Each new generation needs a fresh host grant; the old
remainder is not reused.

This is controlled DSF command execution and console-model verification, not a
mouse-driven IDE workflow. There is one CPU and one time sink. The backend's
private loopback ports remain reachable by another local process, so the relay
is not a sandbox. Fault/reconnect/reset matrices for this new protocol, arbitrary
concurrency and persistent analog state remain separate gates. The existing
fault/reset evidence for the default profile does not transfer automatically.

## Recorded results

The final runs `coop-ide-02`, `coop-ide-03` and `coop-ide-04` passed using identical
executor, Java and coordinator sources. Observed pause times were 9,000 ns,
825,000 ns and 9,000 ns, respectively; each fresh authorization ended exactly
5 ms later. Each relay recorded one intercepted interrupt with `forwarded=false`,
and DSF reported SIGNAL with stable PC/mailbox inspection. Both backend and
relay listeners disappeared after successful IDE shutdown. The initial
`coop-ide-01` development run also passed its narrower checks.

`coop-default-regression-01` passed the original guarded lifecycle on the pinned
backend: all nine stops and circuit arrays are identical to `resume-normal-04`,
and recreated-session detach/reconnect stayed at zero. Seven default-guard tests
and five cooperative-guard tests passed. See [compact evidence](../../../docs/experiments/evidence/E-05-cooperative-cubeide-summary.json).
These are CPU-only results and publish no joint commit.
