# ADR 0005: Classroom target and scope

Date: 2026-08-31. Status: accepted owner target; delivery scope remains conditional.

## Context

The intended classes begin in February 2027. The project starts without a simulator implementation or measured team capacity.

## Decision

Prioritize backend validation, then a small Windows teaching MVP. Reserve January for stabilization, installation rehearsal, lesson guides, and a go/no-go review. Defer WASM, HDL, large component catalogs, and a Linux application release from the initial classroom commitment.

Reusable subcircuits remain a core product requirement. Design for hierarchy early; implement the first authoring workflow before class only if the technical and teaching MVP gates are already satisfied.

## Consequences

A classroom release may expose only a validated subset. If co-simulation or debugging gates fail, revise the lesson scope and preserve physical hardware/other tools as fallback rather than claiming unsupported behavior.

## Revisit when

September capacity review, October integration review, December feature freeze, and January rehearsal provide new evidence.
