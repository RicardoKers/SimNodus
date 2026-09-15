# ADR 0065: Acquire a project into owned bytes before graph loading

Date: 2026-09-15. Status: accepted bounded SN-021 acquisition contract.

## Decision

Compose [physical acquisition](../architecture/PROJECT_ACQUISITION.md) with the
accepted project 0.1 validator and graph loader. Reuse unchanged Windows/NTFS
handle and path rules, select a single leaf, and capture at most 1 MiB through
one opened file while retaining its ancestors. Validate owned bytes after closing
physical handles. Return an immutable source graph with original declaration and
source mapping, or an error with physical/declaration stage and original offset.

This is an explicit application operation. The selected root/leaf come from the
caller, not the document. It does not acquire any referenced resource, create a
runtime, load a library chosen by metadata or authorize saving over the path.

## Consequences

Path mutation after capture cannot change what the validator or caller consumes.
The result is not a pathname lease: another writer can replace the original file
after handle release. Preserve ADR 0064 rather than inventing an overwrite token
from this capture. Metadata, resource bytes, interfaces, trust and runtime gates
remain separate. Existing parser limits and source semantics are unchanged.

No session-wide locking, cancellation, editing, save-over-existing, resource
import, migration, UI or engine capability is added. Linux rejects acquisition
after path validation. Existing platform limitations remain explicit.

Next define a minimal source-preserving revision/edit contract on owned captured
documents, with revalidation and stable IDs/provenance before any save-as or
compilation. Safe overwrite remains pending its identity/version acceptance gate;
compilation needs its own source mapping and real-engine evidence. Full SN-021
is not complete and the next-cycle prompt remains pending full acceptance.
