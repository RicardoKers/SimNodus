# Project format

Status: [project declaration baseline 0.1](PROJECT_SCHEMA.md) is accepted for
SN-020 specification/reference validation. The native [bounded project lifecycle](../experiments/SN-021-project-lifecycle.md)
now acquires owned documents and supports atomic create-only save/copy. Safe
overwrite and full executable-project acceptance remain pending. Extensions such
as `.snod` and `.snodsub` remain unselected.

The [topology-only 0.1 draft](TOPOLOGY_DRAFT.md) is the first SN-020 slice, with
structural validation and JSON round-trip fixtures. It does not define the whole
project format below or implement resource handling, saving or compilation.
The separate [0.2 parameter draft](PARAMETERS_DRAFT.md) adds exact quantities and
scoped instance overrides; it did not cover the later project/resource contracts.
The [0.3 descriptor draft](BINDINGS_DRAFT.md) separates symbols and model interfaces
with explicit mappings; it does not open graphical/model resources.
The separate [resource lock 0.1](RESOURCE_LOCK_DRAFT.md) validates an inert file
inventory and lexical paths. Physical byte capture is implemented separately in SN-021; these metadata
declarations alone are not verification, interpretation or execution approval.

## Principle

Store editable sources in a directory with readable JSON and relative resources. This supports review and version control. A ZIP exchange format may follow; a monolithic archive does not provide the same text diffs.

```text
lesson-project/
  project.json
  circuits/main.json
  components/...
  subcircuits/...
  firmware/src/...
  firmware/build/...        # generated; excluded from Git
  assets/...
  dependencies.lock.json
```

Names are illustrative. No executable circuit project is included in this foundation.

## Required information

- Schema version, project ID, and metadata; distinguish schema and application versions.
- Circuits, instances, parameters, nets, ports, and persistent IDs.
- Layout/symbols separated from connectivity.
- Board/MCU profile, ELF path and hash; board and SoC are different concepts.
- Locked dependencies with ID, version, origin, and content hash.
- Temporal configuration, tolerances, and fidelity policies.
- Model/license references without credentials or mandatory personal paths.

Results, caches, and debugging sessions are not source documents. Trace exports include a reproduction manifest and do not modify the original circuit.

[Persistent probes](DESKTOP_UX.md#shared-definitions-and-data-ownership) are
editable measurement definitions associated with stable entity/instance IDs,
quantity and explicit reference/orientation, rather than labels alone. Their
captured samples/events remain results. Temporary inspection does not create
persistent project edits. The probe schema and migration rules remain pending;
this requirement does not extend the experimental SN-020 drafts.

[Workspace geometry](DESKTOP_UX.md#circuit-editor-layout) is desirable presentation
state. Its storage location, format and relationship to portable project data
remain open; no personal monitor layout is made mandatory for opening a project.

## Validation and evolution

Reject unsupported major versions clearly. Create round-trip fixtures before migrations and preserve backups. Specify treatment of unknown fields rather than dropping user data silently.

Save through temporary writing and atomic replacement where supported; a failed save must not truncate the original. Other-tool importers are later scope.

## Paths and security

Portable resources resolve inside the project/package root. Validate absolute paths, `..`, symlinks/junctions escaping the root, sizes, entity counts, and hierarchy depth. External imports either copy resources or create explicitly nonportable references.

Future archive import must limit extracted size, file count, compression ratios, and destination paths. Opening a manifest must not fetch URLs, run host executables, invoke compilers, or load DLLs.
