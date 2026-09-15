# ADR 0066: Start revisions with a single display-name token

Date: 2026-09-15. Status: accepted bounded SN-021 revision operation.

## Decision

Implement [project display-name revision](../architecture/PROJECT_REVISION.md)
as a pure transformation of complete source bytes. Validate the original, locate
its top-level name through owned syntax, replace only that string value token,
then revalidate/rebuild the graph and provenance. A semantic no-op preserves all
original bytes. Errors distinguish invalid base from invalid revision.

Preserve IDs and every non-edited byte. Do not serialize a reconstructed project
object, accept arbitrary byte patches or treat a mutable/public graph object as
proof of schema validity. The candidate obeys existing declaration and name bounds.
No generic editor framework is necessary to establish this minimal revision path.

## Consequences and next step

Old graph/source captures remain immutable; new provenance refers to new bytes.
The operation carries no session current-version check, pathname lease, save or
overwrite authority. Persistence remains an explicit separate operation; ADR 0064
still governs unsupported overwrite. No resources execute or acquire readiness.

This accepts a minimal edit, not a complete circuit editor. Further editing
features are not selected implicitly. Next define the smallest compilation input
and lowering contract consistent with accepted graph/resource/readiness boundaries,
including stable source mappings and real-engine evidence for any integration
claim. Preserve numerical profiles, MCU/toolchain independence, instrumentation
and SN-017 Python preparation/GDB/fixture ownership. Safe overwrite remains
pending. Full SN-021 is not complete; prepare the requested next-cycle prompt
only after its full acceptance and integration.
