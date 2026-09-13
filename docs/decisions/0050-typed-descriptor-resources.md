# ADR 0050: Typed descriptor/resource references

Date: 2026-09-13. Status: accepted for bounded SN-020 reference validation;
overall project/component schema remains pending.

## Decision

Use the separate [resource-links 0.1](../architecture/RESOURCE_LINKS_DRAFT.md)
document to compose existing topology and lock declarations without migrations.
Resolve descriptors through asset IDs and explicit dependency/resource pairs.
Require role agreement, complete bindings or explicit null, unique resource roles,
and explicit model entrypoint and formal-token mappings. Never infer an association
from names, file suffixes, catalog order or missing bindings.

Keep metadata parsing inert. Physical containment, resource bytes, license review,
SVG anchors and actual model interfaces remain independent unverified gates.
Replacing visual associations must preserve graph/model/parameter data.

## Alternatives and consequences

Direct file paths in descriptors would bypass locking and conflate identity with
storage. Implicit model selection or source-name inference would hide unresolved
interfaces. Loading/rendering resources during validation would cross the
untrusted-input boundary and is unnecessary for this metadata contract.

The only declared roles in this slice are SVG symbol and SPICE model source;
these names grant no renderer, solver, device or peripheral support. A shared
SPICE resource requires explicit entrypoints, not a guessed default subcircuit.
Future importers must verify actual formal interfaces before compilation.

SN-044 and ADR 0049 remain unchanged: preview requires no model execution;
persistent measurements belong to shared instrumentation and stable domain IDs.
No Qt choice, UI implementation or measurement schema is introduced. Keep
ADRs 0027/0028, all prior engine evidence, tolerances, PDF suppression and PID retry.

## Revisit criteria

Complete project-level board/MCU/firmware and temporal policy contracts before
SN-020 acceptance. Add new resource roles only through an explicit schema change.
Physical loading/importer interface checks stay in SN-021. This decision does
not approve an executable project, a plugin ABI or general simulation support.
