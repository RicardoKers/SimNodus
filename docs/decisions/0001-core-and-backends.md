# ADR 0001: Core and backend boundaries

Date: 2026-08-31. Status: accepted direction; concrete integration pending experiments.

## Context

The owner approved C++/Qt, ngspice + Renode, and a small initial proof. A GUI-first implementation would leave the highest-risk integration unanswered.

## Decision

Use a headless C++ core with an independent circuit graph and engine adapters. Propose C++20 and CMake as the initial build baseline; adopt C++23 features only when the compiler matrix justifies them. Use Qt 6 for desktop presentation. Begin with ngspice/XSPICE and Renode; do not build another analog solver or ARM emulator.

Initially host one ngspice instance and control a separate Renode process. Exact versions, API transport, and eventual simulation-worker isolation remain experimental.

## Consequences

Core tests and CLI experiments need no Qt. Adapters contain third-party concerns. The first iteration can be small, but synchronized feedback still requires substantial engineering.

## Revisit when

A verified backend limitation prevents the required lessons, or measured stability/performance demands another process boundary or engine. Record evidence before replacing a backend.
