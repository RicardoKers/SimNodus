# Native final mailbox verification

`--native-readback` requires `--native-adc` and its prerequisites. The harness sends
`enable-readback` before initial worker consumption and then, at the final stable
GDB inspection, `readback <time_ns> <word0> ... <word7>` using decimal integers.
No expected ADC code is supplied: the runner uses native prepared state. See
[ADR 0030](../../docs/decisions/0030-bounded-native-mailbox-readback.md).

Acceptance requires analog-ready at 4027000 ns, completed ADC confirmation, and
the owned eight-word mailbox with the prepared code. A successful reply sets
`readback_verified` without committing. An enabled final commit requires that
verification; any invalid transition aborts and cannot create a subsequent
commit. Host GDB collection and before/after stability remain required. This
interface alone cannot prove origin or freshness of host-supplied fields.

## Reproduction

Use the verified setup and full real-engine command from
[ADC process](ADC_PROCESS.md), adding `--native-rc --native-readback` and a fresh
output directory. This cycle uses `build/sn017/readback-guarded-01`, the four
`readback-<fault>-loss-01` reports, and `readback-recovery-01`. Fault runs select
`--fault cpu`, `analog`, `cpu-owner` or `analog-owner` and omit incompatible
steps/lifecycle/pause/guarded flags. Exact engine hashes and raw reports are in the
[evidence](../../docs/experiments/evidence/SN-017-readback-summary.json).

```sh
cmake --build build/sn017/native --config Debug
ctest --test-dir build/sn017/native -C Debug --output-on-failure
python tests/headless/readback_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --output build/sn017/readback-cases-01
```

Choose an unused directory when repeating. There are 16 pure validator cases and
16 pipe cases. Pipe cases exercise valid confirmation followed by a separate
commit, missing readback, wrong code/magic/time, short/extra/negative/overflow
fields, repeated or early verification, absent/pending ADC preparation, disabled
activation and duplicate/late activation. Real backend-loss controls occur
before final readback; the pipe cases independently cover final-gate rejection.
Preserve all earlier evidence, startup PID retry and PDF suppression.

The subsequent [raw MI slice](READBACK_MI.md) optionally replaces normalized
mailbox words with the exact GDB result line and native little-endian decoding.
The ADC confirmation and separate commit gates above remain in force.
