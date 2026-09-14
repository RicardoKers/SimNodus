# ADR 0057: Native exact parameter validation and inspection

Date: 2026-09-14. Status: accepted bounded implementation contract.

## Decision

Implement the [native parameter contract](../architecture/NATIVE_PARAMETERS.md)
for preserved topology 0.2. Use bounded signed decimal coefficients and powers
of ten for exact unit scaling, comparison and fixed-point inspection strings.
Do not introduce a floating-point conversion, arbitrary expression evaluator or
downloaded numeric dependency. Keep the existing grammar/precision/unit limits.

Share the existing bounded structural validator internally while retaining
separate strict public 0.1 and 0.2 entry points. The structural phase accepts only
known required parameter additions; complete parameter validation precedes any
snapshot. Retain original source offsets rather than serializing a projection.

## Consequences

Each occurrence owns effective parameter values and origins; definitions remain
immutable. Forwarding is explicitly scoped to its containing occurrence and must
fit the target interval for every legal source value. Check expansion costs for
unused definitions before resolving the selected root. Public results are inert
inspection data, not a complete project or runnable graph.

The Python reference and historical evidence remain intact. Differential tests
must check decimal boundary behavior and exact formatted values, as well as
rejections. Descriptor/resource/project semantics, atomic saving, compilation
and runtime verification remain later SN-021 gates. No UI, engine profile,
MCU/toolchain dependency or SN-017 orchestration change is included.
