# Current state

Updated: 2026-08-31.

## Snapshot

- Stage: M0 published; SN-010 baseline, SN-011 / E-01, and SN-019 Windows control prerequisite complete.
- Implementation: standalone ngspice RC/lifecycle and native Renode control/time experiments work; production kernel and application not started.
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

SN-019 adapted the pinned C client for native Windows and generated a separate loopback-only server extension from the matching official sources. Twenty cases passed in two local runs: real handshake/time control/reconnection with normal and one-byte transfers, plus separate fault/input tests. Actual listener ownership/address and cleanup were verified. Each real run advanced an empty machine from zero to 6,018,000 us with exact requested differences. The original all-interface server was not opened.

The [backlog](BACKLOG.md) owns task status. See [SN-010](../experiments/SN-010-results.md), [E-01](../experiments/E-01-results.md), [SN-019](../experiments/SN-019-results.md), [ADR 0008](../decisions/0008-windows-renode-control.md), and [QUALITY](../development/QUALITY.md). E-02 through E-06 have not run. No firmware or coupled simulation has run.

## Next task: SN-012 / E-02 standalone Renode

Follow [SN-012 / #3](https://github.com/RicardoKers/SimNodus/issues/3): build owned firmware using the existing ARM toolchain, define an offline C8 platform, and measure GPIO output/input and relevant peripheral behavior. Reuse the verified [SN-019 control recipe](../../tests/experiments/renode-client/README.md) and microsecond API; do not start the original all-interface listener. No GUI or SDK work is needed yet. Use a branch and pull request; do not bypass main protection.

## Known uncertainties

- The adapted Renode client covers the reported transport/time profile; GPIO/ADC/system-bus behavior and callback ownership still need E-02/E-04 evidence.
- Client timeout/disconnect does not establish that an accepted Renode run stopped. The 1000 ms deadline is an experiment setting, not a product timing guarantee.
- The explicit loopback extension, paths without whitespace, and per-run temporary directories are experiment constraints, not a finished packaging design.
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
