# Bounded local resource snapshots

Status: first SN-021 implementation contract. This is an explicit developer
verification operation, separate from the unchanged SN-020 metadata parsers.
Native project loading, atomic saving and circuit compilation remain pending.

## Acceptance criteria declared before implementation

- Accept only a bounded lock 0.1 byte document and an explicit absolute local
  Windows drive root. Validate all metadata before filesystem access. Preserve
  dependency/resource IDs and verify every inventory entry, including notices.
- Support local NTFS only. Open each component relative to its parent's handle,
starting at the drive root, including the selected root's ancestors. Inspect
  the opened object, reject every reparse point, nonregular resource, hard link
  and alternate filename alias. Keep directory handles until verification ends.
- Use read-only resource handles denying write/delete sharing. Check actual
  size, read at most the declared size plus one, and SHA-256 the captured bytes.
  Reject unavailable, oversized, changed or mismatched resources. No partial
  result escapes when any entry fails.
- Return immutable bytes keyed by stable dependency/resource IDs. A consumer
  must use these snapshots, never reopen the recorded path on the strength of
  a prior check. Replacing the path after return cannot change the snapshot.
- Exercise real Windows filesystem behavior: junctions, file/directory symlinks,
  root/ancestor substitution, hard links, sharing conflicts, missing files,
  size/hash mismatches, bounds, replacement before/during/after verification and
  cleanup on failure. Record unavailable test privileges as skips, not passes.
- Keep all 94 declaration tests and existing native invariants passing; preserve
  historical hashes. No engine run is required for this filesystem-only slice.

## Separate gates

| Gate | Meaning in this slice |
|---|---|
| Metadata | Existing strict schema, lexical paths and inventory fingerprint |
| Physical containment | Handle-relative traversal with inspected/pinned ancestors |
| Bytes | Exact bounded immutable snapshots matching declared sizes/hashes |
| Interfaces | Unverified: SVG anchors, SPICE syntax/ports/units and ELF/boot/device |
| Origin and redistribution | Unverified; a digest or license declaration grants neither |
| Execution and runtime | Unauthorized by this operation; simulation readiness remains false |

No resource is interpreted, rendered, imported into an engine, executed or loaded
as a library. No acquisition or compiler is invoked. The only native APIs used
are fixed Windows system APIs, not project-selected DLLs. This Python reference
operation establishes the filesystem boundary before a future native loader;
it does not change C++20, adapter, GUI or Python/GDB fixture ownership decisions.

## OS mechanism and limits

Use `NtCreateFile` with a single component and `RootDirectory`, opening reparse
points themselves for rejection. Retain ancestors with no delete/write sharing;
inspect type, identity and name on the same handles. This is not textual prefix
comparison or resolve-then-open. The caller selects the initial root; it is not
authenticated as a trusted project merely by opening it. Replacements before an
object is opened are either rejected or checked as the actual opened object.
The initial drive mapping is captured as a direct volume device and that device
is opened, avoiding a second lookup of the mutable drive letter mapping.

The root accepts a drive-absolute path with at most 64 components and 1024
characters, no UNC/device/relative/dot/stream/short-name syntax. Resource paths
retain the unchanged, stricter lock rules. Only direct fixed-disk volume mappings
and NTFS are accepted; network shares, SUBST, archives, removable drives, Linux
and other filesystems fail closed. Filename comparison rejects alternate names;
case differences alone may identify the same Windows name. Multiple hard links
are rejected even when all links are inside the root.

The existing 256-file, 16 MiB/file and 64 MiB total limits apply. Reads use 64 KiB
chunks; memory is bounded by snapshots plus transient per-file copies. Synchronous
OS calls have no hard wall deadline. This is not isolation from a hostile kernel,
filesystem driver, administrator or preexisting writable mapping. The returned
bytes themselves are hashed; no ongoing filesystem stability or simultaneous
multi-file disk transaction is claimed. All handles close before return.

References: Microsoft's [NtCreateFile contract](https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntcreatefile),
[file sharing](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew),
and [handle identity information](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/ns-fileapi-by_handle_file_information).

Next gate: review/extract this bounded snapshot boundary into native application
code before connecting project loading. Actual source/interface checks need
separate contracts and evidence; no parser or executable profile is selected here.
