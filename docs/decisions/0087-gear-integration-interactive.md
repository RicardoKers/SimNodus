# ADR 0087: Gear integration in interactive mode

Date: 2026-10-15. Status: accepted.

## Context

Interactive runs switch circuits abruptly: switches and push buttons are
voltage-controlled switches of 0.01 Ω, GPIO outputs change in one step, and
push buttons can now bounce (SN-136). With ngspice's default trapezoidal
integration, a bouncing button across 1 µF made the node ring numerically
between −2.4 V and +2.2 V in a 3.3 V circuit, which the firmware then counted
as extra edges. Trapezoidal ringing on stiff switching is a known property of
the method, not of the circuit.

## Decision

Interactive segments add `.options method=gear` (second-order Gear). Transient
mode keeps ngspice's default method, whose accuracy the transient tests
measure (max error 9.8e-8 V on the RC reference).

## Alternatives

- *Larger switch on-resistance*: hides the ringing only partly and changes
  the circuit.
- *Gear in both modes*: transient mode is the reference mode for accuracy
  comparisons; changing it needs its own measurements.

## Consequences

- Measured with Gear: the lesson 2 button node stays within 0..3.3 V; with
  the RC filter one press gives exactly one falling edge; the RC steps of
  lesson 1 match 3.3·tanh(T/4τ) within 1.5 %; all earlier interactive tests
  (button charging, segment continuity, pulse phase, pacing, GPIO coupling)
  pass unchanged.
- Gear damps very lightly damped oscillators slightly more than the
  trapezoidal method; interactive mode is labeled approximate already.

## Revisit criteria

Lessons with high-Q resonant circuits in interactive mode, or ngspice
options that remove trapezoidal ringing without changing the method.
