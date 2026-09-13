# ADR 0024: native fixture high and inspection coordination

Date: 2026-09-11. Status: accepted for the bounded SN-017 fixture.

## Decision

Add opt-in `--native-exchange`. The harness routes analog `high` and `inspect`
through native commands rather than writing worker stdin itself. Both arm the
existing exclusive reader before emission, reject buffered stale data and reuse
the unchanged 1900 ms reply deadline. Pending replies block further transitions
except polling and abort. Successful replies must exactly preserve the last
accepted worker snapshot; neither command commits or advances time.

`high-native` is allowed once per fresh session, only after analog agreement at
the measured GPIO edge, 2011375 ns. The host still verifies the real GPIO register
before requesting this exchange. This is not autonomous GPIO event detection or
arbitrary electrical coupling. A write consumes the single-edge allowance even
if the reply later fails; failure is terminal and cannot roll the backend back.

`inspect-native` requires initialized worker state and a stopped or analog-ready
session. The worker snapshot must match committed time when stopped, or CPU
acknowledged time when analog-ready. Inspection during an active grant or before
analog agreement is rejected. The host still controls the observation interval,
actual GDB checks, ADC boundary injection and final commit scheduling.

## Evidence and remaining scope

See the [protocol](../../tests/headless/EXCHANGE_INSPECTION.md) and
[evidence](../experiments/evidence/SN-017-exchange-inspection-summary.json).
Actual pipe tests exercise ordering, duplicate writes, stale/changed/missing
replies and broken channels. Real engine/supervisor loss and fresh recovery run
against the same bounded fixture and are compared with the previous host-write
path. The previous reader limitations, including missing transaction IDs, remain.

SN-017 remains in progress. Next extract the bounded ADC boundary-input
coordination while retaining host GDB verification and separate joint commit.
No capability expansion, PDF/startup retry change, commit or publication occurred.
