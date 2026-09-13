# ADR 0022: native bounded analog catch-up command

Date: 2026-09-10. Status: accepted for the Windows SN-017 fixture slice.

## Decision

Add an inherited analog-worker stdin channel to the session runner, selected by
`--native-analog` in the harness. `advance-native` takes no timestamp: it derives
the target from acknowledged CPU state. It requires CPU acknowledgement, no
pending advance, and a target strictly inside the existing 10 ms RC fixture.
The channel writes only `advance target`; it does not forward arbitrary text.

The host still reads the worker's JSON response and submits the existing analog
observation. With the analog channel enabled, such observation requires a pending
native advance and must arrive within 1900 ms of successful command emission.
Existing finite-value and endpoint checks apply. A duplicate advance, unsolicited
or late observation, broken pipe or wrong endpoint aborts without a joint commit.
Successful pipe writing is not backend acknowledgement. Failed or late execution
cannot be rolled back; fresh recovery remains mandatory after terminal failure.

GPIO exchange, ADC injection, inspection and commit remain separate. The host
retains analog response reading/logging, actual GDB verification and scheduling.
This is command ownership extraction, not complete native analog orchestration.
The pre-existing path is retained as the differential control. Parent-side handle
duplication cleans up both handles even if creating the second one or launching
the helper fails. The native channel owns its duplicate through RAII.

## Measured incidental failure

The first real recovery run failed when the atomically published supervisor PID
file was briefly inaccessible. Preserve that failed report. Startup now retries
only missing/access-denied reads under its original five-second deadline; malformed
PID data remains terminal. Real Windows sharing-lock tests and process lifecycle
regressions validate this fix. No retry budget is renewed. The precise source of
the transient access denial was not identified.

## Evidence and remaining scope

See the [protocol](../../tests/headless/ANALOG_COMMANDS.md) and
[evidence](../experiments/evidence/SN-017-analog-command-summary.json).
ADR 0014 capabilities, engine/fixture inputs and IDE PDF suppression are unchanged.
No general circuit support, unpaced debugging or rollback is inferred. SN-017
remains in progress; next extract bounded analog reply ingress before further
orchestration. No commit or publication is authorized.
