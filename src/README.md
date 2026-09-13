# Simulation source layout

SN-017's bounded headless composition is accepted under
[ADR 0043](../docs/decisions/0043-bounded-headless-extraction-acceptance.md).
Native core contracts, RC/transport adapters, Windows supervisor and composed
application session are implemented. The explicit Python/C# fixture driver still
owns GDB transport, setup and fixture scheduling. See the
[acceptance audit](../docs/experiments/SN-017-acceptance.md). The complete production
kernel/application remains pending. Incremental notes below are historical.

| Planned directory | Responsibility |
|---|---|
| `domain/` | Circuit graph, component/pin/net identity, hierarchy, project data |
| `core/` | Virtual time, events, session states, coordination |
| `coupling/` | Electrical/digital/ADC boundary behavior |
| `adapters/ngspice/` | ngspice API and lifecycle |
| `adapters/renode/` | Renode process/client/platform integration |
| `application/` | Session commands, loading/saving, orchestration |
| `instrumentation/` | Trace storage/export and diagnostics |

Future targets use the `simnodus::` namespace and explicit dependencies. Domain/core must not depend on Qt. See the [architecture](../docs/architecture/README.md).

The seventh slice adds opt-in [native debug reply ingress](../tests/headless/DEBUG_INGRESS.md).
Progress and notification reads now have native closure/deadline checks on that
path; GDB verification and orchestration remain in the harness.

The eighth slice adds [composite native grant transitions](../tests/headless/GRANT_TRANSITIONS.md).
Begin/start and observed-stop/cancel share one runner request each; native analog
catch-up and full orchestration remain pending.

The ninth slice adds [native analog catch-up commands](../tests/headless/ANALOG_COMMANDS.md).
A second inherited pipe emits the acknowledged CPU target; analog reply ingress
remains host-owned. The measured startup PID read now retries transient denial
within its original deadline.

The tenth slice adds [exclusive native analog reply ingress](../tests/headless/ANALOG_INGRESS.md).
The runner frames and validates stdout replies; the host retains exchange, GDB
verification, analytical checks and final scheduling.

The eleventh slice adds [native high/inspection coordination](../tests/headless/EXCHANGE_INSPECTION.md).
The measured GPIO edge and stopped/analog-ready inspections use the native worker
channel. ADC injection, GDB verification and final scheduling remain host-owned.

The twelfth slice adds [bounded ADC preparation/confirmation](../tests/headless/ADC_COORDINATION.md).
The native gate derives channel 0 microvolts at the measured boundary and requires
matching host confirmation; ADC helper invocation/result ingress remain pending.

The thirteenth SN-017 slice adds Windows ADC helper ownership in
`adapters/renode/adc_process.*`: fixed arguments from prepared native state,
bounded stdout/stderr capture, exit/result validation, and job containment before
confirmation. See [ADR 0026](../docs/decisions/0026-native-adc-helper-process.md)
and the [process protocol](../tests/headless/ADC_PROCESS.md). Analytical RC checks,
GDB verification, and joint commit scheduling remain host-owned.

The fourteenth SN-017 slice adds the single-edge analytical oracle in
`core/rc_trajectory.hpp`. Native advance ingress can validate it before accepting
analog state; it has no engine or GUI dependency. See
[ADR 0029](../docs/decisions/0029-native-rc-trajectory-validation.md).

The fifteenth SN-017 slice adds the owned firmware mailbox validator in
`core/fixture_readback.hpp`. Opt-in native verification compares host-normalized
readback with prepared ADC state before the final commit. GDB collection remains
host-owned; see [ADR 0030](../docs/decisions/0030-bounded-native-mailbox-readback.md).

The sixteenth SN-017 slice adds `adapters/gdb/fixture_memory.hpp`, an exact
parser for the measured final mailbox MI result. It decodes little-endian words
before the native mailbox gate; host collection and time association remain
required. See [ADR 0031](../docs/decisions/0031-native-raw-mi-mailbox-ingress.md).

The seventeenth SN-017 slice adds one native mailbox request and a 1900 ms
acceptance deadline. The GDB parser matches the returned token exactly; the host
retains command transport and observed-time decoding. See
[ADR 0032](../docs/decisions/0032-bounded-mailbox-request-correlation.md).

The eighteenth SN-017 slice adds `adapters/gdb/fixture_time.hpp` for the measured
elapsed-time stream and matching command completion. Native integer conversion
feeds the existing mailbox gate under its original deadline. See
[ADR 0033](../docs/decisions/0033-native-raw-stopped-time-validation.md).

The nineteenth SN-017 slice adds `adapters/gdb/fixture_registers.hpp` and native
comparison of the final r0/sp/lr pair before final commit. Read collection and
observation timing remain host-owned. See
[ADR 0034](../docs/decisions/0034-bounded-final-register-stability.md).

The twentieth SN-017 slice adds native sequential ownership of the final
register pair with distinct MI tokens and the original mailbox deadline.
The host retains GDB transport and the 100 ms interval. See
[ADR 0035](../docs/decisions/0035-native-register-pair-correlation.md).

The twenty-first SN-017 slice gates release of the second register request on
100 ms of native steady-clock time after first-result acceptance. Polling retains
the original mailbox deadline. See
[ADR 0036](../docs/decisions/0036-native-observation-interval.md).

SN-017 cycle 22 adds native raw post-pair time confirmation under
[ADR 0037](../docs/decisions/0037-native-post-inspection-time.md). The fixed
request follows the second register reply; its result must satisfy the original
mailbox deadline before inspection is verified. GDB transport remains host-owned.

SN-017 cycle 23 extracts final readback/inspection state into
[FixtureInspection](application/fixture_inspection.hpp), an application-level
fixture coordinator combining GDB parsers and core acceptance rules. The CLI
retains transport and actual commit scheduling. See
[ADR 0038](../docs/decisions/0038-fixture-inspection-coordinator.md).

SN-017 cycle 24 extracts ADC transfer state and helper ownership into
[FixtureAdc](application/fixture_adc.hpp). Read-only ADC facts feed final
inspection; the existing helper adapter retains launch and cleanup. See
[ADR 0039](../docs/decisions/0039-fixture-adc-coordinator.md).

SN-017 cycle 25 extracts analog sequencing and acceptance into
[FixtureAnalog](application/fixture_analog.hpp). It borrows CLI-owned worker
endpoints; adapters still own their handles. See
[ADR 0040](../docs/decisions/0040-fixture-analog-coordinator.md).

SN-017 cycle 26 extracts readiness, cancellation and shared debug transitions
into [FixtureExecution](application/fixture_execution.hpp), borrowing the
CLI-owned Renode channel. See
[ADR 0041](../docs/decisions/0041-fixture-execution-coordinator.md).

SN-017 cycle 27 composes coordinators in
[FixtureSession](application/fixture_session.hpp), which owns joint state and
global dispatch/pending/commit gates. CLI retains startup, endpoints and text I/O.
See [ADR 0042](../docs/decisions/0042-fixture-application-session.md).
