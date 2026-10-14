# ADR 0080: Continuous interactive simulation with ngspice

Date: 2026-10-14. Status: **accepted; memory strategy confirmed by spike S1 (SN-103) on 2026-10-14**.

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

## Spike S1 result (2026-10-14)

[S1 report](../experiments/S1-continuous-run.md), Windows, ngspice-47:

- A single long transient is not viable: memory grows linearly (27 MB per
  simulated second at 1 µs maximum step with `.save`, 0.3 MB/s at 100 µs),
  and ngspice-47 refuses to start when nodes × tstop/tstep exceeds the free
  memory, leaving the library unusable until reset.
- Segments of 1 s restarted with capacitor initial conditions keep memory
  flat (300 segments: 3.95–4.16 MB), cost 0.24 ms median per restart, and
  differ from a continuous run by at most 0.21 mV right after a restart.
- Pacing in `SendData` holds 0.9986–0.9989 of the requested speed with up to
  ~25 ms wake-up lag; a button reaches the circuit within one time step.
- 100 µs maximum step runs 55× faster than real time on the test laptop,
  10 µs 7×, 1 µs only 0.6×.

Resulting rules for SN-118:

1. Segment length 1 s simulated (configurable 0.5–2 s); `.tran` output step
   equal to the maximum step and stop time equal to the segment length.
2. Carry every capacitor voltage and inductor current (`IC=` with `uic`);
   discard the segment plot (`destroy all`, `remcirc`). Acquisition lives in
   SimNodus ring buffers.
3. Interactive default maximum step 100 µs, 10 µs selectable; 1 µs is a
   transient-mode setting.
4. `.save` only probed vectors. `interp` gives no memory benefit.

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

User-visible glitches at segment boundaries (for example semiconductor charge
not carried); classroom PCs unable to hold real time at 100 µs; ngspice gaining
an option to discard old transient data.
