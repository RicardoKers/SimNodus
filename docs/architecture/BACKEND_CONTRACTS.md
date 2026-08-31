# Backend contracts and electrical coupling

These are SimNodus requirements. Conceptual operation names are not claims about existing engine APIs.

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

Use domain IDs and types internally. Adapters own resources; exceptions must not cross C interfaces. Implement only verified operations, not empty methods that report success.

## ngspice

The shared library provides controls and callbacks; temporal behavior must be tested against the selected version. Begin with one session/instance, explicit buffer ownership, and reentrancy rules. Do not assume arbitrary callbacks can safely invoke control commands or that a halt occurs exactly at a requested time. [Official overview](https://ngspice.sourceforge.io/shared.html).

Graph-to-netlist conversion, stable node names, ground reference, external sources, and source mappings belong to the adapter and circuit compiler. Untrusted `.control`, `.include`, and native code models need specific handling, not unrestricted forwarding.

## Renode

Pin the executable, client, and platform revisions. Observe execution, pins, and debugging without adopting register polling as the final integration strategy. A logical GPIO callback does not necessarily expose drive mode, pulls, or alternate-function routing.

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
