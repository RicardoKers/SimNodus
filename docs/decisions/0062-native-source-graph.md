# ADR 0062: Native source connectivity with separate metadata and provenance

Date: 2026-09-15. Status: accepted for the bounded SN-021 source graph slice.

## Decision

Introduce the [source connectivity graph](../architecture/NATIVE_SOURCE_GRAPH.md)
as standard C++ domain values: definitions, pins, ports, instances, nets and typed
terminal references. Keep hierarchy and scoped stable IDs instead of flattening
or generating backend nodes. Display names remain independent from connectivity.

The application loader validates project 0.1 before building an immutable result.
It retains the complete owned declaration and a separate source map for graph
records/terminals, keyed by IDs and endpoint selectors with absolute byte spans.
Domain values carry no JSON/Qt/backend dependency. Exact parameter occurrences and
all non-connectivity metadata remain in the same retained declaration boundary.

## Alternatives and consequences

Using parser tokens as the circuit graph would couple domain traversal to input
syntax. Flattening during load would discard reusable source hierarchy and confuse
definition identity with occurrence identity. Reconstructing a partial project
would risk dropping unrepresented metadata. The composed result avoids those
changes while allowing independent graph traversal/copy and source diagnostics.

This slice does not introduce editing transactions, save serialization or executable
compilation. Graph acceptance does not verify electrical solvability, physical
resources, firmware interfaces, licensing/trust or runtime capabilities. No engine
integration claim, new MCU/toolchain dependency or simulation profile follows.

## Next acceptance

Implement safe bounded persistence of owned source bytes with destination ownership,
failure preservation and atomic replacement evidence. Then implement compilation
with stable source mappings and the real-engine acceptance its supported profile
requires. Keep opening, verifying bytes, importing and execution as distinct gates.
