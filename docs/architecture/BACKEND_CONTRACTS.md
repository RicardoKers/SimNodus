# Backend contracts and electrical coupling

These are SimNodus requirements. Conceptual operation names are not claims about existing engine APIs. The [temporal capability profile](TEMPORAL_CAPABILITY_PROFILE.md) maps the measured E-01/E-02 behavior onto the currently permitted operating modes.

## Minimum contract to demonstrate

| Operation / data | Required meaning |
|---|---|
| `capabilities` | Version, resolution, advancement modes, pause/step limits, I/O types |
| `prepare` | Load validated configuration without uncontrolled advancement |
| `advance` | Requested limit, actual reached time, stop reason, observed events |
| `apply_input` | Value, unit, origin, accepted time; reject input in the past |
| `pause` | Distinguish request from confirmed pause; timeout is not success |
| `reset` | Restore initial domain state as part of coordinated session reset |
| `shutdown` | Release callbacks, handles, threads, and processes after failures |
| `diagnostic` | Stable code, severity, backend, entity, time, useful message |

`advance` must keep requested end, actual end, observation interval, stop reason,
and committed time distinct. `apply_input` must retain producer, crossing,
quantization, command, and effective times where applicable. A timeout or callback
does not synthesize missing actual-time or confirmed-stop evidence.

Use domain IDs and types internally. Adapters own resources; exceptions must not cross C interfaces. Implement only verified operations, not empty methods that report success.

## ngspice

The shared library provides controls and callbacks; temporal behavior must be tested against the selected version. Begin with one session/instance, explicit buffer ownership, and reentrancy rules. Do not assume arbitrary callbacks can safely invoke control commands or that a halt occurs exactly at a requested time. [Official overview](https://ngspice.sourceforge.io/shared.html).

[E-01](../experiments/E-01-results.md) now supplies bounded ngspice 47 evidence: trial-source requests can reverse time, copied data callbacks match accepted analog vectors, integration breakpoints do not pause execution, and zero command returns can accompany a parse failure. Full reset requires reinitialization; background notification must be followed by worker termination before releasing resources. Preserve [ADR 0007](../decisions/0007-ngspice-experiment-contract.md) when implementing an adapter; the conceptual contract above is not implemented yet.

Graph-to-netlist conversion, stable node names, ground reference, external sources, and source mappings belong to the adapter and circuit compiler. Untrusted `.control`, `.include`, and native code models need specific handling, not unrestricted forwarding.

## Renode

Pin the executable, client, and platform revisions. Observe execution, pins, and debugging without adopting register polling as the final integration strategy. A logical GPIO callback does not necessarily expose drive mode, pulls, or alternate-function routing.

[SN-019](../experiments/SN-019-results.md) validates native Windows handshake/time control on an empty machine, reconnects, fragmented transfers, and bounded failure handling. Use its verified loopback server variant; the original server binds every IPv4 interface. A client deadline or disconnect does not establish cancellation of an accepted RunFor.

[E-02](../experiments/E-02-results.md) verifies GPIO callbacks during native `RunFor`: timestamps fall inside the active request, persistent and 20 us input transitions reach firmware, and same-time transitions can collapse into one pending EXTI interrupt. This does not establish an instruction-level stop, lookahead, cancellation, or feedback-causality guarantee. Callback unregistration, concurrent sessions, and production ownership remain unresolved.

[E-04](../experiments/E-04-results.md) verifies an experiment-only F103-compatible ADC subset through the pinned integer-microvolt `IADC` contract. The peripheral performs one 12-bit conversion, saturates at fixed 3.3 V, captures voltage at accepted software start, and exposes measured conversion completion. The pinned generic `STM32_ADC` is not selected. This direct known-schedule path does not establish ngspice electrical coupling or production adapter ownership.

[E-03](../experiments/E-03-results.md) verifies the bounded coupled path. Distinct
direct pulses down to 1 us reached both EXTI edges in its exact firmware/profile;
equal-time opposite levels still collapsed. Sampled threshold feedback met its
declared delay bounds but was late to the MCU, so the adapter contract must expose
the approximation rather than presenting the result as causal.

For the selected contract, exact Renode request/input times lie on the integer-us
grid. Convert a sub-us analog crossing forward to that grid and record the delay.
Equal-time commands for one Boolean pin are resolved before execution; they do not
promise preservation of a zero-duration pulse.

CPU execution is separate from electrical pin modeling. The integration profile owns the board/pin assumptions. Track firmware tests by feature in the [coverage matrix](../research/STM32_SUPPORT.md).

## Electrical boundary

Separate `PinDrive` (what a component tries to impose) from `PinSense` (what the net presents).

| Situation | Proposed initial model |
|---|---|
| Push-pull | Equivalent 0/VDD source with finite, parameterized output resistance |
| Input / high impedance | Disabled driver; leakage/capacitance only if modeled |
| Open-drain | Drive to ground or high impedance; HIGH depends on the external circuit |
| Pull-up/down | Resistance to supply/ground |
| Digital input | VIL/VIH and explicit undefined-zone policy; hysteresis only when modeled |
| ADC | Sampled voltage, reference, and quantization following the actual backend contract |

Resistance, thresholds, and current limits are not universal constants. Use the exact part's datasheet revision and conditions, or label teaching approximations.

When a backend accepts only booleans, `X` requires a stated policy: fail with a diagnostic or retain the last value in an explicitly approximate mode. Do not arbitrarily turn an intermediate voltage into HIGH.

Current is associated with a branch/terminal and orientation, not one scalar for a wire with several connections. Driver conflicts require diagnostics and, where modeled, calculated current; the last callback does not win.


## Extracted bounded contracts (SN-017)

The persistent RC worker, CPU/joint-state gate and
[native cancellation-result ingress](../../tests/headless/RESULT_INGRESS.md)
are implemented and tested in the bounded fixture. The latter validates atomic
file publication, exact cancellation fields and a non-renewable read deadline
before acknowledging the CPU. This does not implement the complete conceptual
backend API above: command transport, general lifecycle and orchestration remain
in the experiment harness. [ADR 0016](../decisions/0016-bounded-cancellation-ingress.md)
defines the measured ingress scope and retained capability restrictions.


The subsequent [native command/ready slice](../../tests/headless/COMMAND_CHANNEL.md)
adds only the two measured start/cancel commands through an inherited pipe and
bounded readiness handling. Process lifecycle, remaining monitor commands and
joint scheduling remain in the harness; this does not implement a general
Renode command API. See [ADR 0017](../decisions/0017-native-cancellation-channel.md).


The [native lifecycle slice](../../tests/headless/PROCESS_LIFECYCLE.md) creates
and reaps the two fixture backends under Windows Job Objects. Process ownership
is separate from virtual-time authority: the experimental host still prepares
the fixture and coordinates debugging/exchange. See
[ADR 0018](../decisions/0018-native-fixture-process-lifecycle.md).

The sixth SN-017 slice moves bounded progress and joint-notification command
emission into the native adapter. Response ingress remains in the harness; see
[ADR 0019](../decisions/0019-native-debug-command-emission.md) for the exact
attestation, deadline and commit boundary.

[ADR 0020](../decisions/0020-native-debug-reply-ingress.md) adds complete native
debug replies on Windows. Observation, notification consumption and joint commit
remain separate; the host retains GDB verification and orchestration.

[ADR 0021](../decisions/0021-composite-fixture-grant-transitions.md) consolidates
fixture grant/open and observation/cancel transitions. Failure is terminal, not
a rollback of backend effects; observation, acknowledgement and commit stay separate.

[ADR 0022](../decisions/0022-native-analog-command-channel.md) adds an analog
worker command channel whose target derives from CPU acknowledgement. The host
still supplies analog replies; emission alone does not establish agreement.

[ADR 0023](../decisions/0023-native-analog-reply-ingress.md) transfers analog stdout
reading exclusively to the native runner on the opt-in path. Native analog
agreement is separate from host exchange/inspection and joint commit.

[ADR 0024](../decisions/0024-native-fixture-exchange-inspection.md) adds native
fixture high/inspection command coordination. Unchanged reply snapshots release
pending operations; host GPIO verification and joint commit remain separate.

[ADR 0025](../decisions/0025-bounded-adc-input-coordination.md) adds bounded ADC
input preparation and confirmation. Native state supplies the input; the host
still invokes the backend helper. Confirmation is separate from joint commit.

## Native ADC helper process slice

[ADR 0026](../decisions/0026-native-adc-helper-process.md) adds Windows-only
`apply-adc`/`poll-adc` execution after native preparation. The explicitly configured
helper receives fixed channel/voltage arguments, inherits only its standard
handles, and runs in a kill-on-close job. Confirmation requires the exact bounded
reply, empty stderr, zero exit, closed streams, and completed job cleanup within
the original 1900 ms acceptance deadline. Failure cleanup has a separate bounded
wait and cannot authorize late success. No synchronous pause, rollback, exact
nanosecond ADC readback, or broader peripheral capability is introduced. Host
analytical validation, GDB checks, and separate joint commit remain pending
extraction.

## Native analytical acceptance

[ADR 0029](../decisions/0029-native-rc-trajectory-validation.md) adds opt-in
single-edge RC validation before native analog acceptance. It uses native worker
observations and high-command state with the existing 10 microvolt/1 ps bounds.
An analytical failure aborts before a new joint commit. This fixture oracle does
not introduce a solver, event prediction, new MCU coupling, or broader temporal
capability. GDB verification and commit scheduling remain host-owned.

## Bounded final readback prerequisite

[ADR 0030](../decisions/0030-bounded-native-mailbox-readback.md) adds an opt-in
native check of the owned final firmware mailbox against the native prepared ADC
code. Verification requires analog-ready at 4027000 ns and completed ADC
confirmation; the final commit requires verification. Host GDB collection and
stability proof remain necessary. This host-normalized interface cannot establish
origin/freshness independently and does not expand debugger or MCU capabilities.

## Raw mailbox result parsing

[ADR 0031](../decisions/0031-native-raw-mi-mailbox-ingress.md) replaces normalized
mailbox input with an opt-in exact MI frame parser. It validates the measured
token/address/range and decodes 32 bytes without third-party domain types. The
raw mode rejects normalized attestations. GDB collection and time association
remain host-owned; the reused token cannot independently prove freshness or
origin. Generic MI transport and request correlation remain unimplemented.

## Mailbox request correlation

[ADR 0032](../decisions/0032-bounded-mailbox-request-correlation.md) adds one
native-owned mailbox request per runner/GDB session. Arming at the final agreed
boundary supplies a fixed command, reserved token and nonrenewable 1900 ms
acceptance budget. Pending requests block other session operations; verification
requires the matching raw token and existing mailbox checks before a separate
commit. Correlation is session-scoped, not host authentication or cross-session
replay defense. GDB transport and raw time decoding remain host-owned.

## Native elapsed-time result validation

[ADR 0033](../decisions/0033-native-raw-stopped-time-validation.md) adds an exact
parser for the final elapsed-time stream and time-command completion. The native
request supplies the monitor command and token; mailbox and time acceptance share
the original deadline. Normalized time is rejected in this mode. Untagged stream
association, GDB transport and inspection stability remain host-owned. No new
monitor operation, time authority, MCU capability or rollback is introduced.

## Final register stability prerequisite

[ADR 0034](../decisions/0034-bounded-final-register-stability.md) adds opt-in
comparison of the two final r0/sp/lr MI records. The native adapter validates
fixed register identities and 32-bit values; the runner requires equality after
timed readback and within its existing deadline before final commit. Host read
association and the 100 ms interval remain necessary. This fixture check is not
proof of distinct observations, host authenticity or a generic MCU register API.

## Native register-pair requests

[ADR 0035](../decisions/0035-native-register-pair-correlation.md) adds one native
sequential pair of fixed register requests after timed readback. Distinct tokens
identify first and second results; the second must preserve the first values
before final commit. Both use the original mailbox deadline. The 100 ms gap and
GDB transport remain host-owned. Correlation is session-scoped and does not
establish host authenticity or cross-session replay protection.

## Native observation interval

[ADR 0036](../decisions/0036-native-observation-interval.md) withholds the second
register request until native steady-clock elapsed time reaches 100 ms. Pending
release polling does not renew the shared deadline or grant simulation time.
This gates command release, not host authenticity or physical acquisition times.
GDB transport and the post-pair time assertion remain host-owned.

SN-017 cycle 22 adds [native post-pair time confirmation](../decisions/0037-native-post-inspection-time.md).
The second register result leaves inspection pending until the fixed raw time
reply is accepted under the original mailbox deadline. Acceptance does not commit.
The host still associates the untagged stream and owns transport; no additional
backend capability or debugger operation is implied.

SN-017 cycle 23 moves the existing final validation protocol into a private-state
[application coordinator](../decisions/0038-fixture-inspection-coordinator.md).
The CLI delegates command acceptance, diagnostics and commit prerequisites; core
contracts and backend transports are unchanged. This fixture composition retains
raw MI parsing and does not constitute a general backend API.

SN-017 cycle 24 introduces the bounded
[ADC application coordinator](../decisions/0039-fixture-adc-coordinator.md).
It owns transfer state, deadline and helper lifetime; CLI worker observations
and eligibility feed it through core values. Final inspection receives read-only
ADC facts. Adapter execution and all backend capabilities remain unchanged.

SN-017 cycle 25 adds the bounded
[analog application coordinator](../decisions/0040-fixture-analog-coordinator.md).
It privately owns worker sequencing and acceptance while borrowing CLI-owned
endpoints. ADC and inspection consume read-only core observations and eligibility
predicates. Worker adapters, deadlines and backend capabilities are unchanged.

SN-017 cycle 26 adds the bounded
[execution coordinator](../decisions/0041-fixture-execution-coordinator.md).
Readiness, cancellation and debugger operations keep shared readers/deadlines
in application. The CLI retains endpoint/session ownership and actual commit;
core accounting and transport adapters are unchanged.

SN-017 cycle 27 adds a bounded
[application session](../decisions/0042-fixture-application-session.md) owning
joint state, the four coordinators and global command gates. It borrows CLI-owned
endpoints. Core/adapters and the text protocol are unchanged; external GDB
transport and fixture progression remain in the experimental harness.
