# Risk register

Baseline: 2026-08-31. Likelihood and impact are qualitative estimates, not measured probabilities. Review after each experiment and milestone.

| ID | Risk | Likelihood / impact | Early signal | Mitigation / decision |
|---|---|---|---|---|
| R-01 | Feedback breaks causality | High / critical | GPIO/input discovered after consumer has advanced | E-03; conservative boundary integration or explicitly limited scope |
| R-02 | Renode version/API mismatch | Medium / high | Headers, docs, and binary disagree about time units | Pin matching executable/client/platform; record API behavior |
| R-03 | STM32F103 peripheral gaps | High / high | Missing ADC, mode changes, clock or interrupt behavior | Per-feature audit; focused extensions with tests, not global support claims |
| R-04 | Debugger bypasses scheduler | High / critical | GDB continue/step advances MCU independently | E-05; arbitration and failure reporting before GUI integration |
| R-05 | Numerical issues or false electrical fidelity | Medium / high | Nonconvergence, step-dependent result, unexplained thresholds | Reference circuits, finite drivers, tolerances and model assumptions |
| R-06 | Native backend crashes/hangs | Medium / high | Process exits, callbacks stall, resources leak | Timeouts, lifecycle tests, simulation-worker isolation for desktop |
| R-07 | Third-party redistribution restrictions | Medium / high | Vendor model or Qt module lacks suitable terms | Inventory per asset/module; avoid unreviewed bundles |
| R-08 | February scope exceeds capacity | High / high | M2 slips past October | Freeze a tested teaching subset; defer SDK/HDL/catalog expansion |
| R-09 | Windows setup friction | Observed / high | SN-010: native Renode client build failure; ngspice missing audio DLLs and pre-init crash | Tested ngspice setup; SN-019 client adaptation; E-01 lifecycle audit; clean-machine rehearsal still pending |
| R-10 | Untrusted project/model execution | Medium / high | Path escape, native load, excessive allocation | Validate before simulation; safe defaults; trust boundary tests |
| R-11 | Lab constraints discovered late | Medium / high | No admin rights, low RAM, no network | Ask early; rehearsal in January; offline-ready materials |
| R-12 | Name/ownership or private-data issue at publication | Unknown / medium | Collision, personal paths, unclear attribution | Publication review; no raw chat export; confirm copyright contact |
| R-13 | Documentation drifts from implementation | Medium / medium | Roadmap says done without report | CURRENT/backlog discipline; ADRs and automated link checks |

## Escalation rules

A failed correctness gate changes the plan; it must not be hidden behind smoother graphics or smaller demonstrations without documenting the restriction. Escalate a material deadline/scope change to the owner with the experiment result and viable alternatives.

No simulator result certifies that a physical circuit is safe. Teaching warnings are bounded by the implemented model and validated parameters.
