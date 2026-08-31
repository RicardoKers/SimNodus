# Current state

Updated: 2026-08-31.

## Snapshot

- Stage: M0 published; SN-010 Windows dependency baseline complete, ready for E-01.
- Simulator implementation: not started.
- Direction: C++20 baseline, Qt 6 presentation, ngspice/XSPICE and Renode behind adapters.
- Platform: Windows first; Linux later.
- Repository language: English only.
- License: MIT for original project material, selected with the owner's authorization.
- Intended classroom use: February 2027; January is stabilization/rehearsal time.
- Author and maintainer: Ricardo Kerschbaumer.
- Git: public [RicardoKers/SimNodus](https://github.com/RicardoKers/SimNodus), default branch `main`.
- Publication: first commit `b3163a1` published on 2026-08-31; SN-041 complete.
- Hosted checks: Windows and Ubuntu foundation jobs passed; simulator behavior is still untested.
- Collaboration: nine initial issues, four milestones, protected `main`, and private vulnerability reporting enabled.

## Completed work

Requirements from the two architecture conversations have been consolidated. Architecture, temporal risks, component/subcircuit design, debugging requirements, ADRs, milestone planning, and experiment specifications have been prepared. Repository conventions, MIT licensing, contribution/security guidance, issue/PR templates, and a local/CI documentation check are included.

SN-010 selected Renode 1.16.1 and ngspice 47, verified archive/file hashes, inventoried the existing C++/ARM tools, and added a real C++ DLL startup probe. Renode headless startup and ngspice load/init/version/quit passed locally. A Windows Renode client build failure and an ngspice pre-init crash were recorded, with a tested ngspice setup workaround.

The [backlog](BACKLOG.md) owns task status. See the [SN-010 report](../experiments/SN-010-results.md) and [QUALITY](../development/QUALITY.md). No E-01 through E-06 experiment has passed; no circuit or firmware has run.

## Next task: SN-011 / E-01

Use the [Windows setup](../development/WINDOWS_BACKENDS.md) to run the standalone ngspice RC/lifecycle experiment. Follow [SN-011 / #2](https://github.com/RicardoKers/SimNodus/issues/2) and the [experiment plan](../experiments/README.md), preserving the exact binaries and recorded startup limitation.

[SN-019 / #11](https://github.com/RicardoKers/SimNodus/issues/11) is also ready: adapt and test the pinned Renode external client on native Windows. It is a prerequisite for E-02 external control. No GUI or SDK work is needed yet. Use a branch and pull request; do not bypass main protection.

## Known uncertainties

- Renode's pinned client uses microseconds and does not compile unchanged with MSVC.
- The generic STM32F103 platform is not a validated C8 memory/clock profile and includes an external SVD download; E-02 needs an offline configuration.
- ngspice startup is verified only with the owned initialization file; the pre-init call crash and full lifecycle still need investigation.
- Bundled runtimes and transitive licenses require review before distributing binaries.
- Feedback causality and debugger arbitration are unproven.
- STM32F103 ADC integration and GPIO mode coverage require auditing.
- Laboratory Windows versions, hardware limits, and exact first lesson are not yet known.

## Resume checklist

1. Read this file and [AGENTS](../../AGENTS.md).
2. Check local changes before editing; do not overwrite unrelated work.
3. Select the next ready task and review its acceptance criteria.
4. Record execution evidence and update only genuinely completed states.
