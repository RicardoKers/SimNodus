# Current state

Updated: 2026-08-31.

## Snapshot

- Stage: M2 technical proof in progress; standalone backends and restricted E-03 GPIO/RC/EXTI coupling complete.
- Implementation: real experiment hosts run replay and approximate sampled coupling; production kernel and application not started.
- Direction: C++20 baseline, Qt 6 presentation, ngspice/XSPICE and Renode behind adapters.
- Platform: Windows first; Linux later.
- Repository language: English only.
- License: MIT for original project material, selected with the owner's authorization.
- Intended classroom use: February 2027; January is stabilization/rehearsal time.
- Author and maintainer: Ricardo Kerschbaumer.
- Git: public [RicardoKers/SimNodus](https://github.com/RicardoKers/SimNodus), default branch `main`.
- Publication: first commit `b3163a1` published on 2026-08-31; SN-041 complete.
- Checks: Windows/Ubuntu foundation and repeated E-01/SN-019/E-02 suites passed on GitHub. The complete E-03 matrix passed locally and in a [clean hosted Windows run](https://github.com/RicardoKers/SimNodus/actions/runs/33462555508), each with 54 isolated real-backend cases.
- Collaboration: nine initial issues, four milestones, protected `main`, and private vulnerability reporting enabled.

## Completed work

Requirements from the two architecture conversations have been consolidated. Architecture, temporal risks, component/subcircuit design, debugging requirements, ADRs, milestone planning, and experiment specifications have been prepared. Repository conventions, MIT licensing, contribution/security guidance, issue/PR templates, and a local/CI documentation check are included.

SN-010 selected Renode 1.16.1 and ngspice 47, verified archive/file hashes, inventoried the existing C++/ARM tools, and added a real C++ DLL startup probe. Renode headless startup and ngspice load/init/version/quit passed locally. A Windows Renode client build failure and an ngspice pre-init crash were recorded, with a tested ngspice setup workaround.

SN-011 ran eight real ngspice cases three times: analytical RC reference, external pulse, integration breakpoint, foreground/background pause and resume, circuit/full reset, invalid-netlist recovery, and solver retry. Copied callback samples matched final vectors. The external pulse's maximum error outside the declared edge windows was about 1.103 mV, below the 16.5 mV limit.

SN-019 adapted the pinned C client for native Windows and generated a separate loopback-only server extension from the matching official sources. Twenty cases passed in two local runs: real handshake/time control/reconnection with normal and one-byte transfers, plus separate fault/input tests. Actual listener ownership/address and cleanup were verified. Each real run advanced an empty machine from zero to 6,018,000 us with exact requested differences. The original all-interface server was not opened.

SN-012 built an owned freestanding STM32F103C8 ELF twice identically, booted it in an offline exact-memory profile, and exercised real SysTick GPIO, input sampling, EXTI on both edges, a 20 us pulse, and same-time edges. Two fresh runs passed the 100 us and 1000 us profiles. GPIO electrical modes, RCC propagation, ADC, GDB, and coupled causality remain unsupported or unvalidated.

SN-013 combined E-01 and E-02 into an [evidence-bounded temporal capability profile](../architecture/TEMPORAL_CAPABILITY_PROFILE.md). It selects checked integer-nanosecond orchestration with forward conversion to Renode's microsecond grid, distinguishes trial/observed/effective/committed time, approves known-schedule replay, labels bounded sampled exchange approximate, and keeps general live feedback, exact joint pause, cancellation, prediction, and rollback unsupported. E-03 thresholds, tolerances, quanta, repetitions, boundary cases, pulse classification, and failure behavior are fixed before execution in [ADR 0010](../decisions/0010-temporal-capability-profile.md).

SN-014 executed [E-03](../experiments/E-03-results.md) through real Renode and ngspice processes. Three replay runs met 1.103 mV / 0.353 us maxima and ended at an exact 10 ms common boundary. The 1000 us, 100 us, and 20 us sampled profiles met their declared `Q + 2 us` bounds with maximum delays of 846 us, 95 us, and 15 us, but every standard threshold was late to Renode and every intermediate ngspice stop was 100 ns beyond the request. Twenty-seven boundary cases, direct pulses, same-time collapse, past-input rejection, and three forced-failure/fresh-recovery pairs passed. [ADR 0011](../decisions/0011-e03-restricted-feedback.md) retains causal replay, permits only labelled sampled approximation, and keeps general live causal feedback unsupported.

The [backlog](BACKLOG.md) owns task status. See [SN-010](../experiments/SN-010-results.md), [E-01](../experiments/E-01-results.md), [SN-019](../experiments/SN-019-results.md), [E-02](../experiments/E-02-results.md), [E-03](../experiments/E-03-results.md), and [QUALITY](../development/QUALITY.md). E-04 through E-06 have not run. No production simulator has been extracted.

## Next task: SN-015 ADC path

Follow [SN-015 / #6](https://github.com/RicardoKers/SimNodus/issues/6): audit the selected Renode ADC model or a focused extension, define exact voltage/code/reference/acquisition semantics, then run E-04 static points and a ramp. Use replay or explicitly approximate sampled coupling; do not treat Boolean GPIO injection as ADC evidence or infer complete STM32 ADC fidelity from model presence. Do not start GUI or production adapter extraction before the ADC and debugger gates.

## Known uncertainties

- The adapted Renode client now covers the reported transport/time and bounded GPIO/EXTI profile. ADC, broader system-bus behavior, callback unregistration/concurrency, and long sessions remain unvalidated.
- Client timeout/disconnect does not establish that an accepted Renode run stopped. The 1000 ms deadline is an experiment setting, not a product timing guarantee.
- The explicit loopback extension, paths without whitespace, and per-run temporary directories are experiment constraints, not a finished packaging design.
- E-02's offline profile has exact C8 memory bounds but only a fixed SysTick clock and RCC storage. It is not a complete Blue Pill or MCU platform.
- ngspice still requires the owned initialization file; the known pre-init call crash is not fixed. E-01 covers bounded RC lifecycle behavior, not arbitrary reentrancy, nonlinear models, leak endurance, or all crash/timeout paths.
- ngspice integration breakpoints do not pause execution. Trial-source time can reverse; accepted analog samples do not establish a joint MCU/circuit commit.
- Bundled runtimes and transitive licenses require review before distributing binaries.
- Known-schedule replay is the only selected causality-preserving profile. Bounded sampled feedback passed only as an approximation; general live feedback and debugger arbitration remain unsupported.
- Same-time input edges can collapse into one EXTI interrupt. Renode GPIO pulls, analog mode, open-drain electrical release, and RCC propagation are not hardware-faithful in the tested profile.
- STM32F103 ADC integration remains absent and requires E-04.
- Laboratory Windows versions, hardware limits, and exact first lesson are not yet known.

## Resume checklist

1. Read this file and [AGENTS](../../AGENTS.md).
2. Check local changes before editing; do not overwrite unrelated work.
3. Select the next ready task and review its acceptance criteria.
4. Record execution evidence and update only genuinely completed states.
5. For E-04, fix ADC units, timing, quantization, and sweep gates before execution.
