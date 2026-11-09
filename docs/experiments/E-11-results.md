# E-11: BLDC motor speed — results

Date: 2026-11-08. [Plan](E-11-plan.md); task SN-276, then SN-278. Status:
**G11a met; G11b failed**, then mostly recovered by SN-278 (below);
decision DE11 taken by the owner on 2026-11-08: go on with SN-277 at the
measured speeds, the parts experimental.

## What was built

- `tools/generate_bldc_parts.py`: `bldc_motor` (trapezoidal back-EMF),
  `pmsm_motor` (sinusoidal) and `bridge_3phase` (three half-bridges with IN
  and EN, the output `VM · d` behind its on-resistance), all marked
  experimental.
- `examples/firmware/bldc_control` (bare metal, 72 MHz): six-step from the
  Hall sensors on EXTI, or sinusoidal commutation (PB12 low) with the angle
  interpolated between Hall edges and a min-max offset; PWM 5, 10 or 20 kHz
  (PB13, PB14); duty from a potentiometer; UART report with the speed and
  the Hall code.
- Example 64 (six-step, 80 % duty, 10 kHz) and its device test in
  `engine-ngspice`.
- A fix found on the way: the STM32 adapter now starts each watched pin at
  the circuit's low level, as the AVR and ESP32-C3 adapters do. Before, a
  pull-up enabled on a pin the circuit held low from the start (an
  open-collector Hall output that is on) read high until the net changed,
  so the firmware saw Hall code 111 and never started the motor.

## G11a (model): met

- Six-step at 80 % duty on the 12 V, 3000 rpm motor: 2337 rpm (expected
  0.8 × 3000 less the friction's share, about 2360).
- Hall codes in the documented order (101, 100, 110, 010, 011, 001), seen in
  the UART reports; the motor starts from rest.
- A load of 30 % of the stall torque slows it to 936 rpm (device test).
- Sinusoidal commutation on the PMSM: 1820-2105 rpm at 80 % amplitude.

## G11b (speed without exact edges): failed

Development laptop, 2 s of running from rest, ratio of simulated to
wall-clock time (`simnodus-cli`, speed 0):

| Mode | PWM | Edges averaged (100 µs) | Exact edges (100 µs) | Exact edges (10 µs) |
|---|---|---|---|---|
| six-step | 5 kHz | 0.083x | 0.078x | |
| six-step | 10 kHz | 0.049x | 0.043x | |
| six-step | 20 kHz | 0.027x | 0.023x | 0.022x |
| sinusoidal | 5 kHz | 0.208x | 0.118x | |
| sinusoidal | 10 kHz | 0.124x | 0.067x | |
| sinusoidal | 20 kHz | 0.073x | 0.036x | 0.035x |

The criterion was 0.5x for every run without exact edges. The speed falls
with the PWM frequency even when the edges are averaged, so the cost is not
the circuit's switching.

## Where the time goes (first look)

Six-step, 10 kHz, edges averaged, 1 s from rest:

| Variant | Speed |
|---|---|
| The example | 0.05x |
| Duty 0 (motor at rest, two phases held low) | 0.07x |
| Maximum step 1 ms, or an MCU window of 10 ms | 0.05x (no change) |
| Without the motor | 1.97x |
| Without the bridge (motor not driven) | 1.41x |
| Bridge and motor on fixed sources, no microcontroller | 7.1-7.6x |
| Motor alone on a DC source | 12x |

Each piece runs fast alone; only the board driving the bridge that drives
the motor is slow, even with the motor at rest. The cost is in the coupling
of the firmware's pins to this circuit (the per-step exchange of the
external sources, or ngspice's step control around them), not in either
model. Profiling it is the next step.

## SN-278: the cause and the fixes

Profiling the session (time points, drive requests and waits, counted per
run) showed ngspice accepting 78 000 points in 0.5 s of example 64, a 6 µs
step on average, with the waits for the firmware negligible (0.3 s of
9.4 s). Three causes, three fixes:

1. **Step averages shorter than the PWM.** The firmware's drive was averaged
   over each step (ADR 0091). Once ngspice shortened a step, the 10 kHz PWM
   reached the circuit as real switching, whose edges kept the steps short:
   the run never recovered, and the cost grew with the PWM frequency. Now,
   without exact edges, the average covers at least one maximum step (ADR
   0091 update). 340 000 → 70 000 points for 2 s.
2. **Switching elements around every commutation.** The bridge's EN switches
   and exponential body diodes cut the steps at each of the 960 commutations
   per second. EN is now a conductance in proportion to its (averaged) level,
   which is also the right average of a leg enabled part of the time, and
   the diodes are straight segments. 70 000 → 51 000 points. (Smoothing the
   Hall switches, snubbers and leakage resistors on the open phase were
   measured and did not help.)
3. **The cost of each point.** ngspice asked for each external source at
   every Newton iteration, about 400 calls per point, and each call built a
   lower-case string and searched two maps: 3.9 s of 10 s. Names are now
   resolved once per loaded circuit, by ngspice's name pointer. 10.0 → 6.6 s
   before fix 2, 4.1 s after it.

Repeated matrix (same laptop, 2 s from rest, ratio reported by the session):

| Mode | PWM | Edges averaged | Exact edges (100 µs) | Exact edges (10 µs) |
|---|---|---|---|---|
| six-step | 5 kHz | 0.13x | 0.073x | |
| six-step | 10 kHz | **0.35x** (was 0.049x) | 0.047x | |
| six-step | 20 kHz | 0.32x (was 0.027x) | 0.025x | 0.023x |
| sinusoidal | 5 kHz | 0.22x | 0.125x | |
| sinusoidal | 10 kHz | **1.16x** (was 0.124x) | 0.068x | |
| sinusoidal | 20 kHz | 0.82x (was 0.073x) | 0.040x | 0.039x |

At 5 kHz the PWM period (200 µs) is longer than the 100 µs step, so its
ripple reaches the motor as it should and costs steps. G11b (0.5x for every
averaged run) is still not met for six-step (0.32-0.35x) nor at 5 kHz; it is
met for sinusoidal commutation at 10 and 20 kHz. Exact edges stay slow, as
ADR 0093 expects.

## Recommendation for DE11

Keep the three parts and example 64 **experimental** and go on with
SN-277 with the measured speeds in the guide: sinusoidal commutation and
FOC at 10 kHz run about in real time, six-step at about a third of it,
enough to watch a start and a load step. The fixes of SN-278 (the averaging
window, the cached source names, the STM32's watched pins) help every board
and stand on their own.

## Example 65 across the step settings (2026-11-09, SN-279)

The owner asked whether field-oriented control stays stable with a 10 µs
maximum step and with exact microcontroller edges. Example 65 ran 3 s of
simulated time for each combination with `simnodus-cli` (speed 0, as fast
as the computer allows): free acceleration to 1 s, then a load of 5 % of
the stall torque. Firmware reports (500 ms windows):

| Maximum step | Exact edges | 0.5 s | 2.0-3.0 s with 5 % load | Wall time for 3 s |
|---|---|---|---|---|
| 100 µs | off | 1020 rpm, iq 801 mA | 2835-2865 rpm, iq 789-800 mA | 7 s |
| 100 µs | on | 905 rpm, iq 779 mA | 2765-2865 rpm, iq 799-810 mA | 28 s |
| 10 µs | off | 900 rpm, iq 801 mA | 2770-2815 rpm, iq 791-799 mA | 25 s |
| 10 µs | on | 880 rpm, iq 785 mA | about the same | 33 s |

All four hold iq at the 800 mA setpoint with id within ±60 mA and settle at
the same speed; none diverged. With exact edges or a 10 µs step the run is
4-5 times slower, as ADR 0093 expects. A load above the 0.8 A the
potentiometer allows stalls the motor (100 % is the stall torque at 12 V,
about 12 A); taking the load back to 5 % restarts it.
