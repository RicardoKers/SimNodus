# SimNodus

**Mixed-Signal & Embedded Systems Simulator**

SimNodus is a desktop simulator for teaching and exploring the interaction between real firmware, microcontrollers, analog circuits, and digital logic. Its first target is the STM32F103C8/Blue Pill, using Renode, ngspice/XSPICE, a C++ co-simulation kernel, and a Qt 6 interface.

**Status: standalone backend experiments. ngspice RC/lifecycle and bounded STM32 firmware/digital-I/O experiments work; the SimNodus application is not implemented yet.** Coupled backend integration, electrical GPIO, ADC, and STM32CubeIDE compatibility still require experimental validation. Cycle accuracy, mandatory real-time execution, and complete hardware equivalence are not promised.

## Start here

| Purpose | Document |
|---|---|
| Resume development | [Current state and next task](docs/planning/CURRENT.md) |
| Understand the product | [Vision](docs/VISION.md) and [requirements](docs/REQUIREMENTS.md) |
| Understand the modules | [Architecture](docs/architecture/README.md) |
| Plan the work | [Roadmap](docs/planning/ROADMAP.md) and [backlog](docs/planning/BACKLOG.md) |
| Set up development | [Getting started](docs/development/GETTING_STARTED.md) |
| Find documentation | [Documentation index](docs/README.md) |
| Contribute and follow progress | [GitHub setup and publication record](docs/development/GITHUB_PUBLISHING.md) |

## First technical milestone

Before building the editor, demonstrate a headless flow:

```text
firmware ELF -> Renode / STM32 -> GPIO -> ngspice / RC -> STM32 input
                                  ^                         |
                                  +----- virtual time ------+
```

Progress through standalone engines, GPIO output, digital/EXTI feedback, ADC, and coordinated debugging. Each step requires reproducible evidence. The circuit model remains independent of the GUI and SPICE netlist.

## Product direction

- Component manifests with symbols separated from simulation behavior.
- Reusable subcircuits with explicit ports and project/user libraries.
- Teaching instruments, actionable diagnostics, and future circuit/firmware inspection.
- STM32CubeIDE/GDB debugging as a product requirement.
- WASM and Verilog/Verilator extensions after the initial proof of concept.
- Windows first; Linux later. Classroom use is targeted for **February 2027**, with a January readiness review.

All repository documentation, code, comments, templates, and committed project text must be in English.

## What runs today

With Python 3.10 or newer, from the repository root:

```sh
python tools/check_repository.py
```

This checks repository structure and documentation; **it does not simulate circuits**. The CMake foundation reserves C++20 settings and exposes the same check. See [setup instructions](docs/development/GETTING_STARTED.md).

The opt-in Windows [E-01 experiment](tests/experiments/ngspice/README.md) runs real RC circuits through ngspice 47 and verifies analytical accuracy, external voltage callbacks, pause/resume, resets, and invalid-netlist recovery. See [measured results and limitations](docs/experiments/E-01-results.md). A separate Windows workflow repeats these checks.

The [SN-019 Windows control experiment](tests/experiments/renode-client/README.md) runs the adapted native client against real Renode with a verified loopback-only server. [E-02](tests/experiments/renode-stm32/README.md) builds owned STM32F103C8 firmware and validates bounded SysTick GPIO, injected input, and EXTI behavior in an offline profile. See [results and limits](docs/experiments/E-02-results.md). Electrical GPIO, ADC, and coupled simulation are still pending.

## License and publication

Original SimNodus code and associated documentation are licensed under the [MIT License](LICENSE). Third-party software, firmware, models, fonts, and other assets keep their own terms; see the [licensing policy](docs/development/LICENSING.md).

Created and maintained by **Ricardo Kerschbaumer** ([RicardoKers](https://github.com/RicardoKers)). Source code and documentation are public at [RicardoKers/SimNodus](https://github.com/RicardoKers/SimNodus). Follow [issues](https://github.com/RicardoKers/SimNodus/issues), [milestones](https://github.com/RicardoKers/SimNodus/milestones), and [checks](https://github.com/RicardoKers/SimNodus/actions). No simulator release is available yet.

STM32 and other product names identify third-party technologies, not affiliations. A preliminary GitHub name search found no matching repository before publication; formal trademark and domain clearance remain unverified.
