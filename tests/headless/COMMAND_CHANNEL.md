# SN-017 native start/cancel and ready handshake

The `--native-commands` option moves the two measured grant/cancel commands and
ready/error interpretation into C++. It requires `--native-results` and the
session helper. The Python path remains available for differential control.
See [ADR 0017](../../docs/decisions/0017-native-cancellation-channel.md).

## Contract

The host passes only a duplicated Renode stdin pipe handle to the helper. C++
owns that copy. `start-native "<path>"` derives the authorized duration from
`JointSession`, arms readiness before writing, and emits one start command.
`poll-ready` accepts complete `active` content and checks `.error`; incomplete
prefixes stay pending. Duplicate start, observe-before-ready, cancel-before-ready,
unknown commands, invalid paths and pre-existing state fail without acknowledgement.
Paths retain the ASCII/no-whitespace fixture restriction and exclude monitor syntax.

After observation, `cancel-native` arms cancellation-result ingress before its
single write. `poll-result` uses that same deadline and acknowledges CPU time
only after the existing native parser and accounting checks succeed. Ready and
successful pipe writes do not acknowledge or commit simulation state. Each phase
uses 1900 ms within the unchanged 2000 ms host watchdog. An OS stall is subject
to host teardown, not an invented synchronous cancellation guarantee.

Helper snapshots include successful `start_writes`, `cancel_writes` and
`ready_status`. Pipe failures also set the terminal session transport error;
a reader may still report pending because no backend result was consumed.
The harness serializes its remaining monitor writes with helper calls. No
concurrent-writer safety or general monitor-command interface is claimed.

## Reproduction

Build using the [RC slice commands](README.md), then run from the repository root
with fresh output directories:

```powershell
ctest --test-dir build/sn017/native -C Debug --output-on-failure
python tests/headless/command_channel_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --output build/sn017/command-pipe-NEW
python tests/experiments/debugging/joint_persistent.py --renode build/sn016/source-notification --ide C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE --native build/sn016/persistent-native/Debug --analog-runner build/sn017/native/Debug/simnodus_rc_fixture.exe --session-runner build/sn017/native/Debug/simnodus_session_contract.exe --native-results --native-commands --steps --lifecycle --pause --guarded --output build/sn017/command-recovery-NEW
python tests/headless/result_ingress_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --reference build/sn017/command-recovery-NEW/0/grant-0.txt --output build/sn017/command-ingress-NEW
python tests/headless/session_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --reference build/sn017/session-guarded-01/summary.json --output build/sn017/command-session-NEW
python -m unittest discover -s tests/experiments/debugging -p 'test_*.py'
```

For faults, replace `--steps --lifecycle --pause --guarded` with `--fault cpu` or
`--fault analog` and use separate directories; run fresh recovery afterwards.
With native commands, CPU loss actually attempts cancellation through the broken
pipe rather than raising immediately after injection. The legacy `result-timeout`
withheld-path injection is intentionally excluded from this option because
native cancel reads the path bound to its grant. Deadline rejection is still
covered by native-file/pipe cases and the earlier real result-ingress experiment.
For the legacy command control omit `--native-commands` and extended profile
flags, retaining `--native-results` and `--session-runner`.

## Measured scope

The [cycle evidence](../../docs/experiments/evidence/SN-017-command-channel-summary.json)
contains raw reports, source/binary hashes and comparisons to the previous slice.
Real Renode/ngspice/GDB exercise six recreated sessions, 54 fixed checkpoints,
six retained MI pauses and ADC 3541. Each session sends ten native starts and ten
native cancellations, consumes ten ready markers and acknowledges ten results.
Successful shutdown checks helper/backend/debugger exits and listener cleanup.

CPU loss preserves two acknowledgements/two commits after the third start;
writing cancellation to the dead CPU fails without a third successful cancel.
Analog loss preserves three acknowledgements/two commits. Fresh sessions recover.
These remain bounded injected failures, not general solver or IDE matrix coverage.

Seventeen real OS pipe cases verify exact command bytes, pacing suffix, partial
and late ready, error sidecars, stale files, invalid order, broken start/cancel,
duplicate cancellation and EOF after inherited-handle cleanup. Fifteen native
file cases, fifteen prior CLI cases, three CTest targets and fourteen relay tests
remain regression gates. Three legacy-command controls retain fixed equivalence.

The engines, 1 ps endpoint tolerance, 10 uV RC tolerance, owned firmware and IDE
startup/PDF suppression are unchanged. No actual IDE launch is needed for this
unchanged startup path. Native process creation/teardown, progress/notification
commands and joint scheduling are not implemented by this slice. Next extract
fixed-fixture process lifecycle, retaining the current harness as a reference.
SN-017 stays in progress; no general capability or production readiness is added.


## Subsequent extraction

The [native backend lifecycle slice](PROCESS_LIFECYCLE.md) now owns opt-in creation
and teardown of Renode and the persistent analog worker. This command slice
remains its historical reference; joint orchestration is still pending.

## Subsequent extraction

The sixth cycle adds [native debug command emission](DEBUG_COMMANDS.md).
Progress and notification response ingress remains in the harness.

The subsequent [execution coordinator extraction](EXECUTION_COORDINATOR.md)
moves shared execution/debug state out of the CLI while preserving this protocol.
