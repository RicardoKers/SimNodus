# Native post-pair time confirmation

Add `--native-inspection-time` to [native interval gating](INSPECTION_INTERVAL.md)
and retain all prerequisites. The harness activates `enable-inspection-time`.
After the second matching register result, step 5 keeps `inspection_verified`
false and exposes `inspection_time_request` with token `5000000 + generation`.
Execute its fixed elapsed-time command and submit:

```text
inspection-time-result "<raw elapsed-time MI stream>" "<matching token>^done"
```

Use the existing quoted-string escaping for both records. The native parser
requires 4027000 ns, matching the CPU boundary, within the original 1900 ms
mailbox deadline. Acceptance sets step 3 and verifies inspection without committing.
Only this reply or abort is allowed in step 5. The host retains transport and
untagged-stream association, plus an independent time assertion. See
[ADR 0037](../../docs/decisions/0037-native-post-inspection-time.md).

## Reproduction and results

Use the full command and setup in [ADC process](ADC_PROCESS.md), adding
`--native-rc --native-readback --native-readback-mi --native-readback-request
--native-readback-time --native-inspection --native-inspection-request
--native-inspection-interval --native-inspection-time` with a fresh output directory.
This cycle uses `build/sn017/inspection-time-guarded-01`, four
`inspection-time-<fault>-loss-01` directories and `inspection-time-recovery-01`.
Fault runs use `--fault cpu`, `analog`, `cpu-owner` or `analog-owner` and omit
steps/lifecycle/pause/guarded flags. Preserve all prior reports.

```sh
cmake --build build/sn017/native --config Debug
ctest --test-dir build/sn017/native -C Debug --output-on-failure
python tests/headless/inspection_time_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --output build/sn017/inspection-time-cases-01
python -m unittest discover -s tests/experiments/debugging -p "test_*.py"
```

Choose unused output paths when repeating. Fifteen pipe cases cover valid
confirmation, wrong/legacy tokens, changed time, malformed stream, missing/error
completion, duplicate/early result, missing result, extra argument, late result,
original-budget expiry, normalized bypass and another pending operation.
Nine CTest targets and 18 Python unit tests passed. Earlier interval, request,
inspection, timed/mailbox/raw/normalized readback, session, RC and ADC suites passed.

The [evidence](../../docs/experiments/evidence/SN-017-inspection-time-summary.json)
contains report bodies, paths, hashes and raw GDB comparisons. Six recovery
sessions passed 54 fixed checkpoints, 60 native RC checks and six joint pauses,
with ADC 3541. All fixed records match the previous cycle. Real loss controls
precede inspection; pipe tests exercise post-pair failures. PDF suppression and
startup PID retry hashes remain unchanged. Host association is not authenticated;
this bounded fixture does not validate general firmware or physical ADC behavior.

The subsequent [coordinator extraction](INSPECTION_COORDINATOR.md) preserves this
protocol while moving final validation ownership out of the CLI.
