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
