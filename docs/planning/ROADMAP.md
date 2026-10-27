# Roadmap

Revision 2 — 2026-10-14. Replaces the August baseline (archived in
[archive/2026-10-legacy/ROADMAP.md](../archive/2026-10-legacy/ROADMAP.md)).
The owner needs a usable tool now to prepare lessons, so the plan delivers a
usable release every two weeks, analog first, then the microcontroller.
Each release is tested hands-on by the owner; feedback adjusts the next phase.

## Releases

| Phase | Dates | Release | Content | Exit check |
|---|---|---|---|---|
| 0 — Reset | 2026-10-14 → 10-16 | — | New requirements and plan, repository restructure (legacy code moved aside), application skeleton building on Windows and Linux CI, engine spikes S1/S2 | Skeleton window runs on both platforms; spike results recorded |
| 1 — Analog interactive editor | 10-16 → **10-30** | **0.1** | Editor (place, wire, properties, undo), component set A, interactive continuous run with switches/buttons/potentiometers/LEDs/meters, transient mode, Signal Analyzer scope, `.snx` save/open, examples | Owner builds and runs his own analog lesson circuits on Windows |
| 2 — MCU GPIO | 11-02 → **11-13** | **0.2** | Blue Pill component, microcontroller engine (Renode first, then the own emulator), ELF load/reload, GPIO out/in, EXTI from schematic buttons, UART terminal, logic lanes, lesson 1–2 examples | Button on schematic → interrupt → LED, observed in analyzer |
| 3 — ADC, PWM, components B | 11-16 → **11-27** | **0.3** | ADC with circuit voltages, timer PWM (after audit), component set B, lesson 3 example | ADC reads a potentiometer; PWM drives RC/LED |
| 4 — Debugging and packaging | 11-30 → **12-11** | **0.4** | CubeIDE/GDB debugging, engine process isolation, Windows installer, pt-BR translation, user guide, diagnostics polish | Clean-PC install and a full lesson run by the owner |
| Freeze | **12-15** | 0.5 RC | Feature freeze; only fixes afterwards | Known-limitations list published |
| 5 — Stabilization | 12-15 → 2027-01-31 | 1.0 | Bug fixes, performance, lab rehearsal by 2027-01-15, go/no-go by 2027-01-31 | Rehearsal on a lab-like PC |
| Classroom | February 2027 | 1.0 | Frozen tested release | — |

Weekly cadence inside each phase: a test build every Friday (Windows package
from CI) with a short list of what to try.

**Status 2026-10-27.** The phases ran far ahead of these dates: releases
0.1.0 to 0.12.0 were published between 2026-10-14 and 2026-10-27, covering
phases 1-4 and more (digital parts, SPI/I2C/1-Wire chips, AI agent control,
external I/O, Cortex-M3 timing, pt-BR guides, a large sensor, logic chip,
motion and automation part library with buzzer sound, a real serial port in the circuit and recorded tutorials driven by commands). The freeze, stabilization and
the lab rehearsal before the February 2027 classroom keep their dates; until
then, work follows the owner's choices from the [backlog](BACKLOG.md) and
[current state](CURRENT.md), and every release runs `tools/examples_pass.py`
and `tools/examples_gui_pass.py`.

## Emulator track

From 2026-10-16 an own STM32 emulator is built in parallel
([plan](MCU_EMULATOR_PLAN.md), [ADR 0089](../decisions/0089-own-stm32-emulator.md)).
It became the default engine on 2026-10-16 (D1, D2,
[ADR 0090](../decisions/0090-own-emulator-default.md)) and the only one on
2026-10-18, when Renode was removed (D4, [ADR 0094](../decisions/0094-remove-renode.md)).

## Scope protection

- MCU work (phase 2) does not start before release 0.1 is usable.
- If a phase slips more than three working days, reduce scope of that phase
  (component set, analyzer extras) instead of moving later phases.
- Debugging (phase 4) is the first candidate to postpone if needed; the classroom
  can use runs without the debugger and UART output.
- The Symbol Editor (PRJ-05), subcircuits, HDL, WASM, decoders, AC/FFT analysis
  and Linux packaging are after the classroom release unless time remains.

## Initial lessons

1. GPIO output: firmware blinks an external LED with a series resistor; RC
   filter on a pin, observed in the scope.
2. Button input and EXTI: push button with pull-up/pull-down and RC debounce
   triggers an interrupt that toggles an LED; show bouncing/debounce on the scope.
3. ADC and UART: potentiometer voltage measured by the ADC and reported over
   UART; optional PWM into an RC filter.

The owner may reorder or replace these; examples ship with firmware source,
build instructions and expected observations.
