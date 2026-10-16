# ADR 0090: The own emulator is the default engine; Renode is frozen

Date: 2026-10-16. Status: accepted (decisions D1 and D2 of the
[emulator plan](../planning/MCU_EMULATOR_PLAN.md), owner).

## Context

ADR 0089 chose to build SimNodus's own STM32 emulator; experiment E-07 and gate
G1 measured it against Renode ([E-07-results.md](../experiments/E-07-results.md)):
every MCU test passes on both engines; the examples run at 1.00-1.01x real time
(example 12, HAL at 72 MHz: 0.84x with Renode); a board starts at once instead
of after 7-9 s; inputs reach the firmware within 0.1 ms instead of up to 2 ms;
coupled runs repeat exactly. The owner tried the build and approved D1.

## Decision

- **D1:** Blue Pill boards run on the own emulator by default (board profile
  `"engine": "native"`). Renode stays available as a legacy engine through
  **Simulate > Use Renode for Microcontrollers (Legacy)** and
  `SIMNODUS_MCU_ENGINE=renode`.
- **D2:** new MCU features — ADC with circuit voltages, timers and PWM, serial
  frames on the pins, debugging — are built only on the own emulator. The
  Renode path is frozen: it gets no new features, only fixes needed to keep
  its tests passing.
- Renode stays in the package and in CI (`engine-renode`,
  `engine-renode-session`, `gui-editor-renode`) until decision D4.

## Consequences

- The default test suites exercise the emulator (`engine-ngspice` with the
  session MCU cases: 10 s instead of 53 s; `gui-editor`: 7 s instead of 41 s).
- Phase 3 tasks SN-140 (ADC through Renode) and SN-141 (Renode timer audit) are
  superseded by SN-178 and SN-177 on the emulator.
- With Renode chosen, a firmware that uses ADC, PWM or later features does not
  get them; MICROCONTROLLER.md says so.
- Removing Renode from the package (about 100 MB, .NET) is decision D4, after
  two releases on the emulator without MCU regressions.

## Revisit criteria

An MCU regression on the emulator that the owner finds and that is not fixed
within three working days (rollback trigger D1b of the plan).
