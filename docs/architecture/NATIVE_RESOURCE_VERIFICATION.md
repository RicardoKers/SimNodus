# Native local resource verification

Status: SN-021 bounded native extraction contract, with
[implementation results](../experiments/SN-021-native-resources.md).
The [Python snapshot reference](LOCAL_RESOURCE_VERIFICATION.md) remains historical
evidence and a differential oracle. This slice does not load project graphs.

## Boundary and acceptance criteria

- A C++20 application API receives an explicit UTF-8 absolute root and typed
  dependency/resource/path/size/SHA-256 requests. It independently rejects invalid
  IDs, lexical paths, duplicate identities/paths, prefix collisions and budgets
  before filesystem access. Complete JSON/schema/origin/license/inventory-digest
  validation remains the existing caller-side declaration gate; this API does not
  replace it or claim a native JSON parser.
- Return either an immutable complete collection of owned snapshots or one
  structured error, never a partial inventory or a checked path for reopening.
  Keep Windows types and APIs in platform implementation, out of the public API.
- Use parent-relative handles, inspect/pin every ancestor and reject reparse,
  offline, nonregular, multiply linked and alternate-name objects. Use the
  captured direct volume target; never a textual containment prefix. Hash the
  same bounded captured bytes that the caller receives, using Windows SHA-256.
- Keep the existing 256-file, 32-dependency, 16 MiB/file and 64 MiB total bounds.
  Native root input is at most 4096 UTF-8 bytes and 1024 UTF-16 code units, with
  at most 64 components. Supplementary Unicode may reach the native limit sooner
  than the Python character limit; no new root syntax is accepted.
- Require queryable, case-insensitive directories on local fixed NTFS volumes.
  Reject case-sensitive directories or unavailable case-policy information.
  Test policy rejection independently of privileges for creating such directories;
  actual enablement tests must report an unavailable host capability as a skip.
  Short-name inputs remain lexically rejected; inspect the actual long basename
  on the handle as a second alias defense. Obtain real 8.3 evidence where the
  host generates aliases without changing volume-wide settings.
- Exercise success, bounds, invalid typed requests, same-size corruption,
  missing/type errors, sharing conflicts, links, replacement before/during/after
  reads and failure cleanup on the real Windows filesystem. Compare snapshots
  and rejections against the preserved Python implementation where profiles agree.
  Record deliberately stricter native case-policy rejection separately.
- Keep all existing schema, Python resource and native invariants passing.
  Linux builds the typed contract and rejects physical access as unsupported.
  No engine campaign is required for filesystem extraction alone.

## Test transport and scope

A developer-only probe carries bounded length-prefixed typed requests over stdin
and returns snapshots or diagnostics over stdout. It is a test transport, not a
project format, native loader or application IPC decision. Only an explicitly
selected test executable may pause at verification boundaries for real host
mutation tests; those hooks are absent from the normal library build.

Only fixed Windows system APIs are used. Nothing in project input selects a DLL,
compiler, decoder, script, renderer or firmware loader. Byte verification grants
no origin/redistribution approval, source-interface compatibility or simulation
readiness. Existing engine restrictions, SN-044, shared instrumentation and
SN-017 Python/GDB/fixture ownership remain unchanged.

Synchronous OS calls have no hard wall deadline. This is not a sandbox against
privileged host/kernel interference, nor a simultaneous multi-file transaction.
Native project loading, actual SVG/SPICE/ELF interfaces, atomic saving and
source-preserving compilation remain later gates within SN-021.

Windows references: [NtCreateFile](https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntcreatefile),
[handle information](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getfileinformationbyhandleex),
[BCryptHash](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcrypthash).
