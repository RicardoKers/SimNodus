# ADR 0003: Components, hierarchy, and persistence

Date: 2026-08-31. Status: accepted direction; schema proposed.

## Context

The owner wants straightforward custom components and subcircuits resembling reusable circuits in existing educational digital tools.

## Decision

Separate symbols, pin identity, parameters, and simulation behavior. Preserve hierarchy in the project graph, even if a backend requires flattening. Subcircuits have explicit ports and independent per-instance state.

Prefer a directory containing versioned, readable JSON and relative resources. Reserve project-local and user-library discovery with explicit dependency identity. Do not finalize filename extensions or public plugin ABI yet.

## Consequences

Netlists are generated artifacts. Portable dependencies and source mappings require deliberate design. Discovery must not execute packages. WASM is a later SDK direction, not a prerequisite for the first component.

## Revisit when

Round-trip/schema experiments reveal migration issues or packaging requirements justify a separate archive exchange format.
