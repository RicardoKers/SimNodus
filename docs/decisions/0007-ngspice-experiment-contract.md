# ADR 0007: Carry measured ngspice semantics into the next experiments

Date: 2026-08-31. Status: accepted for the E-01 standalone profile; no production adapter or coupled algorithm approved.

## Context

[E-01](../experiments/E-01-results.md) reproduced an RC reference, external voltage callbacks, retries, pauses, resets, and invalid-netlist recovery in ngspice 47. Several names/comments and return codes are insufficient to infer the actual behavior.

## Decision

- Keep trial-source queries and accepted sample notifications separate. Sources must tolerate repeated/decreasing trial times. Copy borrowed vectors immediately; never keep them across reset or later engine mutations.
- Register both data callbacks. Keep callback handlers bounded and exception-safe. Issue control commands from the host, not from callbacks.
- Treat `ngSpice_SetBkpt` as an integration boundary, and expose measured effective pause time separately from the request. Do not infer a coupled advance-until capability from E-01.
- For the tested Windows background path, confirm actual worker termination before unloading the DLL or destroying callback state. Use the pinned source's flag semantics, not the contradictory header comment.
- Validate diagnostics and expected outputs alongside return codes. Full library reset requires reinitialization, callback registration, and reload.
- Retain the owned startup configuration. The previously observed pre-init crash remains an unsupported route, not a fixed defect.

## Consequences and revisit criteria

The next Renode experiment can use real, bounded ngspice evidence. SN-013 must still define the joint temporal profile after E-02; SN-014 must test feedback causality. Do not generalize this RC result to nonlinear circuits, digital code models, arbitrary reentrancy, hard real-time stops, or multi-engine rollback. Revisit after any dependency change or an experiment that contradicts the measured profile.
