# ADR 0058: Native descriptor and binding validation

Date: 2026-09-14. Status: accepted for the bounded SN-021 topology 0.3 slice.

## Decision

Implement [native 0.3 validation](../architecture/NATIVE_BINDINGS.md) using the
same captured syntax, structural validator and exact decimal parameter resolver.
Keep the public 0.1/0.2 version gates strict. Internal structural validation alone
cannot publish a successful 0.3 declaration: parameters and descriptors must pass.

Retain complete catalogs, maps, explicit nulls and source positions in owned
syntax. Publish only immutable parameter occurrences, declaration statistics and
false simulation readiness. Preserve the 4096 combined budget and earlier limits.
Symbols, logical pins and model terminals remain distinct, explicitly mapped IDs.

## Alternatives and consequences

A rewritten 0.2 projection could lose original positions or discard fields.
A separate numeric implementation could diverge at exact interval boundaries.
Reusing internal phases avoids both without weakening older public contracts.
Keeping descriptor validation beside the private exact resolver shares its bounded
decimal operations without exposing a general arithmetic or expression API.

No physical resources or engines are opened. Declared interface agreement is not
source-interface verification, trust, redistribution or execution approval. This
adds no UI, engine profile, public automation API or native editable graph.
Existing accepted profiles and the SN-017 Python/GDB boundary remain unchanged.

## Revisit criteria

Port resource lock/link and project declaration semantics next, before native
source graph loading. Actual resource interfaces, atomic saving, compilation and
runtime authorization remain separate SN-021 gates with their own evidence.
