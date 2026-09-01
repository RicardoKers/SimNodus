# Temporal capability profile

Date: 2026-08-31. Status: accepted SN-013 contract, now evaluated by
[E-03](../experiments/E-03-results.md) and [ADR 0011](../decisions/0011-e03-restricted-feedback.md).
This document defines what the evidence permits an orchestrator to claim. The
E-03 host is experiment code, not an implemented production scheduler.

## Evidence boundary

The profile combines two independent experiments:

- [E-01](../experiments/E-01-results.md) measured ngspice 47 trial requests,
  accepted samples, stop behavior, reset, retry, and worker lifetime.
- [E-02](../experiments/E-02-results.md) measured Renode 1.16.1 integer-microsecond
  advancement, GPIO callbacks, persistent input, a 20 us pulse, and same-time
  input collapse with the owned STM32F103C8 profile.

Neither experiment coupled the engines. Statements below are classified as
**measured**, **selected contract**, **candidate for E-03**, or **unsupported**.

E-03 subsequently coupled the pinned engines. It confirmed P0 replay and measured
P1 as an approximation; it did not establish P2 live causal feedback.

## Time representation and conversion

`SimTime` is an unsigned, checked integer count of nanoseconds from session start.
It is the interchange representation at orchestration boundaries. One-nanosecond
representation does not imply one-nanosecond physical accuracy or backend control.
Overflow, negative external values, nonfinite values, and out-of-range conversions
are errors.

| Clock or timestamp | Meaning | Contract |
|---|---|---|
| SimNodus time | Ordered event and commit time | Integer ns; never silently moved backward |
| Renode master time | External-control request ends and GPIO observations | Measured integer us; exact requests require `SimTime % 1000 == 0` |
| ngspice trial time | Tentative source query during integration | Floating-point seconds; may repeat or decrease |
| ngspice accepted time | Accepted transient output sample | Floating-point seconds; monotonic in E-01 |
| Host wall time | Deadlines, process supervision, and performance | Never substitutes for virtual time |

Convert an accepted ngspice timestamp to integer nanoseconds with checked
round-to-nearest and retain the raw floating-point value in evidence. Reject a
conversion whose residual exceeds 0.5 ns. A value sent to Renode is placed on the
next integer-microsecond boundary with ceiling conversion; record the resulting
quantization delay in `[0, 1 us)`. Nearest rounding is forbidden for input
application because it could place an event in the consumer's past.

`t_committed` is the latest time at which every participating domain has a state
accepted by the selected profile. Trial solver state, granted Renode time, last
observed callback time, requested stop time, UI time, and wall time are separate
fields.

## Measured backend capabilities

| Capability | ngspice 47 evidence | Renode 1.16.1 evidence | Orchestrator claim |
|---|---|---|---|
| Bounded advance | A breakpoint forces an integration point but does not stop; a foreground condition overshot by 100 ns in E-01 | Successful `RunFor` calls ended at the exact requested integer us in E-02 | Exact common `advance_until` is unavailable |
| Event observation | `SendData` matched accepted vectors; external-source requests were tentative and reversed time | GPIO callbacks arrived during active `RunFor` and within its requested interval | Preserve backend event kind and observation context |
| Input application | External sources must be pure functions of requested trial time | Boolean inputs apply before subsequent CPU execution; same-time edges may collapse | Reject past input; coalesce equal-time Boolean commands to a resolved final level |
| Pause | Foreground condition and asynchronous halt are different; integration breakpoint is not pause | No synchronous cancellation of an accepted `RunFor` was established | Joint pause is unsupported until every domain confirms a common boundary |
| Failure | A zero return can accompany diagnostic failure; full reset invalidates state | Timeout or disconnect does not prove that an accepted run stopped | Mark domain state unknown and shut down/recreate unless explicitly recovered |
| Rollback | A local solver retry rejects tentative analog state only | No checkpoint or rollback API was established | Coordinated rollback is unsupported |
| Prediction/lookahead | No source-event prediction contract | No future GPIO event prediction or I/O-boundary stop | General conservative feedback is unsupported |

Callback code may copy data into bounded owned storage and signal the controlling
worker. It must not issue engine control commands, publish a joint commit, or
mutate the editable project. Callback thread identity is backend-specific.

## Supported and candidate operating profiles

### P0: known-schedule replay

**Selected contract: causality-preserving for the declared inputs.** All source
transitions are known before either consumer advances. The orchestrator orders and
applies them before granting the consumer time beyond each transition. Analog
values remain subject to the declared numerical tolerance. This profile supports
reproduction and the first E-03 phase; it is not evidence of live feedback.

### P1: bounded sampled exchange

**Measured by E-03: approximate.** Domains exchange their last resolved values
at a configured quantum `Q`. Every observation records producer time, host
observation time, consumer application time, and the consumer time when the value
became effective. The profile is acceptable only with a measured delay bound and
pulse restrictions for the exact configuration.

Reducing `Q` can reduce delay, but cannot establish causal correctness, preserve
every short pulse, or repair a consumer that already passed the causal event. A P1
result must be labelled sampled/approximate in traces, reports, and the UI.

### P2: live causal feedback

**Unsupported.** A general loop in which a newly discovered electrical crossing
can affect firmware that has already advanced requires verified lookahead, an
I/O-boundary stop, or coordinated rollback. E-01 and E-02 establish none of these.
E-03 may justify a focused Renode extension or a narrower circuit/firmware profile;
failure leaves P2 unsupported.

## Ordering and simultaneous events

The selected stable key is `(time_ns, delta, phase, origin_id, sequence)`. IDs and
sequence numbers are assigned by the orchestrator, not callback arrival order.
At a time boundary, the proposed phase order is:

1. collect scheduled external stimuli and backend observations for the boundary;
2. resolve all electrical drivers and threshold state;
3. coalesce Boolean commands per Renode pin and apply the final resolved level;
4. allow backend execution beyond the boundary according to the active profile;
5. collect outputs and publish a commit only when every domain satisfies it.

An equal-time HIGH then LOW is a zero-duration condition, not a supported pulse.
The final LOW may be applied and the collapsed edges must be diagnosed. Firmware
reaction requires positive Renode execution time; the current API does not prove
zero-time discrete settling. Bound delta cycles in future implementations and
reject a nonconverging or zero-delay feedback loop.

## Completion, pause, and failure semantics

A successful domain advance reports requested end, actual end, observed events,
and stop reason. Missing actual time or stop reason cannot be promoted to a joint
commit. A timeout is a host supervision event, not a virtual-time stop. After an
uncertain ngspice worker or accepted Renode request, the production design must
terminate and recreate the isolated worker before claiming a reset state.

`pause requested`, `domain stopped`, and `session paused at T` are distinct states.
The last state requires confirmation from every domain at the same committed
boundary. The current profile cannot promise it. Normal replay completion can
report a consistent final boundary because its complete schedule is known and
both final domain states are queried and checked.

## Predeclared E-03 criteria

E-03 must use the pinned E-01/E-02 packages, owned firmware and fixtures, and raw
machine-readable traces. It must not adjust these gates after inspecting results.

### Reference circuit and thresholds

- Ideal source: 0 V to 3.3 V; resistor: 1 kOhm; capacitor: 1 uF; initial capacitor
  voltage: 0 V; ngspice maximum step: 1 us.
- Teaching Schmitt policy: rising HIGH at `0.70 * VDD = 2.31 V`, falling LOW at
  `0.30 * VDD = 0.99 V`, retain the previous logical value between thresholds.
  This is an experiment policy, not a claim of complete STM32 input fidelity.
- For a rising step at 1 ms, the analytical HIGH crossing is
  `1 ms - 1 ms * ln(0.30) = 2.203972804 ms`.
- Locate a crossing between accepted ngspice samples by linear interpolation.
  Preserve the bracketing samples and the unrounded crossing time.

The replay phase passes when voltage error remains at or below 16.5 mV outside
the existing 2 us source-edge window, each expected threshold transition occurs
once, and crossing-time error is at most 2 us relative to the analytical reference.

### Profiles and repetitions

Run P0 replay, then P1 with `Q = 1000 us`, `100 us`, and `20 us`. Run every case
three times from a fresh backend state. Discrete event keys and counts must match
across repetitions. Report numerical traces even when a classification fails.

For P1, define `t_cross` as the first interpolated accepted crossing and `t_apply`
as the effective Renode input time. Past application (`t_apply < ceil_us(t_cross)`)
is a failure. A run qualifies only as the declared approximate profile when
`0 <= t_apply - ceil_us(t_cross) <= Q + 2 us`, no qualifying pulse is missed, and
the repeated ordering gate passes. This bound includes less than 1 us conversion
quantization and a 1 us observation allowance; it is not a causal-profile gate.

A result may be called causal only if evidence also proves that Renode never
advanced beyond the effective crossing before the input was applied. Meeting the
P1 delay bound alone cannot earn that classification.

### Boundary and adversarial cases

For each `Q`, shift the source transition so the analytical rising crossing falls
at `T - 1 us`, `T`, and `T + 1 us` around an exchange boundary. Also test:

- resolved digital pulses of 1 us, 5 us, 20 us, `Q - 1 us`, `Q`, and `Q + 1 us`
  where the width is positive and distinct;
- two opposite commands at exactly the same timestamp, expecting a diagnosed
  coalesced final value rather than two preserved edges;
- a threshold excursion that returns through the opposite Schmitt threshold;
- a backend timeout/failure injected before commit, expecting no successful joint
  commit and clean worker recreation.

A pulse counts as qualifying for the no-miss gate only when both accepted
threshold crossings exist and their separation is greater than `Q + 2 us`.
Shorter pulses are still measured and classified; they are not silently discarded
or used to claim a supported minimum width. Every case records raw source/sample,
threshold, GPIO callback, input command, request interval, actual end, commit, and
wall-supervision timestamps.

## Decision after E-03

E-03 retained P0 plus a measured P1 approximation. P1 met its numerical, delay,
boundary, pulse, and recovery gates, but every standard feedback crossing was late
to Renode and intermediate ngspice stops were 100 ns beyond the request. P2 remains
unsupported; [ADR 0011](../decisions/0011-e03-restricted-feedback.md) defers a
focused extension until a bounded lesson or experiment requires exact feedback.
