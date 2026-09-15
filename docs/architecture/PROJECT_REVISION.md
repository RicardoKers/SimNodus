# Minimal source-preserving project revision

Status: bounded SN-021 operation under [ADR 0066](../decisions/0066-project-name-revision.md).

`rename_project(original_bytes, name_utf8)` changes only the top-level project
display name. It performs no filesystem or resource I/O. Inputs are borrowed and
must remain stable for the call. Output is an owned immutable `ProjectGraph` or
a staged error. This is a value transformation, not a session transaction.

## Contract and acceptance

- Validate/load the complete original document first. Invalid input is never
  repaired by renaming, and no edit is applied to caller storage. Use owned
  validated syntax to select the one top-level `name` string by decoded key and
  token span. Do not search/replace matching text or trust caller-supplied spans.
- Treat the new name as UTF-8 string content, not a JSON fragment. Escape quotes,
  backslashes and controls when constructing its one JSON token. Keep scalar,
  forbidden-control and 1..80 character checks in the existing project validator.
  Reject more than 320 UTF-8 bytes before encoding; no valid 80-scalar name needs
  more. Invalid UTF-8 and surrogate scalars must fail validation.
- If the decoded name is unchanged, return the validated base without rewriting
  any source bytes. This preserves original escape spelling and whitespace.
- Otherwise replace exactly the original value token. Preserve every byte before
  and after it, including key spelling, ordering, whitespace, numeric spelling,
  nulls, metadata and nested names. Keep the candidate within the existing 1 MiB
  declaration limit. Reject growth beyond it before constructing a full candidate.
- Revalidate/load the complete candidate through the existing graph loader.
  Rebuild provenance against the revised bytes; old byte offsets must not be
  reused as current locations. Stable IDs, graph connectivity and all non-name
  declarations remain unchanged. A returned shared copy survives input/result
  release. Repeated edits are explicit transformations of previous source bytes.
- Base errors refer to original byte offsets; revision validation errors refer
  to candidate offsets. Name preflight errors identify the original name start,
  which is also its candidate start. Whole-document byte-budget errors use zero.
  No candidate or partial result is exposed on failure. Memory errors retain the
  active stage. No history, version token or optimistic-write check is implied.

## Boundaries

This operation edits neither IDs, connectivity, component names, parameters nor
resource declarations. It adds no generic patch API, editor UI, undo system,
collaboration protocol or migration. The caller may pass owned captured source
bytes, but an arbitrary public graph object is never trusted as validation proof.
Existing schema/source limits and numerical profiles remain unchanged.

The result does not save anything or authorize replacement of a pathname. Keep
ADR 0064's overwrite gate and the acquisition capture/lease distinction. A caller
must explicitly select any subsequent create-only save. No DLL, model, firmware,
rendering, download or engine execution follows from editing. Declarative validity
does not prove contained resources, interface compatibility, trusted origin,
redistribution permission or simulation readiness.

The test driver compares the result with an independent JSON decoder, unchanged
Python validators and complete graph/provenance expectations. Pure value tests
are sufficient for this operation; they make no real-engine integration claim.
Compilation needs separate readiness/lowering criteria and real-engine evidence.
