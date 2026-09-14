# SimNodus

**Mixed-Signal & Embedded Systems Simulator**

SimNodus is a desktop simulator for teaching and exploring the interaction between real firmware, microcontrollers, analog circuits, and digital logic. Its first target is the STM32F103C8/Blue Pill, using Renode, ngspice/XSPICE, a C++ co-simulation kernel, and a Qt 6 interface.

**Status: standalone backend experiments, a bounded GPIO/RC coupling experiment, and a focused STM32F103 ADC path work; the SimNodus application is not implemented yet.** Known-schedule replay is the selected causality-preserving profile. Live sampled feedback works only as an explicitly approximate experiment; general causal feedback remains unsupported. Bounded cooperative STM32CubeIDE/GDB debugging passed E-05; physical GPIO/ADC acquisition and general debugging workloads remain unvalidated. Cycle accuracy, mandatory real-time execution, and complete hardware equivalence are not promised.

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

The bounded [native declaration ingress](docs/experiments/SN-021-ingress.md)
captures immutable JSON syntax and source positions without opening resources.
Full native schema semantics and project graph loading remain pending.

With Python 3.10 or newer, from the repository root:

```sh
python tools/check_repository.py
```

This checks repository structure and documentation; **it does not simulate circuits**. The CMake foundation reserves C++20 settings and exposes the same check. See [setup instructions](docs/development/GETTING_STARTED.md).

The opt-in Windows [E-01 experiment](tests/experiments/ngspice/README.md) runs real RC circuits through ngspice 47 and verifies analytical accuracy, external voltage callbacks, pause/resume, resets, and invalid-netlist recovery. See [measured results and limitations](docs/experiments/E-01-results.md). A separate Windows workflow repeats these checks.

The [SN-019 Windows control experiment](tests/experiments/renode-client/README.md) runs the adapted native client against real Renode with a verified loopback-only server. [E-02](tests/experiments/renode-stm32/README.md) builds owned STM32F103C8 firmware and validates bounded SysTick GPIO, injected input, and EXTI behavior in an offline profile. [E-03](tests/experiments/coupling/README.md) couples real Renode GPIO to a real ngspice RC and returns threshold feedback to firmware. [E-04](tests/experiments/adc/README.md) verifies a focused F103-compatible ADC extension from integer microvolts through firmware readback, including quantization, timing, saturation, and sampling. See the measured [E-03 restrictions](docs/experiments/E-03-results.md) and [E-04 scope](docs/experiments/E-04-results.md). Physical pin/ADC acquisition and general causal feedback remain outside the validated profile; bounded debugging evidence is described below.

The bounded [E-05 debugging gate](docs/experiments/E-05-gate-review.md) is complete:
real GDB and CubeIDE exercise persistent CPU/RC checkpoints, cooperative pause,
steps, recreated lifecycle and fault recovery. The selected profile requires
the measured backend extension and explicit pacing constraints; general unpaced
debugging remains unapproved. [ADR 0014](docs/decisions/0014-bounded-cooperative-debugging.md)
opened SN-017 headless extraction, now [accepted for the bounded composition](docs/experiments/SN-017-acceptance.md).
Native contracts, adapters and application session run with the declared Python/C#
fixture driver. GDB transport and fixture scheduling remain host-owned; the
complete production kernel/application remain pending. SN-018 has a
[first local reproducibility/performance baseline](docs/experiments/SN-018-baseline.md)
accepted for the [bounded local reference scenarios](docs/experiments/SN-018-acceptance.md).
Product performance targets and portable setup remain unvalidated.

SN-020 has an [experimental topology schema](docs/experiments/SN-020-topology.md)
with [exact parameters](docs/experiments/SN-020-parameters.md) and
[separate symbol/model interface bindings](docs/experiments/SN-020-bindings.md).
An [inert resource lock](docs/experiments/SN-020-resource-lock.md) validates file
inventory metadata and lexical paths without physical resource verification.
The composed [project declaration baseline](docs/experiments/SN-020-acceptance.md)
is accepted for SN-020, including board/firmware metadata and requested temporal
policy. Its hierarchical JSON fixtures remain non-executable; physical resource
verification is separate; runtime negotiation and native loading/saving are pending.

SN-021 now has an explicit [Windows/NTFS byte snapshot verifier](docs/architecture/LOCAL_RESOURCE_VERIFICATION.md)
separate from those declaration parsers. It checks local containment and locked
bytes without interpreting resources. See [coverage and limits](docs/experiments/SN-021-local-resources.md);
native project loading, atomic saving, source interfaces and compilation remain pending.

The [native C++20 snapshot API](docs/experiments/SN-021-native-resources.md) now
implements the bounded physical step with Windows types confined to platform
code. Complete declaration parsing remains a separate caller gate; this does
not implement native project loading or authorize resource execution.

## License and publication

Original SimNodus code and associated documentation are licensed under the [MIT License](LICENSE). Third-party software, firmware, models, fonts, and other assets keep their own terms; see the [licensing policy](docs/development/LICENSING.md).

Created and maintained by **Ricardo Kerschbaumer** ([RicardoKers](https://github.com/RicardoKers)). Source code and documentation are public at [RicardoKers/SimNodus](https://github.com/RicardoKers/SimNodus). Follow [issues](https://github.com/RicardoKers/SimNodus/issues), [milestones](https://github.com/RicardoKers/SimNodus/milestones), and [checks](https://github.com/RicardoKers/SimNodus/actions). No simulator release is available yet.

STM32 and other product names identify third-party technologies, not affiliations. A preliminary GitHub name search found no matching repository before publication; formal trademark and domain clearance remain unverified.
