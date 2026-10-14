# ADR 0080: Continuous interactive simulation with ngspice

Date: 2026-10-14. Status: **accepted direction; memory strategy pending spike S1 (SN-103)**.

## Context

The owner requires continuous simulation with interaction: pressing a button
must visibly affect circuit and firmware while running (REQUIREMENTS SIM-01,
INT-02). A deterministic transient mode is also required (SIM-03). E-01 and
E-03 established how the ngspice shared library behaves: external sources,
`stop when`/`resume`, accepted-point callbacks, no pausing inside a step.

A measurement on 2026-10-14 (ngspice 42, Linux shared library, RC circuit with a
1 µs maximum step, one saved vector) showed about 48 MB resident for 2 s of
simulated time: ngspice keeps every accepted point of the transient plot in
memory. An unbounded run at real time would grow by roughly 20 MB per second.

## Decision

- **Interactive mode** uses one ngspice transient with a very long stop time in a
  worker thread. The `SendData` callback paces the run against wall clock times
  the speed factor and copies accepted points of probed vectors into bounded
  ring buffers. Interactive controls are ngspice sources with the `external`
  option whose values come from an atomic input table through `GetVSRCData`;
  switches/buttons are voltage-controlled switches driven by such sources.
- **Bounded memory:** the run is split into segments (initially 0.5–2 s of
  simulated time). At a segment end the session records node voltages and
  inductor currents and restarts with them as initial conditions (`uic` plus
  `.ic`/element `ic=`). The spike must confirm continuity error, restart cost
  and suitable segment length, and evaluate alternatives (larger output step
  with `interp`, ngspice data-storage options).
- **MCU coupling** (phase 2) follows the E-03 sampled exchange with quantum Q:
  Renode leads, ngspice catches up, input crossings are applied at quantum
  boundaries and the mode is labeled approximate in UI and exports.
- **Transient mode** is a plain `.tran` with stored results.

## Alternatives

- **Own circuit solver for interactive mode** (as some educational simulators
  do): full control of memory and timing but a large accuracy and maintenance
  cost; reconsider only if segmented ngspice runs prove unusable.
- **Repeated short batch runs without state transfer:** discontinuities; rejected.

## Consequences

Nonlinear device internal charge is not part of the carried state; small
discontinuities at segment boundaries may be visible and are bounded by the
spike measurements. Real-time pacing depends on circuit stiffness; the status
bar reports the achieved factor.

## Revisit criteria

Spike S1 results; user-visible glitches at segment boundaries; ngspice gaining
an option to discard old transient data.
