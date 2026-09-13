# ADR 0048: Inert locked resource inventory

Date: 2026-09-13. Status: accepted for an experimental SN-020 metadata slice;
resource loading and full project schema remain pending.

## Decision

Specify a separate [resource lock 0.1](../architecture/RESOURCE_LOCK_DRAFT.md),
with exact inventory versions, origin and license declarations, notice resources,
byte counts, file hashes and a deterministic inventory fingerprint. Retain all
topology drafts without migration. Require conservative portable lexical paths,
global case-insensitive uniqueness, bounded counts/sizes and strict fields.

The reference validator is pure metadata validation. Always report physical
containment, resource verification, redistribution verification and simulation
readiness false. Explicitly distinguish declared hashes from measured bytes.
Specify a future handle-based physical containment gate; do not implement a
path-prefix check that could be mistaken for safe resource consumption.

## Alternatives and consequences

Adding filesystem loading now would conflate SN-020 schema work with SN-021
persistence. Inferring approval from a hash or license string would trust the
untrusted manifest itself. Automatic acquisition, execution hooks, package
solving and transitive dependency graphs are outside this bounded inventory.

ASCII paths and numeric version labels deliberately exclude otherwise legitimate
filenames/version schemes. They can be revisited with explicit compatibility
evidence. Descriptor/resource links, board/firmware and temporal policies remain
pending. SN-020 stays in progress and SN-021 planned. ADRs 0027/0028, existing
backend restrictions, tolerances, PDF suppression and PID retry remain unchanged.

## Revisit criteria

Define typed descriptor/resource references next. Before actual resource access,
test physical containment and byte verification on Windows, including junctions,
aliases and replacement races. Before incorporating external files, verify their
origin and redistribution permission; declared metadata alone is insufficient.
