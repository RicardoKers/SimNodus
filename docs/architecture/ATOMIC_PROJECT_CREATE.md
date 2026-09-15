# Atomic creation of a project document

Status: bounded SN-021 contract under [ADR 0063](../decisions/0063-atomic-project-create.md).
This first persistence operation creates a new project file. It never overwrites
an existing entry and does not implement save-over-existing or editing transactions.

## Boundary and acceptance

- `save_new_project(root_utf8, filename, bytes)` explicitly validates and captures
  the entire project 0.1 before filesystem access. Borrowed arguments must remain
  stable during the call. Persist the exact owned bytes, including whitespace,
  unknown-field rejection, stable IDs, nulls and every retained metadata field.
- The destination is a separately selected existing Windows fixed NTFS directory,
  using the existing root syntax and handle-relative traversal policy. The name is
  one portable ASCII leaf of at most 80 characters. Reject path separators, drive/
  UNC/device paths, traversal, ADS, reserved names, trailing dots/spaces and aliases.
  No extension is selected; resources are not copied or imported by saving JSON.
- Reuse the existing internal path and handle helpers without changing resource
  verification rules. Inspect root/ancestors through opened handles, reject reparse
  points and case-sensitive directories, and retain the selected directory handle.
  Neither textual prefix comparison nor resolve-then-open proves containment.
- Create a random `sn-save-<32 hex digits>.tmp` entry relative to that handle using
  NT FILE_CREATE, no sharing, no reparse traversal and no recall. At most eight
  candidate collisions are attempted. A candidate collision never opens an existing
  entry. Write bounded chunks (64 KiB), require progress, flush the temporary and
  verify its regular-file/single-link metadata and exact size before publication.
- Publish with one native FileRenameInformation operation, the retained parent
  handle, one simple destination leaf and replacement disabled. Allocate all
  fallible state before that operation. Success reports bytes published, not any
  resource/interface/firmware/runtime approval. No path is reopened to publish.
- An existing or concurrently created destination must prevent publication,
  including files, case aliases, directories, symlinks/junctions and hard links.
  Never truncate, remove or follow that entry. Competing prepared writers may
  publish only one result. The new destination remains absent until publication.
- On ordinary failure, mark only the owned temporary handle for deletion; never
  recursively clean a directory or delete by an unverified pathname. Report a
  cleanup failure explicitly. Do not retry a failed publish as an overwrite.
- Test exact bytes and the 1 MiB input boundary, one/80-character names, Unicode
  roots, invalid input before writes, existing/concurrent entries, pinned temporary
  access, ancestor rename denial, six injected failure phases and killed writers.
  Regress physical resource verification after helper extraction. Linux rejects
  valid persistence requests with `platform`; no Linux save backend is selected.

## Failure and durability limits

Atomic namespace publication is distinct from power-loss durability. Flushing the
temporary precedes publication; this slice does not prove volume/directory metadata
durability, power-loss recovery or hard OS memory/time quotas. Disk-full, hardware
I/O failures, random-name exhaustion and cleanup failure are not fault-injected;
the six phase failures exercise ordinary cleanup and original-file preservation.

Killing the process before rename can leave an orphan temporary because destructors
do not run. The test records one partial 65536-byte temporary with the original
destination intact. No automatic scavenger is provided. If a process stops after
rename but before reporting success, the caller may have an uncertain result;
retrying remains create-only and cannot overwrite that entry. Another authorized
process can change files after this operation releases its handles.

The receipt does not certify portable completeness of the saved project: referenced
resources may be absent. It does not modify the in-memory graph or serialize edits,
approve source interfaces, license/origin/redistribution, load DLLs/models/firmware,
render content, download, compile or execute. No engine acceptance follows from
filesystem tests. Existing tolerances/profiles, SN-044, local SN-045, instrumentation,
MCU/toolchain independence and SN-017 Python/GDB/fixture ownership remain unchanged.

Next establish safe save-over-existing semantics with explicit destination ownership
and concurrent-change protection; never substitute a textual check followed by
replacement. Path acquisition, editing and compilation remain separate pending work.

## Platform references

The implementation uses Microsoft's documented
[native rename structure and no-replace behavior](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/ns-ntifs-_file_rename_information),
[NtSetInformationFile signature](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-ntsetinformationfile),
[FileRenameInformation class 10](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ne-wdm-_file_information_class)
and [FlushFileBuffers](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers).
The installed user-mode SDK omits the native setter declaration and enum member;
the adapter declares that exact signature and links the Windows ntdll system API.
This does not load a project-selected library. The failed initial Win32 wrapper
attempt and subsequent native correction remain in the acceptance evidence.
