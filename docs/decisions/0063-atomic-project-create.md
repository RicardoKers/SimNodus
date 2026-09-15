# ADR 0063: Create-only atomic project publication

Date: 2026-09-15. Status: accepted for the bounded SN-021 persistence slice.

## Decision

Start persistence with [atomic creation](../architecture/ATOMIC_PROJECT_CREATE.md)
of exact validated project bytes in an explicitly selected existing directory.
Use the existing Windows/NTFS path and handle protections, an exclusively created
temporary, bounded writes/flush and a single handle-relative rename with replacement
disabled. All metadata is validated before writes. No resource is opened or executed.

Keep root/name authority outside the untrusted manifest. An existing destination
always blocks publication, including an entry created after preparation. Ordinary
failure deletes only the owned temporary through its handle and reports cleanup
failure. A killed writer can leave an orphan; no recovery/scavenging policy is
implied. Atomic publication is not a power-loss durability guarantee.

## Alternatives and consequences

Truncating the destination would destroy its previous content on write failure.
Checking a pathname and then replacing it would not protect a concurrent owner's
entry. Create-only native rename establishes the smallest useful persistence
contract without pretending that safe overwrite ownership has been solved.

Shared internal helper extraction keeps containment rules aligned with the accepted
resource verifier; full resource regressions remain required. The operation saves
an already validated source document, not a serialization of arbitrary mutable
domain structs. It adds no extension, migration, UI or engine capability.

## Next acceptance

Define and prove safe save-over-existing semantics, including destination identity,
concurrent changes, failure preservation and supported filesystem behavior. Path
acquisition/editing and source-preserving compilation remain pending. SN-021 is
not complete; prepare the next-cycle prompt only after its full acceptance.
