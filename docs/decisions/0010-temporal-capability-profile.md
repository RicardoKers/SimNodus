# ADR 0010: evidence-bounded temporal capability profile

Date: 2026-08-31. Status: accepted for SN-013; E-03 execution pending.

## Context

E-01 shows that ngspice trial time can reverse, accepted samples are distinct,
integration breakpoints do not pause, and asynchronous stop is not an exact
virtual-time boundary. E-02 shows exact integer-microsecond Renode `RunFor` ends
and callbacks inside active requests, but no event prediction, cancellation,
instruction-level timestamp, or rollback. Same-time inputs can collapse.

The coupled experiment needs an honest contract before implementation. A common
`advance_until(T)` shape would hide incompatible meanings and could apply feedback
after the MCU had already consumed the affected interval.

## Decision

Adopt the [temporal capability profile](../architecture/TEMPORAL_CAPABILITY_PROFILE.md).
Use checked integer nanoseconds for orchestration, retain raw backend times, and
round Renode input forward to its integer-microsecond grid. Separate requested,
actual, observed, effective, and committed times.

Approve known-schedule replay as causality-preserving for its declared input
schedule. Treat bounded sampled exchange as approximate and require a measured
delay/pulse profile. Keep general live feedback, exact joint pause, accepted-run
cancellation, prediction, and coordinated rollback unsupported. Predeclare E-03
thresholds, tolerances, quanta, repetitions, boundary cases, and classification
rules before any coupled run.

## Alternatives

- Calling both backends in fixed quanta without a capability label was rejected
  because smaller quanta do not repair late events or guarantee short pulses.
- Treating ngspice breakpoints as exact stops was rejected by E-01 execution.
- Treating callback timestamps as instruction-level Renode stops was rejected by
  E-02 evidence.
- Building rollback or a custom Renode extension now was deferred until E-03 shows
  which missing capability matters for the bounded classroom circuit.

## Consequences

- E-03 can produce a useful replay and measured approximation even if exact live
  feedback fails.
- Traces and diagnostics need several time fields and an explicit profile label.
- Equal-time Boolean commands are resolved to a final pin level and diagnosed;
  zero-duration edge preservation is not promised.
- An uncertain backend failure prevents a joint commit and requires worker
  recreation unless a narrower recovery path is independently verified.
- No production scheduler, adapter, or coupled capability is implemented by this
  decision.

## Revisit criteria

Revisit after E-03, after a focused Renode extension proves an I/O-boundary stop or
lookahead, after coordinated checkpoint/rollback exists, or when backend versions
change the measured semantics. Any replacement must preserve the distinction
between trial, observed, effective, and committed time.
