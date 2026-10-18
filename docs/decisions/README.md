# Architecture decision records

## Latest decisions (2026-10-14 rebuild)

- [0094: Remove Renode; the own emulator is the only microcontroller engine](0094-remove-renode.md):
  decision D4 (2026-10-18); supersedes ADR 0082 and the legacy-engine part of ADR 0090.
- [0093: Exact microcontroller edges as a simulation setting](0093-exact-edges-option.md):
  breakpoints at firmware edges on request (square waves at the 100 µs step, slower runs).
- [0092: ADC samples read the circuit at a time it has always passed](0092-adc-samples-known-circuit-time.md):
  SN-178; deterministic codes, 0.1–0.2 ms delay at the default settings.
- [0091: Firmware outputs enter the circuit as a step-averaged drive](0091-step-averaged-pin-drive.md):
  SN-176; exact PWM averages at the default step, breakpoints measured and not taken.
- [0090: The own emulator is the default engine; Renode is frozen](0090-own-emulator-default.md):
  decisions D1 and D2 (2026-10-16); new MCU features only on the emulator (Renode part
  superseded by ADR 0094).
- [0089: Own STM32 emulator as the microcontroller engine](0089-own-stm32-emulator.md):
  **accepted** 2026-10-15 (D0, after E-07); in-process ARMv7-M emulator with
  pin-level peripherals, Renode as fallback.
- [0088: Clock model for the STM32F103C8](0088-clock-model.md): RCC ready
  flags and SWS, CPU MIPS and SysTick follow HCLK, bit-banding; CubeIDE HAL
  projects run at 72 MHz.
- [0087: Gear integration in interactive mode](0087-gear-integration-interactive.md):
  avoids trapezoidal ringing on abrupt switching (bouncing buttons, GPIO edges).
- [0086: UART terminal through the control protocol](0086-uart-terminal.md): new
  UART command in the loopback server and client; USART1–3; terminal tab with
  time stamps; baud rate not simulated.
- [0085: GPIO inputs, EXTI and the coupling quantum](0085-gpio-inputs-and-quantum.md):
  Schmitt inputs (1.16/1.83 V) reach the firmware within the quantum Q (default
  1 ms); own STM32F1 GPIO model; EXTI routing; Renode idle skipping.
- [0084: GPIO outputs drive the circuit](0084-gpio-output-coupling.md): pin model
  (40 Ω push-pull, open drain, 40 kΩ pulls) as a behavioral source per pin; the
  circuit never runs ahead of the firmware.
- [0083: Microcontroller in lockstep with the circuit](0083-microcontroller-lockstep.md):
  firmware and circuit stay within 20 ms of simulated time in both directions;
  board profiles are library data; measured Renode model deviations.
- [0082: Renode engine integration](0082-renode-engine-integration.md): **superseded
  by ADR 0094**; one Renode process per run, loopback-only control server, per-run folder, vendored client;
  distribution of the Renode bundle still needs a license review.
- [0081: Desktop UI technology and layout](0081-desktop-ui-technology.md): Qt Widgets,
  QGraphicsView canvas, own plotting, data-driven symbols, QSpice-like layout.
- [0080: Continuous interactive simulation](0080-continuous-interactive-simulation.md):
  paced continuous ngspice run with external-source controls; 1 s segments restarted with
  initial conditions keep memory bounded (spike S1).
- [0079: Product rebuild and lightweight process](0079-product-rebuild-and-lightweight-process.md):
  previous product code moved to `legacy/`; backend evidence kept.

ADRs 0001–0078 below are the history of the first development line. Where they
conflict with 0079–0081, the newer ADRs prevail.

## Latest presentation decision

- [0078: Qt Widgets and worker boundary](0078-qt-widgets-worker-boundary.md):
  bounded SN-022 shell and crash-containment selection; production integration
  and full desktop usability remain pending.

## Latest experimental commit decision

- [0077: Bounded managed commit candidate](0077-bounded-managed-commit-candidate.md):
  selected experiment; [bounded physical acceptance](../experiments/SN-021-final-acceptance.md)
  audited for one Windows/local-NTFS managed document; no production service selected.

## Latest saving decision

- [0076: Managed document saving](0076-managed-document-saving.md): accepted owner
  workflow; bounded experimental lifecycle audited. External overwrite remains
  unsupported; no production service installation is selected.

An ADR records context, decision, consequences, and revisit criteria. Do not rewrite history to hide a changed decision: add a new ADR and mark the old one superseded.

| ADR | Decision | Status |
|---|---|---|
| [0001](0001-core-and-backends.md) | Headless C++ core, independent graph, ngspice/Renode, Qt presentation | Accepted direction; implementation details proposed |
| [0002](0002-time-and-debugging.md) | One virtual-time authority and explicit debugging coordination | Accepted invariant; algorithm pending experiments |
| [0003](0003-components-and-projects.md) | Separate models/symbols; versioned text projects and reusable subcircuits | Accepted direction; schema proposed |
| [0004](0004-license-language-platform.md) | MIT, English repository, Windows first | Accepted under owner authorization |
| [0005](0005-classroom-scope.md) | February 2027 teaching target with a January readiness gate | Accepted target; delivery scope conditional |
| [0006](0006-windows-backend-baseline.md) | Exact Windows experiment packages and native client prerequisite | Accepted for experiments; distribution/integration unproven |
| [0007](0007-ngspice-experiment-contract.md) | Measured ngspice trial/output, pause, reset, and lifecycle semantics | Accepted for bounded E-01 profile; coupled algorithm pending |
| [0008](0008-windows-renode-control.md) | Native Windows client, verified loopback exposure, bounded operation failures | Accepted for SN-019 control/time profile; firmware and coupling pending |
| [0009](0009-stm32-experiment-profile.md) | Owned firmware, exact C8 memory, bounded digital GPIO/EXTI profile | Accepted for E-02 evidence; electrical modes, ADC, and coupling pending |
| [0010](0010-temporal-capability-profile.md) | Evidence-bounded replay, sampled exchange, time conversion, and unsupported feedback operations | Accepted for SN-013; evaluated by E-03 / ADR 0011 |
| [0011](0011-e03-restricted-feedback.md) | Retain causal known-schedule replay and explicitly approximate sampled feedback | Accepted from E-03 evidence; production extraction pending |
| [0012](0012-focused-stm32f103-adc.md) | Use an owned F103-compatible ADC subset with microvolt input and single quantization | Accepted for E-04 evidence; production integration pending |
| [0013](0013-guarded-debugging-profile.md) | Gate debugger packets and abort unsupported operations before forwarding | Retained default policy; cooperative profile added by ADR 0014 |
| [0014](0014-bounded-cooperative-debugging.md) | Accept bounded cooperative joint debugging and permit headless extraction | Accepted from E-05 evidence; SN-016 done, SN-017 ready |
| [0015](0015-incremental-session-contract.md) | Incremental C++ joint-state gate with real-engine harness validation | Accepted for SN-017 extraction slice; native orchestration pending |
| [0016](0016-bounded-cancellation-ingress.md) | Strict cancellation-result parsing and bounded native file ingress | Accepted for SN-017 extraction slice; command transport remains experimental |
| [0017](0017-native-cancellation-channel.md) | Native grant/cancel pipe and ready handshake under phase deadlines | Accepted for SN-017 fixture slice; process lifecycle remains experimental |
| [0018](0018-native-fixture-process-lifecycle.md) | Native Windows fixture process creation and job-owned teardown | Accepted for SN-017 slice; orchestration remains experimental |

| [0019](0019-native-debug-command-emission.md) | Native bounded progress and joint-notification emission | Accepted for SN-017 slice; debug response ingress remains experimental |

| [0020](0020-native-debug-reply-ingress.md) | Complete native progress/notification reply ingress | Accepted for Windows fixture; orchestration pending |

| [0021](0021-composite-fixture-grant-transitions.md) | Composite native grant/start and observation/cancel transitions | Accepted for fixture; full orchestration pending |

| [0022](0022-native-analog-command-channel.md) | Native analog catch-up command from CPU acknowledgement | Accepted for fixture; analog reply ingress pending |

| [0023](0023-native-analog-reply-ingress.md) | Exclusive native analog stdout framing and agreement | Accepted for fixture; exchange/inspection scheduling pending |

| [0024](0024-native-fixture-exchange-inspection.md) | Native bounded GPIO high and analog inspection coordination | Accepted for fixture; ADC coordination and full scheduling pending |

| [0025](0025-bounded-adc-input-coordination.md) | Bounded native ADC preparation and confirmation | Accepted for fixture; helper invocation/result ingress pending |

| [0026](0026-native-adc-helper-process.md) | Native bounded ADC helper execution and result ingress | Accepted for Windows fixture; analytical validation and full scheduling pending |

| [0027](0027-shared-instrumentation.md) | Shared signal/event infrastructure and GUI-independent protocol decoding | Accepted architectural direction; implementation and optional integrations pending |

| [0028](0028-mcu-platform-independence.md) | MCU-family-neutral core and instrumentation; external IDEs/toolchains | Accepted principle; additional platform support pending |

| [0029](0029-native-rc-trajectory-validation.md) | Native bounded RC analytical acceptance | Accepted for fixture; GDB verification and commit scheduling pending |

| [0030](0030-bounded-native-mailbox-readback.md) | Native final mailbox verification and commit prerequisite | Accepted for fixture; raw GDB ingress remains host-owned |

| [0031](0031-native-raw-mi-mailbox-ingress.md) | Native exact MI mailbox parsing and decoding | Accepted for fixture; independent request correlation pending |

| [0032](0032-bounded-mailbox-request-correlation.md) | Native mailbox request token and shared deadline | Accepted for one owned session; raw time validation pending |

| [0033](0033-native-raw-stopped-time-validation.md) | Native raw elapsed-time and completion validation | Accepted for fixture; stream association and inspection stability remain host-owned |

| [0034](0034-bounded-final-register-stability.md) | Native final r0/sp/lr stability and commit prerequisite | Accepted for fixture; register request ownership remains host-owned |

| [0035](0035-native-register-pair-correlation.md) | Native sequential register-pair requests | Accepted for fixture; minimum observation interval remains host-owned |

| [0036](0036-native-observation-interval.md) | Native minimum register observation interval | Accepted for fixture; post-pair time confirmation remains host-owned |

| [0037](0037-native-post-inspection-time.md) | Native raw post-pair time confirmation | Accepted for fixture; host GDB transport and stream association remain |

| [0038](0038-fixture-inspection-coordinator.md) | Bounded final inspection application coordinator | Accepted for extraction; protocol and transport boundaries preserved |

| [0039](0039-fixture-adc-coordinator.md) | Bounded ADC application coordinator | Accepted for extraction; transfer and helper lifetime preserved |

| [0040](0040-fixture-analog-coordinator.md) | Bounded analog worker coordinator | Accepted for extraction; borrowed endpoints and acceptance preserved |

| [0041](0041-fixture-execution-coordinator.md) | Bounded Renode execution/debug coordinator | Accepted for extraction; accounting and deadlines preserved |

| [0042](0042-fixture-application-session.md) | Bounded application session and global gates | Accepted for extraction; protocol and endpoint ownership preserved |

| [0043](0043-bounded-headless-extraction-acceptance.md) | Accept bounded SN-017 headless composition | Accepted; native extraction done with explicit host orchestration limits |

| [0044](0044-bounded-baseline-acceptance.md) | Accept local reproducibility/performance baseline for fixed fixture scenarios | Accepted; product performance and portable setup remain pending |

| [0045](0045-experimental-topology-draft.md) | Bounded experimental topology schema and reference validator | Accepted for SN-020 first slice; full project/component schema pending |

| [0046](0046-exact-parameter-draft.md) | Exact unit-bearing declarations, scoped overrides and bounded resolution | Accepted for SN-020 0.2; model/project contracts pending |

| [0047](0047-declarative-bindings-draft.md) | Separate symbol/model interfaces with explicit complete mappings | Accepted for SN-020 0.3; resource/runtime contracts pending |

| [0048](0048-declarative-resource-lock.md) | Inert resource inventory and lexical paths with explicit physical verification boundary | Accepted for SN-020 metadata; loading remains pending |

| [0049](0049-desktop-ux-and-measurements.md) | Independent editor/analyzer windows, contextual probes and shared measurements | Accepted owner direction; Qt selection and implementation pending |

| [0050](0050-typed-descriptor-resources.md) | Typed descriptor/resource links and explicit source entrypoints | Accepted for SN-020 reference validation |

| [0051](0051-project-declaration-baseline.md) | Project declaration baseline and SN-020 acceptance boundary | Accepted for schema/reference validation; runtime remains pending |

Use the [template](TEMPLATE.md) for new decisions.

- [0055: Automation-first headless application boundary](0055-automation-first-headless.md):
  accepted direction; public contracts, MCP and Python integration remain pending.

- [0054: Bounded native declaration syntax ingress](0054-declaration-ingress.md):
  immutable original bytes/tokens; native schema semantics and graph remain pending.

- [0053: Native resource verification](0053-native-resource-verification.md):
  typed C++ snapshot API and Windows implementation; full project loading pending.

- [0052: Bounded local resource snapshots](0052-bounded-local-resource-snapshots.md):
  first SN-021 reference boundary; native persistence and execution remain pending.

The SN-020 draft records retain their historical statuses. [ADR 0051](0051-project-declaration-baseline.md)
accepts their composition as a bounded declaration baseline, without runtime approval.
