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

| Milestone | Scope | Prerequisites |
|---|---|---|
| M4 — Reuse | Subcircuit authoring, project/user libraries, dependency locking | Stable persistence and independent instance state |
| M5 — Teaching depth | Better diagnostics, peripheral coverage, node/software inspection, decoders | Reliable trace provenance and validated device profiles |
| M6 — Component SDK | Versioned WASM capabilities, safe authoring tools, package validation | Threat model and resource-limit tests |
| M7 — HDL | Verilog/Verilator integration and selected logic examples | One scheduler owner per model; licensing/build review |
| M8 — Expansion | Linux application release, additional STM32/other MCUs | Windows classroom baseline stable; platform CI and packaging |

M4 is an optional pre-class stretch goal if M2/M3 are already secure. Linux portability is considered from the start, but a Linux application release is not a February commitment. Core Linux CI is useful earlier and does not imply application support.
