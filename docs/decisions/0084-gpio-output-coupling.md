# ADR 0084: GPIO outputs drive the circuit

Date: 2026-10-15. Status: accepted.

## Context

SN-132 (MCU-03) makes the Blue Pill's GPIO outputs part of the circuit. E-03
showed that output changes recorded from Renode can be replayed into ngspice
exactly when the firmware runs ahead (known schedule), while circuit-to-MCU
feedback is only approximate (sampled at the exchange boundary). The run
already keeps firmware and circuit within a window (ADR 0083); ngspice reads
external sources through a callback at every time point.

## Decision

- **Pin model.** Each GPIO pin is a behavioral current source into its net,
  `I = i - g * (V(pin) - V(G))`, with `g` and `i` from two external sources
  per pin (template token `{ext:NAME}`), plus 100 MΩ leakage for the DC path:
  - push-pull output (also alternate function): `g = 1/R_out`,
    `i = level * V_supply / R_out` — a 3.3 V or 0 V source behind R_out;
  - open-drain output: low `g = 1/R_out, i = 0`; high released (`g = 0`);
  - input with pull-up/down: `g = 1/R_pull`, `i = V_supply / R_pull` or 0;
  - floating input or analog: open.
  Values from the board's `pin_model`: 3.3 V, R_out = 40 Ω (STM32F103
  datasheet: V_OL ≤ 0.4 V at 8 mA, so ≤ 50 Ω; 40 Ω is a teaching
  approximation, not a measured curve), R_pull = 40 kΩ (datasheet typical).
- **Causality.** The circuit never runs ahead of the firmware: before ngspice
  uses the pin sources at time t, the session waits until the firmware has
  reached t, so outputs are replayed from the firmware's recorded history
  (E-03 profile P0). The firmware may lead by at most `mcu_lead_us` (20 ms here; since ADR 0085 the
  coupling quantum, default 1 ms).
- **History.** The runner records each watched pin's level from Renode's GPIO
  events (Renode master-time stamps, quantum 100 µs) and its configuration
  (CRL/CRH/ODR) after every run chunk of at most 10 ms, effective from the
  chunk's start; history older than 100 ms before the circuit's time is
  dropped.

## Alternatives

- *Voltage source plus switch per pin*: needs switch models with on/off
  resistance and a control per state; the single B-source covers all modes.
- *Shorter Renode quantum for finer edges*: more synchronization cost; kept
  at 100 µs until a lesson needs sub-100 µs edges (PWM in phase 3).
- *Reading the configuration at every event*: one bus read per edge; the
  chunk-start rule is exact for the usual "configure, then drive" order.

## Consequences

- Output edges fall on Renode's 100 µs quantum and on the circuit's time
  step (100 µs by default); a configuration change inside a chunk is applied
  up to 10 ms early. Both are stated in the run's approximation note.
- Measured (Windows, blink firmware into 1 kΩ): high level 3.1731 V
  (3.3 V × 1000/1040), edges at 249.996 / 499.948 / 749.996 / 999.896 ms
  for 250 / 500 / 750 / 1000 ms (SysTick 125 ppm fast, quantum); example 07
  drives a red LED at about 3.8 mA.
- At maximum speed the run is as fast as the slower engine; in tests the
  firmware led the circuit by up to 10 ms and was never behind.
- Circuit-to-firmware inputs (SN-133) remain the approximate direction:
  applied at exchange boundaries, labeled approximate (ADR 0011).

## Revisit criteria

Lessons with fast edges (PWM, UART bit timing on wires), measured output
characteristics of real Blue Pills, or SN-133 results that call for a
different window.
