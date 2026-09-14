# SimNodus documentation

Initial baseline: 2026-08-31. These documents describe intent and development criteria, not existing simulator capabilities.

## Product and architecture

- [Vision](VISION.md): audience, value, and boundaries.
- [Requirements](REQUIREMENTS.md): traceable commitments and acceptance evidence.
- [Architecture](architecture/README.md): modules, dependencies, and processes.
- [Virtual time and synchronization](architecture/SIMULATION_TIME.md).
- [Temporal capability profile and E-03 gates](architecture/TEMPORAL_CAPABILITY_PROFILE.md).
- [Backend contracts and electrical coupling](architecture/BACKEND_CONTRACTS.md).
- [Components and subcircuits](architecture/COMPONENTS.md).
- [Project format](architecture/PROJECT_FORMAT.md).
- [Bounded Windows local resource snapshots](architecture/LOCAL_RESOURCE_VERIFICATION.md).
- [Debugging and instruments](architecture/DEBUGGING.md).
- [Desktop UX and visual instrumentation](architecture/DESKTOP_UX.md): editor/analyzer windows, contextual probes, adjustable workspace and initial/future scope.
- [Architecture decisions](decisions/README.md).

## Living planning documents

- [Current state](planning/CURRENT.md): read first when resuming work.
- [Roadmap](planning/ROADMAP.md): milestones, dates, and gates.
- [Backlog](planning/BACKLOG.md): tasks and dependencies.
- [Risks](planning/RISKS.md): uncertainties that may change the plan.
- [Open questions](planning/QUESTIONS.md): remaining owner decisions.

## Development and evidence

- [Getting started](development/GETTING_STARTED.md).
- [Local build storage and safe cache cleanup](development/BUILD_STORAGE.md).
- [Windows backend setup](development/WINDOWS_BACKENDS.md) and [SN-010 results](experiments/SN-010-results.md).
- [E-01 ngspice results](experiments/E-01-results.md) and [executable experiment](../tests/experiments/ngspice/README.md).
- [SN-019 Windows Renode control results](experiments/SN-019-results.md) and [reproduction](../tests/experiments/renode-client/README.md).
- [E-02 STM32 firmware/I/O results](experiments/E-02-results.md) and [reproduction](../tests/experiments/renode-stm32/README.md).
- [SN-013 temporal capability profile](architecture/TEMPORAL_CAPABILITY_PROFILE.md) and [ADR 0010](decisions/0010-temporal-capability-profile.md).
- [E-03 GPIO/RC coupling results](experiments/E-03-results.md), [reproduction](../tests/experiments/coupling/README.md), and [ADR 0011](decisions/0011-e03-restricted-feedback.md).
- [E-04 focused STM32F103 ADC results](experiments/E-04-results.md), [reproduction](../tests/experiments/adc/README.md), and [ADR 0012](decisions/0012-focused-stm32f103-adc.md).
- [E-05 actual CubeIDE partial results](experiments/E-05-results.md) and [executable probe](../tests/experiments/debugging/cubeide.py).
- [SN-017 headless acceptance audit](experiments/SN-017-acceptance.md) and [reproduction](../tests/headless/APPLICATION_SESSION.md).
- [SN-018 first local baseline](experiments/SN-018-baseline.md) and [measurement contract](../tests/headless/BASELINE.md).
- [SN-018 observer control and sampler attribution defect](experiments/SN-018-observer-control.md).
- [SN-018 corrected identity sampler and real-engine control](experiments/SN-018-identity-control.md).
- [SN-018 bounded baseline acceptance](experiments/SN-018-acceptance.md).
- [SN-020 topology draft and validation](experiments/SN-020-topology.md).
- [SN-020 exact parameters and instance overrides](experiments/SN-020-parameters.md).
- [SN-020 separate symbol/model descriptor bindings](experiments/SN-020-bindings.md).
- [SN-020 inert resource lock and lexical path validation](experiments/SN-020-resource-lock.md).
- [SN-020 typed resource links](experiments/SN-020-resource-links.md).
- [SN-020 accepted project declaration baseline](experiments/SN-020-acceptance.md).
- [SN-021 local resource verification](experiments/SN-021-local-resources.md).
- [SN-021 native resource verification](experiments/SN-021-native-resources.md).
- [Quality and validation](development/QUALITY.md).
- [GitHub publication](development/GITHUB_PUBLISHING.md).
- [Licensing](development/LICENSING.md).
- [Experiments](experiments/README.md) and [report template](experiments/REPORT_TEMPLATE.md).
- [Sources and conversation synthesis](research/SOURCES.md).
- [Dependencies](research/DEPENDENCIES.md).
- [STM32 coverage](research/STM32_SUPPORT.md).

## Status vocabulary

**Owner-confirmed** means a product decision. **Proposed** means a reversible implementation choice. **Source-verified** means documentation was inspected. **Execution-validated** requires reproducible results. These states are not interchangeable.

`BACKLOG.md` owns task status; `CURRENT.md` is a short summary. ADRs preserve decisions and rationale, not task queues. When GitHub Issues exist, retain `SN-xxx` IDs and cross-links instead of maintaining two independent task lists.
