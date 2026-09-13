# Current acceptance: SN-017 bounded headless composition

SN-017 is done for the declared native/Python fixture composition. Read the
[acceptance audit](../../docs/experiments/SN-017-acceptance.md),
[ADR 0043](../../docs/decisions/0043-bounded-headless-extraction-acceptance.md) and
[latest reproduction](APPLICATION_SESSION.md). Native contracts, adapters and
application session are validated with real engines; GDB transport, setup and
fixture scheduling remain harness-owned. SN-018 has an
[accepted bounded local baseline](../../docs/experiments/SN-018-acceptance.md)
with fixed reference scenarios and explicit measurement limitations.
The subsequent [observer control](../../docs/experiments/SN-018-observer-control.md)
exposed a process-ancestry defect. Preserve it as historical evidence; the separate
[corrected identity sampler/control](../../docs/experiments/SN-018-identity-control.md)
passed tests and real-engine measurement with recorded ancestry checks.
This is not a production simulator or a general native-only runner.
The original first-slice notes below are historical.

# SN-017 first extraction: persistent RC runner

This completed slice builds `simnodus_rc_fixture`, a Windows x64 C++20 console
worker, and `simnodus_rc_adapter` from the root CMake project. It extracts the
owned E-05 persistent RC implementation into `src/adapters/ngspice`, with an
engine-independent boundary contract in `src/core/backend_contracts.hpp`.
The original E-05 worker remains frozen as the differential reference.
No Qt or ngspice types enter the core contract. This is a fixture worker, not a
complete C++ joint simulation runner: Renode, GDB relay, grant accounting,
CPU acknowledgement, joint commit and lifecycle ownership remain in the E-05
Python/C# harness. SN-017 remains in progress.

## Build and reproduce

Run from the repository root with the previously verified engine packages:

```powershell
cmake -S . -B build/sn017/native -DSIMNODUS_BUILD_RC_FIXTURE=ON -DNGSPICE_ROOT=D:/03_Projects/01_Actives/SimNodus/build/deps/ngspice/Spice64_dll
cmake --build build/sn017/native --config Debug
ctest --test-dir build/sn017/native -C Debug --output-on-failure
python tests/headless/rc_fixture_regression.py --runner build/sn017/native/Debug/simnodus_rc_fixture.exe --output build/sn017/rc-regression-NEW
python tests/experiments/debugging/joint_persistent.py --renode build/sn016/source-notification --ide C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE --native build/sn016/persistent-native/Debug --analog-runner build/sn017/native/Debug/simnodus_rc_fixture.exe --steps --lifecycle --pause --guarded --output build/sn017/guarded-NEW
python tests/experiments/debugging/joint_persistent.py --renode build/sn016/source-notification --ide C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE --native build/sn016/persistent-native/Debug --analog-runner build/sn017/native/Debug/simnodus_rc_fixture.exe --fault analog --output build/sn017/analog-loss-NEW
python tests/experiments/debugging/joint_persistent.py --renode build/sn016/source-notification --ide C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE --native build/sn016/persistent-native/Debug --analog-runner build/sn017/native/Debug/simnodus_rc_fixture.exe --output build/sn017/fresh-recovery-NEW
python -m unittest discover -s tests/experiments/debugging -p 'test_*.py'
python tools/check_repository.py
git diff --check
```

Each output directory must be new. The runner receives explicit DLL, audio
support and initialization paths followed by target/rise/fall nanoseconds, as
in the original worker. The harness supplies the fixed 10 ms horizon and no
initial edges. Stdin accepts `advance <ns>`, `inspect`, `high`, `low`, `quit`
and the retained experimental `stall` fault command. EOF without `quit` is a
failure. Requests never load project files or download dependencies.

## Contract and scope

Strictly increasing advance requests must precede the configured horizon.
Successful observations expose requested integer nanoseconds, actual solver
seconds and voltage; finite values and 1 ps endpoint agreement are mandatory.
The ngspice equality stop correction is retained. Inspection and drive changes
do not advance the solver. The fixed fixture supports one rise and one fall,
not arbitrary circuits or transitions. Analytical RC tolerance remains 10 uV.
An analog response is not CPU cancellation acknowledgement or joint commit.
No rollback, general causal feedback or independent joint pause is advertised.

The cooperative harness retains exact unused-grant accounting, pacing, retained
MI interruption, fresh-session recovery and ADC boundary sampling. The extracted
worker does not replace that authority. Arbitrary solver recovery remains
unvalidated. The IDE startup/PDF suppression code is unchanged; no IDE matrix
was rerun for this slice because no IDE startup behavior was changed.

## Measured cycle

See [cycle evidence](../../docs/experiments/evidence/SN-017-rc-extraction-summary.json).
The real ngspice comparison passed 72 boundaries with byte-identical stdout
against the frozen worker, independent analytical checks and nine rejected
adversarial requests. CTest boundary invariants and all 14 relay/accounting tests
passed. Real Renode/ngspice/GDB passed three recreated lifecycle repetitions:
six sessions, 54 fixed checkpoints, six retained MI pauses and ADC 3541.
Real analog loss retained two prior checkpoints and was followed by three fresh
successful sessions. Processes/listeners were checked by the existing harness.

Next extract the CPU grant/result contract and joint-state transitions in a
similarly bounded slice, then migrate orchestration. Preserve the E-05 reference
and its evidence until the complete replacement has real-engine equivalence.


## Follow-on slice

The [CPU/joint session contract cycle](SESSION_CONTRACT.md) is now validated.
The original RC cycle above remains historical evidence. Joint scheduling and
transport remain in the harness, with an opt-in live C++ transition gate.

The subsequent [native Renode result-ingress slice](RESULT_INGRESS.md) is also
validated; its opt-in parser/reader leaves command scheduling in the harness.

The [native start/cancel and ready slice](COMMAND_CHANNEL.md) is validated as a
further opt-in path. Process lifecycle and joint scheduling remain in the harness.

The [native backend lifecycle slice](PROCESS_LIFECYCLE.md) adds opt-in Windows
supervision and cleanup for both engines; fixture orchestration remains pending.
