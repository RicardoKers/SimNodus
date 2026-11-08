# E-11: BLDC motor speed — results

Date: 2026-11-08. [Plan](E-11-plan.md); task SN-276. Status: **G11a met,
G11b failed**; decision DE11 waits for the owner.

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

## Recommendation for DE11

Keep the three parts and example 64 **experimental** in the library (they
work, slowly) and investigate the coupling cost before SN-277 (a new task
in the backlog); publish the speed in the guide until then. The fix for the
STM32's watched pins stands on its own.
