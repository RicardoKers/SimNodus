# Bounded native project acquisition

Status: SN-021 slice under [ADR 0065](../decisions/0065-project-acquisition.md).

`acquire_project(root_utf8, filename)` explicitly acquires one selected document,
then invokes the existing complete project validator and source graph loader.
It returns an immutable owned `ProjectGraph`, or a structured error separating
physical acquisition from declaration validation. Declaration offsets refer to
the exact captured bytes. A successful result grants no resource/runtime gate.

## Contract and acceptance

- Select the root and leaf outside project metadata. Borrowed arguments must stay
  stable during the call. Reuse the accepted root syntax and one portable ASCII
  filename of at most 80 characters; reject nested paths, traversal, ADS, aliases,
  device/UNC paths and reserved names. Unicode is supported in the selected root.
- Use the existing Windows fixed-NTFS adapter helpers unchanged. Traverse each
  ancestor relative to opened handles; inspect reparse/offline, case-sensitive,
  alias and type conditions. Retain all ancestors and the selected file until
  byte capture ends. Do not resolve a pathname and reopen it or infer containment
  from a textual prefix. Require a regular single-link file on the root volume.
- Read the file through that same handle with no write/delete sharing. Reject
  preexisting conflicting writers. Check size before allocation against the
  existing 1 MiB declaration limit. Read in at most 64 KiB chunks, with one byte
  of bounded overflow detection; require expected byte count and unchanged handle
  identity/size after reading. Empty input reaches declaration rejection.
- Close physical handles after capture. Validate the owned bytes through the
  complete project 0.1 validator and graph loader. Never reopen the path to parse
  it. Keep every source byte and existing graph/source mapping semantics. A copy
  of the returned shared graph remains valid after the result wrapper is released.
- Test exact whitespace/Unicode root/case and name bounds, exact/over-limit bytes,
  invalid declarations, missing entries, wrong types, hardlinks, root/ancestor/
  destination junctions, symlinks, readonly input and conflicting writers. Pause
  at root, opened, captured and released boundaries for deterministic real I/O
  mutation tests. A post-close replacement must not change the returned capture.
- Linux performs path argument validation and rejects valid acquisition requests
  with `platform`; a Linux filesystem backend is not introduced.

## Limits and separate gates

The first path lookup may encounter an entry different from an earlier caller
observation; its actual bytes are captured and validated. No expected file hash
is supplied for this selected document. A retained capture is not proof that its
pathname still refers to the same identity later. No lease, version token or
overwrite authority is returned. Preserve the [overwrite boundary](../decisions/0064-overwrite-ownership-boundary.md).

The inherited handle policy does not claim protection against privileged raw-disk
mutation, kernel/driver faults or preexisting writable mappings. Hard OS memory/
time quotas and device failure injection are not implemented. Memory remains
bounded by the existing parser/entity limits plus the bounded raw capture; there
is an additional owned source copy during validation. The API does not supply a
cancellation or long-lived open-document session protocol.

Referenced resources may be absent. Opening the document never opens its declared
DLLs, models, firmware or other assets, renders content, downloads dependencies or
runs a simulator/compiler. Metadata validity, physical containment and captured
bytes do not establish interface compatibility, trusted origin, redistribution
permission or execution authorization. No hash match grants these properties.
Source-preserving editing, safe overwrite and compilation remain separate work;
real-engine claims require their own integration evidence. No domain, UI,
instrumentation, MCU/toolchain or SN-017 ownership boundary changes.
