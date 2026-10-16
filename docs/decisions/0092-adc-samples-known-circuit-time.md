# ADR 0092: ADC samples read the circuit at a time it has always passed

Date: 2026-10-16. Status: accepted (SN-178).

## Context

The ADC of the own emulator (SN-178, MCU-06) converts the voltage of a pin at
the end of its sampling time. The firmware runs up to one coupling window
(L = 100 µs, ADR 0090 / SN-173) ahead of the circuit, so the voltage at the
sample instant is usually not computed yet. The design (MCU_EMULATOR.md
section 7) planned an `analog_at` call that would wait for the circuit. That
wait can deadlock: ngspice evaluates a trial step ending after the sample
instant and waits for the firmware to reach the step's end, while the firmware
waits at the sample for the circuit. Reading "the latest value the circuit
has" instead would make the codes depend on thread timing.

## Decision

- At each accepted time point the session sends the voltages of the board's
  wired pins to the runner (`McuBackend::set_analog`), before it lets the
  firmware run further.
- A sample taken in window k (time in [kL, (k+1)L)) reads the pin voltage at
  **(k+1)L − lag**, interpolated between accepted points, with
  lag = firmware lead + maximum circuit step (200 µs by default). The
  firmware may run window k only after the circuit granted a limit of at
  least (k+1)L, which needs an accepted point at or after (k+1)L − lead −
  maximum step. So the voltage read is always known, and the same circuit
  gives the same codes in every run.
- The voltage is therefore 100–200 µs old at the default settings: labeled
  approximate, like the input delay of digital pins (SN-173). A counter
  (`late_analog_samples`) records samples that had to use the latest value
  instead (none in the tests; after Stop the firmware no longer advances).

## Alternatives

- Waiting for the circuit at the sample instant: deadlocks as described,
  unless each sample also cut the circuit's step, which costs speed at
  continuous conversion rates (16 000 samples per second in example 14).
- Latest committed value: no wait and no delay bound, but codes would change
  between runs (it depends on scheduling).

## Consequences

- ADC codes are exact for voltages that change slowly against 0.2 ms
  (potentiometers, sensors, RC filters): example 14 gives the code of the
  wiper voltage ngspice computed (2048 at 1.65 V, 819 at 0.66 V).
- Sampling a fast waveform (a 1 kHz sine) shows a phase delay of 0.1–0.2 ms.
- A smaller maximum step or quantum shortens the delay.

## Revisit criteria

A lesson that samples fast signals (audio, sine at kHz) needs a smaller delay:
then cut the circuit's step at sample instants for those channels and measure
the speed, as for the edges in ADR 0091.
