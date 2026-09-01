# ADR 0011: retain replay and a restricted sampled feedback profile

Date: 2026-08-31. Status: accepted from E-03 evidence; production extraction pending.

## Context

[E-03](../experiments/E-03-results.md) coupled the pinned Renode and ngspice engines with owned STM32 firmware and the predeclared RC/Schmitt policy. Known-schedule replay met the numerical and exact-final-boundary gates. Sampled exchange met its `Q + 2 us` delay gate at 1000 us, 100 us, and 20 us, but all standard feedback events reached Renode after their causal crossing. ngspice also stopped 100 ns after intermediate requested boundaries. No lookahead, I/O-boundary stop, cancellation, or coordinated rollback appeared.

## Decision

Retain P0 known-schedule replay as causality-preserving for fully declared inputs. Accept P1 bounded sampled exchange only as an experimental approximation whose configuration records quantum, thresholds, numerical limits, observed delay, pulse range, and late-event diagnostics. Never label P1 causal solely because its error decreases with `Q`.

Keep P2 general live causal feedback unsupported. Defer a focused Renode extension until a bounded lesson or later experiment requires exact feedback. E-04 may investigate ADC through P0 or explicitly labelled P1. E-05 must still establish coordinated debugger stopping. Do not extract a production kernel from E-03 alone.

## Alternatives

- Promoting the 20 us result to causal was rejected because both standard crossings were still applied after Renode passed their effective times.
- Silently moving a detected crossing to the next boundary was rejected; the trace preserves crossing, ceiling conversion, application, and late status separately.
- Implementing an immediate custom stop/lookahead extension was deferred because replay and measured approximation permit the next bounded experiments, while the exact classroom lesson has not yet required P2.
- Abandoning coupling entirely was rejected because P0 and P1 provide useful, reproducible teaching evidence when their limits are visible.

## Consequences

- The application must display or export the active temporal profile and approximation status.
- An exact final state does not make intermediate sampled feedback causal.
- Late source and consumer events are diagnostics, not retimestamping opportunities.
- Direct 1 us Boolean pulses worked in the tested profile; zero-duration opposite levels collapsed. Neither result establishes electrical or physical pulse fidelity.
- Backend connection failure invalidates the joint commit and requires a fresh worker under the current contract.
- General feedback-dependent lessons remain at risk until requirements select a restricted profile or an extension is proven.

## Revisit criteria

Revisit when E-04/E-05 need stronger coordination, a selected classroom lesson requires exact feedback, an engine provides verified lookahead or I/O-boundary stops, coordinated checkpoint/rollback is implemented, or a dependency revision changes the measured timing. A replacement must rerun the boundary, pulse, late-input, and failure gates.
