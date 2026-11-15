# ADR 0110: The STM32F407 on SimNodus's own emulator

Date: 2026-11-15. Status: **accepted** (decision DE12, owner, 2026-11-15).

## Context

The owner's courses move from the Blue Pill to the STM32F407VET6: a
Cortex-M4 with a single-precision FPU, at up to 168 MHz. Experiment E-12
([plan](../experiments/E-12-plan.md),
[results](../experiments/E-12-results.md)) recorded its gates before any
code and compared the result with the owner's board:
- **G12a (the Cortex-M4F core): met.** Every DSP and FPU instruction
  against a reference; 6 000 000 FPU results against the host's IEEE
  arithmetic with no difference; lazy stacking; the Cortex-M3 unchanged.
  On the chip, `f4_float` prints the simulator's checksums.
- **G12b (firmware in the circuit): met.** The clock tree from the crystal
  on the diagram, GPIO, the serial ports, resets, STM32CubeIDE's debugger,
  and the board comparison: `f4_clock`, `f4_float`, `f4_blink` and the
  timing of 27 loops (`f4_timing`), within 0.24 % when the flash's wait
  states are behind the ART accelerator or absent.
- **G12c (the peripherals students use): met on the host**, and by the
  owner's comparison of examples 67 to 71 on the board (by eye and on the
  serial port; the plan's tolerances for `f4_station` were not measured).

## Decision

- The STM32F4 runs on SimNodus's own emulator: the Cortex-M3 core with
  the Cortex-M4F as an architecture switch (`src/emu/cpu.*`, `cpu_m4.*`,
  `softfloat32.*`), and a machine per chip (`src/emu/stm32f407.*`) behind
  the `McuMachine` interface (ADR 0106). Peripherals close to the
  STM32F103's (USART, timers, SPI, I2C, watchdogs, CRC) are one model
  configured by the chip.
- Instruction timing is calibrated on a real chip with a counting
  firmware, as for the Blue Pill (SN-181): the Cortex-M4's rules are
  apart from the Cortex-M3's and apply to the Cortex-M4 only.
- **The `stm32f407vet6_lqfp100` part is no longer marked
  experimental.** These stay labeled in the guide: the ADC is approximate
  until compared with a measured voltage; timing with wait states and the
  accelerator off is an approximation (within 14 % on the measured
  loops); what is not simulated (slave modes, I2S, USB, Ethernet, CAN,
  SDIO, the supply voltage, PH0 and PH1 as GPIO) is reported once in a run.
- Further STM32F4 chips (the STM32F411 "Black Pill" first) are a machine
  description on the same core, each experimental until compared with a
  board.

## Alternatives

- **Keep the part experimental** until every G12c item is measured on the
  board to the plan's tolerances. Not chosen: the plan
  ties the label to G12b.9, which passed, and the owner's comparison of
  the five examples found one difference, a breadboard wire.
- **Another emulator for the Cortex-M4** (Renode, QEMU). Not chosen: the
  own emulator is the only microcontroller engine since
  [ADR 0094](0094-remove-renode.md), and the chip's cycle timing and its
  pins in the circuit's time come from it.
- **One timing model for both cores.** Not chosen: the Cortex-M3 matches
  its board with its own rules, and two of the Cortex-M4's (the wait of a
  load after a store, the FPU's) do not exist on it.

## Consequences

- Classes can use the STM32F407 as they use the Blue Pill, with
  STM32CubeIDE projects, the HAL and the debugger.
- Loops counted in instructions take the chip's time when the firmware
  sets the flash as STM32CubeMX does.
- The timing rules were fitted to 27 loops; other instruction sequences
  may differ, and a new measurement can change them without changing the
  design.
- Firmware at 168 MHz that never sleeps runs below real time (about 0.8
  times); the guide says so.

## Revisit criteria

A firmware of a course that behaves differently on the board; a
measurement of the ADC, of `f4_station`'s PWM and DAC, or of the baud rate
on an analyzer outside the plan's tolerances; a need for the peripherals
left out. Tasks SN-292 to SN-299; experiment E-12.
