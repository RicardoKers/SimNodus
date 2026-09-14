# Native topology semantics

Status: first native semantic slice of SN-021, under
[ADR 0056](../decisions/0056-native-topology-semantics.md).

Validate the complete preserved [topology 0.1 contract](TOPOLOGY_DRAFT.md) over
the [native syntax capture](DECLARATION_INGRESS.md). This version is the existing
structural foundation, not a replacement for project 0.1's topology 0.3. Reject
later versions here rather than silently projecting away their fields. Native
parameters, bindings, resource lock/links and project semantics remain pending.

## Acceptance criteria

- Accept caller bytes only; retain immutable original bytes/tokens and source
  offsets. No paths, callbacks, resource opening, model interpretation or engines.
- Enforce exact root/nested fields and types, stable IDs, Unicode display names,
  electrical domains, port directions and instance kinds. Definition IDs share
  a namespace; circuit ports/instances/nets share a local namespace. No repairs.
- Resolve all definition, instance, pin and port references by IDs and kind,
  independent of definition order. Reject duplicate terminal connections across
  nets; display names, including GND, never join nets implicitly.
- Validate unused definitions too. Reject direct/indirect cycles, hierarchy
  deeper than eight circuit definitions, more than 4096 declared entities and
  more than 16384 expanded occurrences for any definition. Count repeated
  instances separately; use bounded memoized traversal, not expansion allocation.
- Return a complete topology-only result (captured syntax, root ID, entity count,
  root depth and expanded count) or a structured error code and byte offset.
  Errors correspond to the existing semantic categories; first-error order and
  byte positions are developer diagnostics, not a public automation API.
- Reuse the preserved Python topology suite as a differential oracle. Add exact
  hierarchy/entity/expansion boundaries, order independence, Unicode names,
  namespace collisions, wrong shapes and ownership/source-location checks.
  Keep all existing syntax/schema/resource tests passing without rewriting them.

The existing stricter native UTF-8/lone-surrogate ingress gate remains in force.
Display-name length counts Unicode scalars, not UTF-8 bytes or UTF-16 code units.
Names containing U+0000..001F or U+007F..009F are rejected as in the reference.
Input remains bounded to 1 MiB and 32 JSON container levels. Memory allocation
failure is structured; no hard OS memory or wall-time quota is claimed.

Success does not publish an editable domain graph or establish electrical
solvability, resource correctness, trust/redistribution or simulation readiness.
Later semantic slices must preserve source IDs/positions and explicit unbound
states before graph construction. Atomic saving, source compilation, UI,
firmware/boot, runtime capabilities and SN-017 orchestration remain unchanged.
