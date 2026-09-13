# Exclusive native analog reply ingress

Tenth SN-017 cycle; see [ADR 0023](../../docs/decisions/0023-native-analog-reply-ingress.md).
Select `--native-analog-results` with the earlier native options. The harness
passes `--analog-stdout HANDLE`; it never reads this pipe itself on the opt-in path.

| Command | Native effect |
| --- | --- |
| `read-worker` | Arm initial or inspection/exchange reply; allow an already available frame. |
| `advance-native` | Reject stale buffered bytes, arm reply deadline, emit acknowledged CPU time. |
| `poll-worker` | Read available bytes, frame/parse/validate; advance replies establish analog agreement only. |
| `abort` | Terminal state and pending-read cleanup; preserves earlier commits. |

`worker_status` records transport state. Successful polls return `worker_result`
and the exact validated JSON payload as `worker_raw` (line terminator removed).
The harness writes these payloads to the existing log. Inspection responses are
checked against the previous native snapshot. The host still performs analytical
trajectory validation, boundary exchange, actual GDB verification and commit.

The frame limit is 4096 bytes including line ending. The wire parser intentionally
requires the owned worker's field order and compact spelling. It is not a general
JSON parser. Partial frames never establish agreement; coalesced extra data fails.
Request correlation remains limited by the existing protocol's lack of IDs.

## Reproduction

Use the [existing build setup](README.md) and fresh output directories:

```powershell
cmake --build build/sn017/native --config Debug
ctest --test-dir build/sn017/native -C Debug --output-on-failure
python tests/headless/analog_ingress_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --output build/sn017/analog-ingress-pipes-04
python tests/experiments/debugging/joint_persistent.py --renode build/sn016/source-notification --ide C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE --native build/sn016/persistent-native/Debug --analog-runner build/sn017/native/Debug/simnodus_rc_fixture.exe --session-runner build/sn017/native/Debug/simnodus_session_contract.exe --process-runner build/sn017/native/Debug/simnodus_fixture_process.exe --native-results --native-commands --native-debug-results --native-grants --native-analog --native-analog-results --steps --lifecycle --pause --guarded --output build/sn017/analog-ingress-recovery-01
```

Control omits only `--native-analog-results`. Real engine/supervisor loss controls
replace extended pause flags with `--fault cpu`, `analog`, `cpu-owner` or
`analog-owner`. The 25 new actual pipe cases cover partial/bytewise/CRLF frames,
missing/late/lost replies, malformed/duplicate/oversized frames, invalid numeric
values/endpoints/sample counts, stale input, duplicate polling, host bypass,
pending transitions and changed inspection. Native reader closure is checked
with Win32 closed-pipe errors rather than Python's platform-dependent errno.

All earlier command, grant, debug and result regressions remain required. Raw
reports (including unsuccessful test attempts) and hashes are in the
[evidence](../../docs/experiments/evidence/SN-017-analog-ingress-summary.json).
Next extract the smallest bounded exchange/inspection coordination step. The
complete application/kernel, arbitrary circuits and general debugging are pending.

## Subsequent exchange coordination

[Native high/inspection](EXCHANGE_INSPECTION.md) adds command emission under
state and fixture-boundary gates, reusing this reader and its deadlines.

The subsequent [analog coordinator extraction](ANALOG_COORDINATOR.md) moves
worker sequencing out of the CLI while preserving this raw-ingress protocol.
