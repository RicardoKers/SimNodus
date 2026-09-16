# Selected passive interface correspondence

SN-021's `match_passive_interface` accepts a complete resource-links 0.1 document,
an exact descriptor ID and caller-supplied stable source bytes. It revalidates the
whole declaration and inspects the whole bounded R/C source grammar before
matching one selected non-null model binding. No filesystem or engine is opened.

The selected asset resolves through dependency/resource IDs to its lock row.
The owned bytes must match that row's exact size and SHA-256. Entrypoint, formal
terminal names and parameter names compare case-insensitively under the SPICE
grammar. Descriptor IDs remain case-sensitive. Both actual terminals and the one
actual parameter must be covered explicitly. Returned terminal IDs follow actual
formal-source order, independent of descriptor-array or map-key order. Reversed
explicit maps are honored; there is no positional or label-based inference.

The recognized primitive determines the required descriptor unit: `ohm` for R,
`F` for C. Declared minimum/maximum strings are preserved exactly, together with
the complete validated metadata, owned source, model index and metadata source
offsets. The source reader retains the subcircuit span. Output ownership survives
release of caller buffers and the result wrapper. Errors distinguish declaration,
selection, source syntax and correspondence; offsets refer to the associated
metadata/source bytes, with zero used for global selection errors.

## Acceptance boundary

Test both fixture descriptors; reorder descriptors/terminals and reverse explicit
maps; vary source-name case while preserving case-sensitive descriptor identity;
reject missing/null selections, unknown entrypoints/maps, primitive-unit mismatch,
wrong size/hash, unsupported source syntax, over-budget source and invalid unrelated
metadata. Verify exact retained metadata/source and source-offset provenance.
All output declaration readiness flags must remain false.

Matching bytes does not prove physical containment: callers must separately use
the existing retained-handle acquisition and consume captured bytes without
reopening paths. This pure operation also works with synthetic caller bytes and
therefore makes no containment claim. It never infers trust, licensing or authority.

The result is deliberately a correspondence record, not a fully verified model:
it does not convert/evaluate the source default, compare defaults/effective values
against ranges, bind expanded component occurrences, verify SVG interfaces or
authorize analysis. A matching out-of-range source default still returns the
record with all readiness flags false; tests preserve this explicit boundary.
No netlist or engine behavior changes. Real-engine evidence remains the bounded
[owned RC acceptance](../experiments/SN-021-passive-source.md), not proof of new
numerical/profile support. Next implement exact effective-value/range binding
and explicit reference/stimulus/analysis authority before backend lowering.

See [ADR 0069](../decisions/0069-passive-interface-correspondence.md),
[source grammar](PASSIVE_SPICE_SOURCE.md) and [resource links](RESOURCE_LINKS_DRAFT.md).
