# Virtual time and causality

Status: design invariants, informed by [standalone E-01 evidence](../experiments/E-01-results.md). Joint timing and feedback remain hypotheses for E-02 through E-05. No scheduler is implemented.

## Representation

Proposed `SimTime`: integer ticks of 1 ns at event boundaries. Resolution is not model accuracy. The ngspice adapter converts to floating-point seconds with explicit rounding and tolerances. Internal analog steps may be smaller; committed timestamps must not move backward through conversion. Check overflow and maximum session duration.

The selected Renode 1.16.1 external client uses microseconds, as recorded in the [pinned inventory](../research/DEPENDENCIES.md). Newer external-control documentation must not be substituted for that API. The proposed internal 1 ns representation does not establish 1 ns MCU control precision.

`t_committed` is the last consistent point across all domains. Tentative solver time, granted MCU time, UI time, and host wall time are distinct.

## Required invariants

1. No engine receives a causal input earlier than its committed state.
2. An event before `t_committed` is an explicit synchronization error; never silently retimestamp it.
3. Final exports and normal UI snapshots contain committed states. Label tentative previews.
4. Order events by a stable key such as `(time, delta, phase, origin, sequence)`. Resolve simultaneous drivers together, not by OS callback arrival order.
5. Bound same-time iterations; diagnose nonconverging loops instead of hanging.
6. Pause, step, failure, and reset coordinate every domain. Declare the session paused only at a confirmed consistent boundary.

## The central experiment

Running Renode to T, collecting GPIO, then running ngspice to T can establish a one-way demonstration. With feedback, the MCU may already have executed instructions that should depend on an analog input before T. Delivering the event afterward does not restore causality.

Therefore a conceptual `advance_until(T)` interface is not proof of synchronization. Determine whether a backend can stop at I/O boundaries, what lookahead it guarantees, whether partial advancement is observable, and when callbacks arrive. Do not assume future-GPIO prediction or whole-system rollback.

## Strategies to compare

| Strategy | Use | Constraint |
|---|---|---|
| Replay known GPIO edges | Initial one-way experiment | Does not validate feedback |
| Bounded steps with sampled exchange | Exploration or explicitly approximate mode | Measure coupling delay and missed pulses; not exact feedback causality |
| Conservative I/O-boundary advancement | Preferred direction for feedback | Must intervene before a consumer passes a causal event |
| Coordinated rollback/checkpoints | Later research | Requires restoration of all engine state, queues, and analog history |

No production strategy is approved yet. E-03 determines whether the official API suffices or a focused Renode extension is necessary. Failure is useful evidence and must not be hidden by smoothing traces.

## Crossings and ADC

The solver can attempt and reject steps. E-01 distinguishes trial-source queries from accepted ngspice data callbacks; neither alone establishes a joint MCU/circuit commit. Threshold detection needs an explicit method and tolerance; retrospectively locating a crossing cannot rewind an already advanced MCU. Refine before committing the consumer.

ADC integration must define acquisition time, VREF, resolution, saturation, and API units. Verify whether the backend expects volts, scaled voltage, or a conversion code. Do not quantize twice if its ADC model already performs conversion.

## Reproducibility and speed

Record UI interactions at their effective virtual times for replay. Fix versions, platform, ELF hash, parameters, seeds, and quantum. Require stable discrete events in the same environment and published analog tolerances; bit-identical cross-platform floating-point results are not an initial requirement.

Slow/fast playback is presentation policy. Measure simulated time per wall-clock second without changing peripheral clocks to fabricate performance.
