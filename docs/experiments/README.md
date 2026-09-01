# Experiment plan

[E-01 passed](E-01-results.md) for its bounded standalone Windows RC/lifecycle profile, [E-02 passed](E-02-results.md) for bounded owned firmware and digital GPIO/EXTI, [E-03 passed](E-03-results.md) for known-schedule replay plus an explicitly approximate sampled feedback profile, and [E-04 passed](E-04-results.md) for a focused direct-voltage STM32F103 ADC path. General live causal feedback and electrical ADC coupling remain unsupported. **E-05 and E-06 have not run.** [SN-010](SN-010-results.md) and [SN-019](SN-019-results.md) record dependency/control prerequisites. Use the [report template](REPORT_TEMPLATE.md) and store completed reports here as `E-xx-results.md`. Keep generated large traces in ignored build/output directories; commit a small sanitized reference only when useful.

Every report must identify exact backend/client/platform revisions, host/toolchain, ELF/source hashes, circuit inputs, virtual-time configuration, commands, tolerances, observed results, and limitations.

## E-01: Standalone ngspice — SN-011

**Question:** Can the selected Windows shared library provide the controls and data needed by a host?

Start with a 1 kOhm / 1 uF RC driven by a 0-to-3.3 V step, zero initial capacitor voltage. Compare samples with the analytical first-order response, using a proposed maximum voltage error of 0.5% of full scale outside a documented edge window. Treat this as a test target, not achieved accuracy.

Exercise loading, external-source callbacks, time points, discontinuities, pause/resume, reset, failed netlists, and cleanup. Inspect the actual synchronization API, callback reentrancy, and tentative versus accepted points. Measure effective stop time; do not infer it from a requested limit.

**Pass:** Reference response and lifecycle behavior reproduced; deviations and exact API semantics recorded. Otherwise document failure and a bounded next experiment.

## E-02: Standalone Renode — SN-012

**Question:** Can a matching executable/client/platform run the target firmware and expose usable I/O?

Prerequisite complete: [SN-019](SN-019-results.md) provides native Windows client/time evidence and a verified loopback-only server variant. Use that explicit control recipe and an offline C8 platform; the generic upstream STM32F103 file has oversized memory regions and an external SVD reference. Neither transport success nor a generic platform establishes a validated board configuration.

Build a minimal owned-source ELF for the selected STM32F103C8 profile. Record startup, memory map, clocks, toolchain, and linker configuration. Toggle a documented external GPIO; inject input and observe a firmware read/interrupt. Probe mode changes, pulls, alternate functions, and GPIO timestamps.

Compare requested and actual advancement, callback timing, and time units. Inspect ADC availability and the exact GDB control behavior without claiming coupled operation.

**Result:** Passed for the [bounded offline profile](E-02-results.md): reproducible boot, timed GPIO, persistent and 20 us EXTI input, callback boundaries, and initial mode/peripheral audit. Electrical modes, clock propagation, ADC, and GDB are explicitly outside the achieved profile.

## E-03: GPIO and feedback causality — SN-014

**Question:** Can engines exchange causal signals without silently applying input too late?

First replay known GPIO into the E-01 RC circuit. Then close a digital feedback loop: an electrical threshold drives a MCU input/EXTI, and firmware changes another output in response.

Use known transitions at a boundary, just before/after a boundary, within a proposed quantum, and with pulses shorter than that quantum. Capture request, observation, application, and commit times. If only sampled exchange works, report delay/error and classify it approximate.

The criteria are fixed before execution in the [SN-013 profile](../architecture/TEMPORAL_CAPABILITY_PROFILE.md): 1 kOhm/1 uF and 3.3 V, 0.70/0.30 VDD Schmitt thresholds, 16.5 mV voltage tolerance, 2 us crossing tolerance, three fresh repetitions, and sampled quanta of 1000 us, 100 us, and 20 us. It also defines forward microsecond quantization, `Q + 2 us` approximate-delay bounds, qualifying pulses, same-time collapse, boundary cases, and failure evidence. Do not relax those gates after observing a run; record a failed classification instead.

**Pass for a causal profile:** No event is committed in the consumer's past; the tested threshold error is within the predeclared tolerance; repeated runs preserve discrete event ordering; and evidence shows that the consumer did not advance beyond the effective crossing before input application. Meeting only the sampled delay gate earns an approximate classification. Reducing a quantum without evaluating missed pulses does not establish correctness.

**Decision:** Accept the verified transport/algorithm, implement a focused extension, or publish a deliberately restricted experiment profile. Do not proceed with full-feedback claims on one-way evidence.

**Result:** The [restricted profile](E-03-results.md) was selected. Replay is the
causality-preserving path; sampled exchange met its declared approximation gates
but every standard feedback event was late. No general causal algorithm or focused
extension was approved.

## E-04: ADC path — SN-015

**Question:** Can an analog value reach firmware through the intended STM32 ADC model?

Audit the selected ADC implementation, API units, conversion timing, reference, saturation, and channel mapping. Apply static points at 0%, 25%, 50%, 75%, and 100% of VREF, plus a ramp. Check firmware readback and sample instant.

For an idealized ADC profile, propose agreement within one LSB of the documented quantization rule at static points. More detailed models need their own reference; out-of-range behavior must be reported explicitly.

The detailed [predeclared E-04 profile](../../tests/experiments/adc/README.md)
selects a focused owned STM32F103 register subset after rejecting the pinned
generic model for this path. It fixes microvolt units, one internal quantization,
VREF, sample instant, conversion timing, three repetitions, static/boundary/
saturation points, a 101-point ramp, channel mapping, and adverse cases before
execution.

**Pass:** Verified unit mapping without double quantization, known sampling semantics, voltage sweep, and recorded unsupported modes. DMA and every ADC mode are not implied.

**Result:** Passed for the [focused owned extension](E-04-results.md). Three fresh Renode processes produced identical results for 363 accepted conversions in total, with zero maximum code error and exact declared conversion endpoints. Voltage was injected directly through `IADC`; ngspice, electrical acquisition, dynamic VREF, and unsupported ADC modes were not tested.

## E-05: Coordinated debugging — SN-016

**Question:** Can GDB/CubeIDE control firmware while all circuit domains remain consistent?

Test plain GDB first, then the chosen CubeIDE version. Break on a GPIO change and ADC read. Exercise continue, instruction-step, step-over, pause, session reset, debugger reset, disconnect, backend failure, and timeout.

Record effective stop times in each domain. Test a breakpoint inside an advancement interval to detect overshoot. Specify who controls Renode advancement while GDB is attached. Never enable independent free-running GDB and scheduler control without an explicit design.

**Pass:** Consistent committed state on every supported action; unsupported commands are rejected or diagnosed. Produce the actual tested CubeIDE launch recipe and versions.

## E-06: Classroom rehearsal — SN-028

On a representative Windows PC, reproduce installation without development-machine paths. Open each lesson, build/load firmware, run and debug, save/reopen, and recover from an intentional error. Test offline operation when required. Record setup duration, memory, responsiveness, missing assets, and instructor observations.

**Pass:** Owner records readiness for specific lessons on a specific release, with limitations and fallback materials. This gate is required before classroom use.
