# SN-017 native Renode result-ingress slice

The opt-in `--native-results` path reads cancellation results in C++ through
`src/adapters/renode/cancellation_result.*`. It requires the existing
`--session-runner`. The original Python parser remains the default control.
See [ADR 0016](../../docs/decisions/0016-bounded-cancellation-ingress.md).

## Protocol and limits

After a measured CPU observation, `expect-result "<absolute UTF-8 path>" <ms>`
arms one reader before cancellation is sent. `poll-result` returns pending or
validates the published result and acknowledges the CPU through `JointSession`.
The helper replies with `result_status`, `read_retries` and, only after successful
acknowledgement, `cpu_result`. The subsequent GPIO/ADC/analog checks and joint
commit remain separate. An active reader cannot be rearmed; a consumed result
cannot be acknowledged twice.

The harness uses a 1900 ms native deadline under its existing 2000 ms watchdog.
Missing files and temporary Windows sharing/access errors do not renew it.
Only the atomically published final file is parsed, at most 4096 bytes, with
exactly `start`, `end`, `requested`, `unused`, `sinks`, `reason`, `cancelled`.
Only the measured single-sink cancelled profile is accepted. A pre-existing
result, backend error sidecar, malformed result or timeout fails the session.
Native state never promotes CPU acknowledgement to joint commit automatically.

The Windows local-file path is validated; the portable fallback is not Linux
integration evidence. The host watchdog remains responsible for a stalled helper
or OS I/O. Command stdin, ready/progress/notification file handling, GDB relay,
pacing, engine processes and cleanup still belong to the experimental harness.
No full native Renode transport or complete headless orchestrator is claimed.

## Reproduce

Build from the repository root using [the RC protocol](README.md), then use fresh
output names for every run:

```powershell
ctest --test-dir build/sn017/native -C Debug --output-on-failure
python tests/experiments/debugging/joint_persistent.py --renode build/sn016/source-notification --ide C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE --native build/sn016/persistent-native/Debug --analog-runner build/sn017/native/Debug/simnodus_rc_fixture.exe --session-runner build/sn017/native/Debug/simnodus_session_contract.exe --native-results --steps --lifecycle --pause --guarded --output build/sn017/result-recovery-NEW
python tests/headless/result_ingress_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --reference build/sn017/result-recovery-NEW/0/grant-0.txt --output build/sn017/result-ingress-NEW
python tests/headless/session_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --reference build/sn017/session-guarded-01/summary.json --output build/sn017/result-session-regression-NEW
python -m unittest discover -s tests/experiments/debugging -p 'test_*.py'
```

For each fault run, replace `--steps --lifecycle --pause --guarded` with one of
`--fault result-timeout`, `--fault cpu`, `--fault analog`, and use a fresh output
directory. Run the guarded recovery afterwards. For the legacy parser control,
omit `--native-results` and the extended profile flags; retain the session runner.

The timeout fault deliberately arms a result path that the backend does not
publish. Renode's actual third cancellation file is retained untouched. This
models a withheld response, not a claim that Renode failed to cancel. Its absence
must leave two CPU acknowledgements and two commits. CPU process loss before
confirmation also retains two/two; analog loss after confirmation retains
three acknowledgements and two commits. Neither failed path transfers ADC.

## Evidence and handoff

The [cycle evidence](../../docs/experiments/evidence/SN-017-result-ingress-summary.json)
records exact input hashes and raw reports. The initial guarded run passed six
sessions before an explicit UTF-8 IPC adjustment; final recovery and faults use
the same recorded sources. Native-file regression covers 15 cases, including
real sharing locks released before/after the deadline, missing/late files,
atomic publication, Unicode paths, malformed fields and stale accounting.
Fifteen prior CLI cases and all 14 relay/accounting tests remain regression gates.
The three CTest targets cover analog, joint-state and parser/deadline invariants.

Renode 1.16.1 with the selected patches, ngspice 47 and GDB 15.2.90.20241229 are
unchanged. Time/RC tolerances remain 1 ps/10 uV. The final direct-GDB profile
checks six sessions, 54 fixed checkpoints, six retained MI pauses and ADC 3541.
The three legacy parser controls check exact prior fixed-state equivalence.
No IDE startup code was changed or actual IDE matrix rerun. Historical failures
and the PDF startup correction are preserved.

SN-017 remains in progress. Next extract the native cancellation command and
ready/error handshake under a single phase deadline, then process ownership and
orchestration. Keep the experimental reference until each replacement has real
engine equivalence; do not expand unsupported capabilities during extraction.


## Subsequent slice

The [native command/ready cycle](COMMAND_CHANNEL.md) now sends the two bounded
commands and consumes readiness on an opt-in inherited pipe. This result-ingress
cycle remains its historical reference; native process lifecycle is still pending.
