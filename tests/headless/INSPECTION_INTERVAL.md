# Native observation-interval release

Add `--native-inspection-interval` to [register-pair correlation](INSPECTION_REQUEST.md)
and retain its prerequisites. The harness selects `enable-inspection-interval`.
After the first `inspection-result`, step 4 indicates a pending minimum interval
and no `inspection_request` is returned. `release-inspection` stays pending before
100 ms, exposing `inspection_wait_ms`; at or after the threshold it enters step
2 and returns the second request. `inspection_interval_ns` records elapsed native
steady-clock time at release. See
[ADR 0036](../../docs/decisions/0036-native-observation-interval.md).

All release and result acceptance uses the original 1900 ms mailbox deadline.
No polling or waiting renews it. A guessed second token cannot bypass the waiting
state. This is monotonic wall-time gating; simulation time remains unchanged.

## Reproduction

Use the verified setup and full command from [ADC process](ADC_PROCESS.md), adding
`--native-rc --native-readback --native-readback-mi --native-readback-request
--native-readback-time --native-inspection --native-inspection-request
--native-inspection-interval` with a fresh output directory. This cycle records
`build/sn017/inspection-interval-guarded-01`, four separate
`inspection-interval-<fault>-loss-01` reports and `inspection-interval-recovery-01`.
Fault runs select `--fault cpu`, `analog`, `cpu-owner` or `analog-owner` and omit
incompatible steps/lifecycle/pause/guarded flags. Reports, hashes, native intervals
and GDB comparisons are in the
[evidence](../../docs/experiments/evidence/SN-017-inspection-interval-summary.json).

```sh
cmake --build build/sn017/native --config Debug
ctest --test-dir build/sn017/native -C Debug --output-on-failure
python tests/headless/inspection_interval_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --output build/sn017/inspection-interval-cases-01
python -m unittest discover -s tests/experiments/debugging -p "test_*.py"
```

Choose unused output directories to repeat. Fifteen pipe cases cover valid and
repeated polling, premature results, pending commit/inspection, bad release
order/arguments, waiting and second-result timeout, shared-budget expiry and
wrong/replayed/changed second results. Success checks both native elapsed duration
and host elapsed time at release are at least 100 ms. Real backend losses precede
pair arming; controlled pipe cases cover interval failures separately.
The host still owns GDB transport and post-pair time collection. Preserve the
allowlist, PDF suppression, startup PID retry and all earlier evidence.

The subsequent [post-pair time mode](INSPECTION_TIME.md) also requires native
raw elapsed-time confirmation before inspection can permit final commit.
