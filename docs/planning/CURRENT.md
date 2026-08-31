# Current state

Updated: 2026-08-31.

## Snapshot

- Stage: M0 published; SN-010 dependency baseline and SN-011 / E-01 complete.
- Implementation: standalone ngspice RC/lifecycle experiment works; production kernel and application not started.
- Direction: C++20 baseline, Qt 6 presentation, ngspice/XSPICE and Renode behind adapters.
- Platform: Windows first; Linux later.
- Repository language: English only.
- License: MIT for original project material, selected with the owner's authorization.
- Intended classroom use: February 2027; January is stabilization/rehearsal time.
- Author and maintainer: Ricardo Kerschbaumer.
- Git: public [RicardoKers/SimNodus](https://github.com/RicardoKers/SimNodus), default branch `main`.
- Publication: first commit `b3163a1` published on 2026-08-31; SN-041 complete.
- Checks: Windows/Ubuntu foundation checks and two complete E-01 suites on GitHub's Windows runner passed. Local E-01 evidence records three passing runs; hosted results are linked in its report.
- Collaboration: nine initial issues, four milestones, protected `main`, and private vulnerability reporting enabled.

## Completed work

Requirements from the two architecture conversations have been consolidated. Architecture, temporal risks, component/subcircuit design, debugging requirements, ADRs, milestone planning, and experiment specifications have been prepared. Repository conventions, MIT licensing, contribution/security guidance, issue/PR templates, and a local/CI documentation check are included.

SN-010 selected Renode 1.16.1 and ngspice 47, verified archive/file hashes, inventoried the existing C++/ARM tools, and added a real C++ DLL startup probe. Renode headless startup and ngspice load/init/version/quit passed locally. A Windows Renode client build failure and an ngspice pre-init crash were recorded, with a tested ngspice setup workaround.

SN-011 ran eight real ngspice cases three times: analytical RC reference, external pulse, integration breakpoint, foreground/background pause and resume, circuit/full reset, invalid-netlist recovery, and solver retry. Copied callback samples matched final vectors. The external pulse's maximum error outside the declared edge windows was about 1.103 mV, below the 16.5 mV limit.

The [backlog](BACKLOG.md) owns task status. See the [SN-010 report](../experiments/SN-010-results.md), [E-01 report](../experiments/E-01-results.md), [ADR 0007](../decisions/0007-ngspice-experiment-contract.md), and [QUALITY](../development/QUALITY.md). E-02 through E-06 have not run. No firmware or coupled simulation has run.

## Next task: SN-019 / native Windows Renode client

Follow [SN-019 / #11](https://github.com/RicardoKers/SimNodus/issues/11): adapt and test the pinned Renode external client on native Windows. Require a real loopback handshake, time query/advance, and socket failure/timeout evidence. Preserve the pinned microsecond API and provenance. Use the [Windows setup](../development/WINDOWS_BACKENDS.md); this is a prerequisite for E-02 external control. No GUI or SDK work is needed yet. Use a branch and pull request; do not bypass main protection.

## Known uncertainties

- Renode's pinned client uses microseconds and does not compile unchanged with MSVC.
- The generic STM32F103 platform is not a validated C8 memory/clock profile and includes an external SVD download; E-02 needs an offline configuration.
- ngspice still requires the owned initialization file; the known pre-init call crash is not fixed. E-01 covers bounded RC lifecycle behavior, not arbitrary reentrancy, nonlinear models, leak endurance, or all crash/timeout paths.
- ngspice integration breakpoints do not pause execution. Trial-source time can reverse; accepted analog samples do not establish a joint MCU/circuit commit.
- Bundled runtimes and transitive licenses require review before distributing binaries.
- Feedback causality and debugger arbitration are unproven.
- STM32F103 ADC integration and GPIO mode coverage require auditing.
- Laboratory Windows versions, hardware limits, and exact first lesson are not yet known.

## Resume checklist

1. Read this file and [AGENTS](../../AGENTS.md).
2. Check local changes before editing; do not overwrite unrelated work.
3. Select the next ready task and review its acceptance criteria.
4. Record execution evidence and update only genuinely completed states.
