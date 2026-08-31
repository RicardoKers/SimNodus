# Current state

Updated: 2026-08-31.

## Snapshot

- Stage: M0 repository foundation.
- Simulator implementation: not started.
- Direction: C++20 baseline, Qt 6 presentation, ngspice/XSPICE and Renode behind adapters.
- Platform: Windows first; Linux later.
- Repository language: English only.
- License: MIT for original project material, selected with the owner's authorization.
- Intended classroom use: February 2027; January is stabilization/rehearsal time.
- Author and maintainer: Ricardo Kerschbaumer.
- Git: initial commit and public remote are being prepared.
- Publication: authorized at `RicardoKers/SimNodus`; SN-041 is in progress.

## Completed work

Requirements from the two architecture conversations have been consolidated. Architecture, temporal risks, component/subcircuit design, debugging requirements, ADRs, milestone planning, and experiment specifications have been prepared. Repository conventions, MIT licensing, contribution/security guidance, issue/PR templates, and a local/CI documentation check are included.

The [backlog](BACKLOG.md) owns task status. Local verification results are recorded in [QUALITY](../development/QUALITY.md); no backend experiment has passed.

## Next task: SN-010

Inventory the Windows toolchain and select an exact Renode executable/client/platform revision and ngspice shared-library revision. Record provenance, license details, download/build recipe, and checksums in [DEPENDENCIES](../research/DEPENDENCIES.md).

Then execute E-01 and E-02 from the [experiment plan](../experiments/README.md). The first useful deliverable is a reproducible standalone engine report, not a GUI.

## Known uncertainties

- API versions found during research differ; match documentation, headers, and binaries.
- Feedback causality and debugger arbitration are unproven.
- STM32F103 ADC integration and GPIO mode coverage require auditing.
- Laboratory Windows versions, hardware limits, and exact first lesson are not yet known.

## Resume checklist

1. Read this file and [AGENTS](../../AGENTS.md).
2. Check local changes before editing; do not overwrite unrelated work.
3. Select the next ready task and review its acceptance criteria.
4. Record execution evidence and update only genuinely completed states.
