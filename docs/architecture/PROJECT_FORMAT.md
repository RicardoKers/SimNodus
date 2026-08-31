# Project format

Status: persistence requirements. Extensions such as `.snod` and `.snodsub` and the exact schema are **not finalized**. No loader exists.

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

## Validation and evolution

Reject unsupported major versions clearly. Create round-trip fixtures before migrations and preserve backups. Specify treatment of unknown fields rather than dropping user data silently.

Save through temporary writing and atomic replacement where supported; a failed save must not truncate the original. Other-tool importers are later scope.

## Paths and security

Portable resources resolve inside the project/package root. Validate absolute paths, `..`, symlinks/junctions escaping the root, sizes, entity counts, and hierarchy depth. External imports either copy resources or create explicitly nonportable references.

Future archive import must limit extracted size, file count, compression ratios, and destination paths. Opening a manifest must not fetch URLs, run host executables, invoke compilers, or load DLLs.
