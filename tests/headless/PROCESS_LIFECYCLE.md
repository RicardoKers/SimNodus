# SN-017 native fixture process lifecycle

`--process-runner` routes both Renode and the persistent analog worker through
`simnodus_fixture_process`. Their real PIDs remain available for independent
listener checks. The existing fixture preparation, GDB commands and timeline
stay in the harness. See [ADR 0018](../../docs/decisions/0018-native-fixture-process-lifecycle.md).

## Ownership and records

The supervisor starts a child suspended, assigns a kill-on-close Windows Job
Object, atomically publishes its PID, then resumes it. The child inherits only
explicit stdio handles. It receives the same arguments, environment and working
directory as the legacy launch. The supervisor never interprets simulation data.
The existing native cancellation pipe still reaches the actual Renode stdin.

The internal invocation is `simnodus_fixture_process --record <absolute-path>
-- <absolute-executable> [args]`. Fresh records are mandatory. `<record>` contains
the actual child PID; `<record>.exit.json` contains root PID, exit code and forced
flag after the job is empty. `<record>.stop` requests forced native termination.
Temporary records are renamed without overwriting earlier evidence.

Normal backend `quit` commands pass through unchanged. Forced teardown uses the
job; supervisor death also closes the non-inherited job and stops its children.
The Python shim retains its supervisor PID separately and provides existing
process methods to the harness. No PID is used to terminate an unrelated process.
The owner-loss tests wait on an already-open child handle to confirm its exit.
A supervisor killed unexpectedly leaves no successful native terminal record.

Launch publication has a five-second host bound. Backend listener startup keeps
its existing 30-second wait after launch. Native forced root wait and descendant
cleanup each have five-second limits; host kill has a supervisor-kill fallback.
These resource deadlines do not replace CPU cancellation or joint-commit rules.
Host-harness death while a supervisor remains alive is outside this new evidence.

## Reproduction

Build using the [RC fixture protocol](README.md). Use fresh output directories:

```powershell
ctest --test-dir build/sn017/native -C Debug --output-on-failure
python tests/headless/process_lifecycle_regression.py --runner build/sn017/native/Debug/simnodus_fixture_process.exe --output build/sn017/process-lifecycle-NEW
python tests/experiments/debugging/joint_persistent.py --renode build/sn016/source-notification --ide C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE --native build/sn016/persistent-native/Debug --analog-runner build/sn017/native/Debug/simnodus_rc_fixture.exe --session-runner build/sn017/native/Debug/simnodus_session_contract.exe --process-runner build/sn017/native/Debug/simnodus_fixture_process.exe --native-results --native-commands --steps --lifecycle --pause --guarded --output build/sn017/process-recovery-NEW
python -m unittest discover -s tests/experiments/debugging -p 'test_*.py'
```

For individual failures, replace the extended-profile flags `--steps --lifecycle
--pause --guarded` with `--fault cpu`, `--fault analog`, `--fault cpu-owner` or
`--fault analog-owner`. The owner variants require the supervisor option and
kill the actual supervisor, then wait on the actual engine process handle.
Run fresh guarded recovery after faults. For the legacy lifecycle control, omit
`--process-runner` and the extended-profile flags while retaining native
commands/results and the session runner.

The [channel](COMMAND_CHANNEL.md), [ingress](RESULT_INGRESS.md) and
[session](SESSION_CONTRACT.md) protocols provide the additional regression commands.
All remain applicable; no existing evidence should be overwritten.

## Evidence and limits

The [cycle report](../../docs/experiments/evidence/SN-017-process-lifecycle-summary.json)
contains source/binary hashes, raw results and reference comparisons. The initial
real run predates owner-injection helpers; final faults and recovery share their
recorded inputs. The native supervisor binary is the same.

Nine process tests cover argv quoting/Unicode, environment/cwd preservation,
nonzero exit, ignored quit, explicit force, root/descendant shutdown, supervisor
loss, missing executable, stale records and fresh recovery. The tree tests use
owned Python children, not simulation claims. Actual Renode and ngspice runs
validate the supervisor integration, including six recreated sessions, 54 fixed
checkpoints, six retained MI pauses and ADC 3541.

CPU or CPU-supervisor loss retains two acknowledgements/two commits. Analog or
analog-supervisor loss retains three acknowledgements/two commits. No failed
boundary transfers ADC or publishes a joint pause. For owner loss, native terminal
records are intentionally absent and child termination is checked independently.
Fresh sessions recover and process/listener cleanup remains mandatory.

Engines, firmware, pacing, 1 ps endpoints, 10 uV RC tolerance and IDE/PDF startup
suppression are unchanged. No actual IDE matrix was rerun. Native lifecycle is
not a complete C++ fixture application: progress/notification commands, fixture
preparation, GDB relay and scheduling remain in the experimental harness.
Next extract the remaining bounded progress/notification commands before
consolidating the native orchestration loop. SN-017 remains in progress.

## Subsequent extraction

The sixth cycle adds [native debug command emission](DEBUG_COMMANDS.md).
Progress and notification response ingress remains in the harness.
