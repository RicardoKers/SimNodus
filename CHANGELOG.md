# Changelog

All entries describe actual repository changes, not promised simulator capabilities.

## Unreleased

### Added

- E-03 real Renode/ngspice GPIO/RC/EXTI coupling host, owned firmware, 54-case repeated matrix, compact evidence, result report, ADR 0011, and Windows workflow.
- Measured known-schedule replay and bounded sampled feedback at 1000 us, 100 us, and 20 us, including boundary, pulse, late-input, forced-failure, and fresh-recovery cases.
- SN-013 temporal capability profile and ADR 0010: checked time conversion, measured backend semantics, supported replay, explicitly approximate sampled exchange, unsupported live-feedback operations, and predeclared E-03 gates.
- Owned STM32F103C8 firmware, offline exact-memory profile, real GPIO/input/EXTI experiment, coverage audit, evidence report, ADR 0009, and repeated Windows workflow.
- Native Windows Renode control/time experiment, generated loopback-only server, 20-case suite, pinned source provenance, and a dedicated Windows workflow.
- SN-019 report and ADR 0008: measured timing/reconnection, bounded failures, safe error ownership, and explicit network exposure restrictions; E-02 is now ready.
- Real ngspice RC/lifecycle experiment with eight cases, three recorded local runs, analytical checks, a reference chart, and an opt-in Windows CI workflow.
- ADR 0007 records measured trial/callback, pause, reset, error, and worker-lifetime semantics; no coupled simulation is claimed.
- Pinned Windows backend packages, SHA-256 verifier, C++20 ngspice startup probe, reproducible setup guide, and SN-010 execution report.
- ADR 0006 and SN-019 for the verified native Windows Renode client incompatibility; E-02 external control requires that adaptation.
- English product requirements, architecture, backend/time contracts, and component/subcircuit design.
- Architecture decisions and a Windows-first roadmap targeting classroom use in February 2027.
- Backlog, risk register, open questions, and reproducible experiment specifications.
- MIT license for original project material and third-party review guidance.
- Development scaffolding, repository checks, and GitHub issue/PR/CI templates.
- Public repository under RicardoKers, with Ricardo Kerschbaumer as the initial author.
- Nine initial issues, four dated milestones, private vulnerability reporting, and main-branch protection requiring Windows/Ubuntu checks.

### Not implemented

Production simulation kernel/adapters, general causal feedback, ADC coupling, circuit editor, project loader, component runtime, and synchronized debugging.
