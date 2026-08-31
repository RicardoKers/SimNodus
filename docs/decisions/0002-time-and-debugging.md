# ADR 0002: Virtual time and debugging

Date: 2026-08-31. Status: accepted invariants; synchronization algorithm pending.

## Context

MCU execution, analog integration, digital events, and debugger actions must describe one consistent experiment. Host clock speed and internal engine resolution do not establish this automatically.

## Decision

The kernel owns session time. Reject late events instead of silently retimestamping. Label any approximate coupling mode. Propose integer event time, stable same-time ordering, bounded delta iterations, and explicit committed states.

Preserve CubeIDE/GDB integration. Coordinate pause, step, continue, reset, and failure across domains. Test GDB arbitration in E-05 before treating debugging as supported.

## Consequences

A successful one-way GPIO replay is insufficient for feedback. No global rollback or event prediction is assumed. If backend APIs cannot satisfy the contract, a focused extension or a reduced validated scope is needed.

## Revisit when

E-03/E-05 establish the exact stop/callback semantics, or later multicore/multi-MCU/HDL support requires a revised synchronization model.
