# Roadmap

Baseline: 2026-08-31. Owner target: classes in February 2027. Dates below are planning targets, dependent on available development time and experiment results. No staffing or weekly capacity has been assumed.

## Before classroom use

| Milestone | Target window | Deliverable | Exit gate |
|---|---|---|---|
| M0 — Foundation | August 2026 | English repository, architecture, MIT license, plans, local checks | Documents are consistent and next experiments actionable |
| M1 — Standalone backends | September 2026 | Reproducible Renode/ELF and ngspice/RC experiments | Fixed versions; boot/GPIO and analog reference tests recorded |
| M2 — Headless co-simulation | September–October 2026 | GPIO output, digital feedback, ADC, traces, synchronized debugging | E-03 through E-05 pass for a declared capability profile |
| M3 — Teaching MVP | November–December 2026 | Minimal Windows editor, saving/loading, essential instruments, debug workflow | Three small lesson projects run reliably from a clean setup |
| Classroom candidate | January 2027 | Installation package/recipe, offline assets, guide, known limitations | Owner rehearses a class on a representative Windows PC |
| Classroom use | February 2027 | Frozen, tested teaching subset | No unreviewed feature upgrades immediately before class |

M3 must not be declared ready if its documented simulator behavior is unsupported. Early file-format and component work can proceed where it does not obscure M2 integration failures.

## Acceleration priority

The owner requests earlier delivery where the evidence supports it, reserving
any gained time for tests and improvements. Keep the baseline target windows;
no earlier delivery date is committed without measured progress and capacity.
SN-016's bounded gate is complete under ADR 0014; SN-017's bounded headless
composition is accepted under [ADR 0043](../decisions/0043-bounded-headless-extraction-acceptance.md).
SN-018's local baseline on that profile is accepted under
[ADR 0044](../decisions/0044-bounded-baseline-acceptance.md); SN-020 has started
through topology, parameters and separate symbol/model interface drafts. Continue
locked resource/dependency metadata and path containment next. This does not complete the
M2 product runner or approve general unpaced debugging. Reuse passing fixtures and narrow regression
runs to changed behavior; do not reopen completed experiments without a concrete
reason. Preserve acceptance criteria and a declared supported capability profile.
Use earned schedule margin first for fault coverage, clean-install rehearsals,
lesson examples, and usability fixes before adding optional features. Update
forecasts at completed gates, rather than counting partial probes as completion.

## Scope protection and decision dates

- By 2026-09-30: review M1 evidence, toolchain friction, peripheral gaps, and actual development capacity.
- By 2026-10-31: decide whether feedback and synchronized debugging support the intended lessons. If not, reduce the first lesson to a clearly labeled validated subset and retain physical hardware/existing tools as the teaching fallback. Do not claim the full product is ready.
- By 2026-12-15: freeze the classroom feature list. Prioritize defects, packaging, examples, and documentation.
- By 2027-01-15: rehearse setup and exercises on laboratory-like hardware.
- By 2027-01-31: record the go/no-go decision, exact tested release, supported lessons, and fallback materials.

These dates are review checkpoints, not automatic scheduled jobs.

## Initial lesson candidates

1. Firmware-driven GPIO with an external resistor/LED and an RC transient.
2. Button/RC input and EXTI, including a demonstrable wiring or firmware mistake.
3. ADC measurement and UART reporting, optionally PWM-to-RC only after timer/ADC coverage passes.

The owner will select the first lesson. Each distributed example needs source, build instructions, expected observations, known limitations, and tested versions.

## Longer-term evolution

The [shared instrumentation direction](../architecture/DEBUGGING.md#instruments)
guides SN-024's essential M3 views and SN-033's later circuit/firmware inspection.
Exports, probes, plots, scope, and logic views share signal/event infrastructure
and the kernel timeline. Advanced decoding remains M5+ work: the suggested native
order is PWM/frequency, UART, I2C, then SPI; Python and sigrok paths are optional
future evaluations. This does not add a runtime, decoder suite, or full inspection
to the classroom commitment, change milestone dates, or alter SN-017 extraction.

| Milestone | Scope | Prerequisites |
|---|---|---|
| M4 — Reuse | Subcircuit authoring, project/user libraries, dependency locking | Stable persistence and independent instance state |
| M5 — Teaching depth | Better diagnostics, peripheral coverage, node/software inspection, decoders | Reliable trace provenance and validated device profiles |
| M6 — Component SDK | Versioned WASM capabilities, safe authoring tools, package validation | Threat model and resource-limit tests |
| M7 — HDL | Verilog/Verilator integration and selected logic examples | One scheduler owner per model; licensing/build review |
| M8 — Expansion | Linux application release, additional STM32/other MCUs | Windows classroom baseline stable; platform CI and packaging; mature CPU/peripheral models and validated firmware, I/O, timing, and debug coverage for each selected MCU/backend |

M4 is an optional pre-class stretch goal if M2/M3 are already secure. Linux portability is considered from the start, but a Linux application release is not a February commitment. Core Linux CI is useful earlier and does not imply application support.

[MCU independence](../decisions/0028-mcu-platform-independence.md) preserves ESP32,
AVR, and other platforms as future candidates through suitable backends; it is
not a claim of current Renode coverage. STM32F103C8/Blue Pill remains the initial
reference and MVP focus. No additional platform implementation is scheduled here.
