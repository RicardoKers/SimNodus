# E-11: BLDC motor speed — plan

Date: 2026-11-08. Task: SN-276 in the [backlog](../planning/BACKLOG.md);
the parts follow in SN-277. Results: [E-11-results.md](E-11-results.md)
(G11a met, G11b failed; DE11 waits for the owner).
The criteria below are recorded before any measurement and are not relaxed
after a run; a failed criterion leads to a recorded decision.

## Question

Can students run a BLDC controller (six-step from the Hall sensors, and
sinusoidal commutation as the first step to FOC) on a Blue Pill against a
SPICE model of the motor and a three-phase bridge, fast enough to watch the
motor start, run and take a load in a lesson?

## Model under test

- **Motor:** three windings (R + L) in star with an internal neutral; back-EMF
  `ke · ω · f(θe − k · 120°)` with `f` trapezoidal (120° flat tops) or
  sinusoidal; torque `Σ ke · f_k · i_k`; inertia, friction and a live load
  torque as in the DC motor (SN-271); pole pairs; three open-collector Hall
  outputs 120° apart, switching at the commutation points of six-step.
- **Bridge:** three half-bridges with IN and EN per phase (DRV8313 style).
  Each output is `VM · d` behind its on-resistance, `d` being IN over the
  logic level clamped to 0..1, and EN connects it (Hi-Z when low, with the
  body diodes to VM and GND). With exact microcontroller edges IN is 0 or the
  logic level, so the bridge switches; without them IN is the step average of
  the PWM and the bridge gives the average voltage (ADR 0091), with the
  supply current `Σ d_k · i_k` taken from VM.

## Measurements

The six-step firmware and the sinusoidal firmware, with PWM at 5, 10 and
20 kHz, with and without exact edges (circuit step 100 µs; 10 µs with exact
edges at 20 kHz), on the owner's class of laptop (the development laptop,
2026): the ratio of simulated time to wall-clock time over 2 s of running
at a steady speed.

## Criteria

- **G11a (model):** the motor's no-load speed at its rated voltage under
  six-step is within 10 % of the rated no-load speed; the Hall sequence has
  six distinct codes per electrical turn in the documented order; a load
  torque slows it and raises the bus current.
- **G11b (speed without exact edges):** six-step and sinusoidal runs at any
  PWM frequency reach at least 0.5x real time.
- **G11c (speed with exact edges):** recorded for each PWM frequency; no
  threshold, the guide states the numbers.

## Decision DE11

If G11a and G11b pass, SN-277 publishes the parts (motor, sinusoidal motor,
bridge) and examples, with the guide recommending runs without exact edges
for control lessons and exact edges for current-ripple lessons, at the
measured speeds. If G11b fails, the parts wait for a faster bridge model.
