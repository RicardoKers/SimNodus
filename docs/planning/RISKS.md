# Risk register

Baseline: 2026-08-31. Likelihood and impact are qualitative estimates, not measured probabilities. Review after each experiment and milestone.

| ID | Risk | Likelihood / impact | Early signal | Mitigation / decision |
|---|---|---|---|---|
| R-01 | Feedback breaks causality | High / critical | GPIO/input discovered after consumer has advanced | SN-013 limits current support to replay and measured sampled approximation; E-03 must prove any causal profile or require an extension |
| R-02 | Renode version/API mismatch | Medium / high | Headers, docs, and binary disagree about time units | Pin matching executable/client/platform; record API behavior |
| R-03 | STM32F103 peripheral gaps | High / high | Missing ADC, mode changes, clock or interrupt behavior | Per-feature audit; focused extensions with tests, not global support claims |
| R-04 | Debugger bypasses scheduler | High / critical | GDB continue/step advances MCU independently | E-05; arbitration and failure reporting before GUI integration |
| R-05 | Numerical issues or false electrical fidelity | Medium / high | Nonconvergence, step-dependent result, unexplained thresholds | Reference circuits, finite drivers, tolerances and model assumptions |
| R-06 | Native backend crashes/hangs | Medium / high | Process exits, callbacks stall, resources leak | Timeouts, lifecycle tests, simulation-worker isolation for desktop |
| R-07 | Third-party redistribution restrictions | Medium / high | Vendor model or Qt module lacks suitable terms | Inventory per asset/module; avoid unreviewed bundles |
| R-08 | February scope exceeds capacity | High / high | M2 slips past October | Freeze a tested teaching subset; defer SDK/HDL/catalog expansion |
| R-09 | Windows setup friction | Observed / high | Native client/compiler differences, ngspice setup, Renode temporary cleanup/path handling | E-01 and SN-019 passed bounded profiles; fresh per-run configuration/temporary files; clean-machine rehearsal still pending |
| R-10 | Untrusted project/model execution | Medium / high | Path escape, native load, excessive allocation | Validate before simulation; safe defaults; trust boundary tests |
| R-11 | Lab constraints discovered late | Medium / high | No admin rights, low RAM, no network | Ask early; rehearsal in January; offline-ready materials |
| R-12 | Name/ownership or private-data issue at publication | Unknown / medium | Collision, personal paths, unclear attribution | Publication review; no raw chat export; confirm copyright contact |
| R-13 | Documentation drifts from implementation | Medium / medium | Roadmap says done without report | CURRENT/backlog discipline; ADRs and automated link checks |
| R-14 | Control server exposed to the network | Observed / high | Pinned Renode provider binds every IPv4 interface | SN-019 explicit loopback variant; verify actual address/port/PID; never assume a localhost client restricts its server |
| R-15 | Boolean GPIO behavior mistaken for electrical pin fidelity | Observed / high | Pulls, analog mode and open-drain release differ from hardware | Publish E-02 mode findings; model electrical behavior at an explicit coupling boundary |
| R-16 | Input edges delivered too late or collapsed | Observed / high | Same-time input edges produced one pending EXTI interrupt in E-02 | SN-013 coalesces and diagnoses equal-time levels; E-03 measures boundaries, delay, and short pulses; never silently retimestamp |

## Escalation rules

A failed correctness gate changes the plan; it must not be hidden behind smoother graphics or smaller demonstrations without documenting the restriction. Escalate a material deadline/scope change to the owner with the experiment result and viable alternatives.

No simulator result certifies that a physical circuit is safe. Teaching warnings are bounded by the implemented model and validated parameters.
