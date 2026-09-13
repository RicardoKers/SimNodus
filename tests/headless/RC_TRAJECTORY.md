# Native RC trajectory validation

Enable the opt-in harness flag `--native-rc` together with `--native-exchange`
and its prerequisites. The harness sends `enable-rc` before initial worker
consumption. The session runner then validates each advance reply before analog
acceptance. No caller parameters are accepted. See
[ADR 0029](../../docs/decisions/0029-native-rc-trajectory-validation.md).

The fixed model is zero before the native high at 2011375 ns, then a 3.3 V step
through a 1 ms RC. The voltage tolerance is 10 microvolts, inclusive; endpoint
tolerance is 1 ps. The original Python comparison remains an independent test
oracle. Inspection frames continue to require exact unchanged snapshots. This
slice changes neither CPU accounting nor commit scheduling.

## Reproduction

Use the verified Windows engine setup from [ADC process](ADC_PROCESS.md), adding
`--native-rc` to its real-engine command and choosing a fresh output directory.
The cycle records guarded and recovery matrices under
`build/sn017/rc-trajectory-guarded-01` and `rc-trajectory-recovery-01`; four separate
`--fault` runs cover `cpu`, `analog`, `cpu-owner` and `analog-owner`. They retain
native RC validation and omit the incompatible steps/lifecycle/pause/guarded
flags. Exact inputs, hashes and results are preserved in the
[evidence](../../docs/experiments/evidence/SN-017-rc-trajectory-summary.json).

```sh
cmake --build build/sn017/native --config Debug
ctest --test-dir build/sn017/native -C Debug --output-on-failure
python tests/headless/rc_trajectory_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --output build/sn017/rc-trajectory-cases-02
```

Choose another unused output directory to repeat. CTest includes 15 analytical
cases. The nine pipe cases include a reference path with validation disabled,
positive/negative voltage errors, pre-edge error, missing high, and invalid
activation ordering. Rejected results cannot create a subsequent commit.
`rc-trajectory-cases-01` is preserved as a failed producer-format attempt.

No general GPIO, ADC acquisition, unpaced debugging, rollback, or application
support is inferred. Preserve the existing PDF suppression and startup PID retry.

The subsequent [readback slice](READBACK.md) adds final mailbox verification as
an optional commit prerequisite. Analytical validation retains the rules above.
