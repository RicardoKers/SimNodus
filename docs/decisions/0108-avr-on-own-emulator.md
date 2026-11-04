# ADR 0108: AVR microcontrollers on SimNodus's own emulator

Date: 2026-11-04. Status: **accepted** (decision DE10, owner, 2026-11-04).

## Context

Students build their projects on bare Atmel/Microchip AVR chips. The owner
asked on 2026-11-02 for the ATmega8, ATmega32, ATmega328/328P and the
Arduino UNO ([feasibility study](../experiments/AVR-feasibility.md)).

Experiment E-10 ([plan](../experiments/E-10-plan.md),
[results](../experiments/E-10-results.md)) started with the ATmega8 and
recorded its gates before any code. Results:
- **G10a (the core): met.** Every instruction of the AVR Instruction Set
  Manual with its flags and cycles; 0 differences against avr8js on 1 500
  random blocks; about 120-140 MIPS.
- **G10b (the ATmega8 in the circuit): met.** The board comparison G10b.9
  passed on 2026-11-04. The owner ran examples 50, 51, 52 and 54 on his
  ATmega8 and saw the same serial output, PWM, crystal clock, EEPROM and
  sleep timing as in the simulation. G10b.8 has no course files yet; the
  owned firmware set stood in for them.
- **G10c (the complete chip): met in the simulator.** ADC, analog
  comparator, SPI, TWI, sleep modes, Timer2 asynchronous, brown-out. The
  ADC's ±2 LSB has not been measured on a chip, and the TWI/SPI example
  with real chips has not been run on the board.

## Decision

- AVR chips run on SimNodus's own emulator: the AVRe+ core
  (`src/emu/avr_cpu.*`) and one machine described by a table per chip
  (`src/emu/atmega.*`), behind the `McuMachine` interface (ADR 0106).
- The **ATmega8-PU** and **ATmega8A-PU** parts are no longer marked
  experimental.
- The ADC stays labeled approximate in the guide until it is compared with
  a measured voltage on a chip. So do the analog comparator (sampled every
  10 µs) and the brown-out levels (typical values).
- The fuses are part properties, in hex, with the **Fuses...** dialog. An
  ELF's fuses become the part's properties when a run starts.
- The EEPROM is kept beside the project as `<designator>-eeprom.bin`.
- The next chips follow on the same core and machine: SN-260 (ATmega328/328P,
  ATmega32/32A), SN-261 (Arduino UNO with Optiboot, shipped with its
  licence), SN-262 (avr-gdb). Each starts experimental until it is compared
  with a real chip, as the ATmega8 was.

## Alternatives

- **Keep the ATmega8 experimental** until the ADC and TWI/SPI are measured
  on a chip as well. Not chosen: the comparison asked by the plan passed,
  and the analog parts stay labeled approximate on their own.
- **simavr or avr8js as the emulator.** Rejected in the study: simavr is
  GPL-3, which SimNodus cannot link. avr8js is TypeScript and models
  ATmega328P peripherals only. The differential test found five
  deviations from the manual in avr8js.

## Consequences

- Classes can use the ATmega8 like the Blue Pill. The examples, guides and
  the CHANGELOG no longer call it experimental.
- The machine's chip table makes the next AVR chips mostly data. Their
  peripherals that the ATmega8 lacks (PCINT, the ATmega32's JTAG fuse and
  differential ADC, the 328's timers) still need code and tests.
- The fidelity labels remain where nothing was measured: the ADC, the
  comparator's sampling, the internal RC's nominal frequency, the
  brown-out levels.

## Revisit criteria

A difference reported between the simulation and a real ATmega8 in a
behavior the gates cover reopens G10b. That includes timing, the serial
port, PWM, interrupts, the EEPROM, fuses and sleep. The ADC's label is
removed when its ±2 LSB is measured on a chip (G10c.1).
