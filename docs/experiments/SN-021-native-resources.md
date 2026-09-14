# SN-021 native local resource verification

Date: 2026-09-13. Result: bounded C++20 snapshot API implemented and locally
validated; SN-021 remains **in_progress**. See the
[contract](../architecture/NATIVE_RESOURCE_VERIFICATION.md),
[ADR 0053](../decisions/0053-native-resource-verification.md), and
[machine-readable evidence](evidence/SN-021-native-resources-summary.json).

## Implemented boundary

The public application API takes a UTF-8 absolute root and typed resource
requests. It validates IDs, lexical paths, duplicate keys/paths, prefix conflicts
and count/byte budgets before opening resources. Complete project/lock metadata
validation, including origin/license fields and the inventory fingerprint,
remains with the existing Python declaration validator. No native JSON or graph
loader is claimed. Tests explicitly preserve this caller boundary.

The platform implementation opens the captured direct volume device, traverses
one component relative to each retained parent handle and inspects the opened
objects. It rejects reparse/offline objects, wrong types, multiple hard links,
alternate basenames and case-sensitive/unqueryable directory policies. Files
deny write/delete sharing while their size, bytes and SHA-256 are checked through
the same handle. No partial inventory is returned. A const snapshot collection
owns all bytes after handles close; paths are source labels, not reopen permits.

Windows system types stay in platform code. BCrypt supplies SHA-256; no new
third-party dependency, custom crypto implementation, engine or renderer is used.
Immutable returned data supplies no trust, interface, redistribution or execution
approval. No resource can choose a DLL, script, compiler, decoder or firmware loader.

## Local verification

- Fresh Windows Debug configuration and completed build with MSVC 19.51.36246.0,
  Visual Studio 18 2026 and Windows SDK 10.0.26100.0. The new library and hooked
  fixture compile with `/W4 /WX /permissive- /EHsc`.
- All 41 native typed-input cases passed, including overflow, count/total limits,
  unsupported root/path syntax, duplicate identities and file/directory prefixes.
  A compile-time assertion checks const access through the snapshot result.
- The native filesystem suite ran 32 cases: 30 passed; file and directory
  symlinks skipped locally because the process lacks the Windows privilege.
  Hosted Windows must run those cases, under the inherited baseline test policy.
- Normal native snapshots/rejections are compared with the unchanged Python
  reference on real files. The test-only build pauses at root/opened/captured
  boundaries for real replacement, write, delete and ancestor-rename attempts.
  It also verifies immutable bytes after capture and cleanup after partial failure.
- Actual 8.3 alias generation on the system temporary volume was observed; the
  native verifier accepted the long name and its lexical gate rejected the
  existing alias. This does not claim a bypass
  of lexical checks to independently test a short name at the handle layer.
- A disposable empty NTFS directory was made case-sensitive and native access
  rejected it with `case_sensitive`. This intentionally differs from Python's
  older gate. No volume-wide setting, installed dependency or user project changed.
- All 94 schema tests and the preserved 32-case Python resource suite passed
  with their existing four local skips. All 18 Windows CTest entries passed.
  Linux builds the typed API and rejects physical verification; hosted counts
  must be measured separately. No Linux resource traversal is implemented.

The setup/build/test commands and hashes are in the JSON evidence. No Renode,
ngspice, CubeIDE, firmware or numerical profile changed, so no engine rerun was
needed. This is actual filesystem evidence, not fake-backend integration evidence.

All 44 selected prior schema/reference inputs matched their recorded SHA-256;
the three historical resource audit files remain unchanged. CMake is deliberately
extended for the native targets. The repository checker passed for 479 text
files, and `git diff --check` passed. Source and local Debug artifact hashes
are recorded; binaries remain ignored and are not published.

## Preserved unsuccessful attempts

The initial build failed in `native_contracts.cpp` with C3535/C2440: a test
initializer mixed string literals and `std::string`. An explicit string vector
fixed the test; the subsequent full build passed. The platform library and
probe had already compiled, but that initial build is not counted as successful.

The first 32-case filesystem run had 29 passes, one failure and two skips.
Case-sensitive fixture setup returned Windows error 145 because the directory
was nonempty. Moving the owned file out before setting the flag, then restoring
it, produced a successful real rejection test. This was fixture preparation,
not a bypass of the verifier. Earlier read-only `fsutil` volume queries were
denied (error 5); real temporary-file alias creation supplied evidence instead.

An additional long-name read on the system temporary C: volume was rejected by
the restricted agent token with `STATUS_ACCESS_DENIED` (`0xc0000022`). The same
complete suite passed outside that token, without changing ACLs or volume
settings. The final CTest/audit uses that ordinary host-access context; this
does not grant symlink-creation privilege, so the two local skips remain.

## Reproduction and remaining work

### Hosted fixture follow-up: 2026-09-14

[Run 34801684641](https://github.com/RicardoKers/SimNodus/actions/runs/34801684641)
at source `80318dd` passed all 13 Linux CTest entries (42 native typed cases),
but Windows passed only 17/18 entries: the native filesystem suite had 31 passes
and one error in the system-temp long-name fixture. Both symlink cases executed
successfully. This failed hosted run is preserved, not counted as acceptance.

The fixture now obtains the owned temporary directory's long spelling with
`GetLongPathNameW` before its positive check. If the original spelling differs,
it separately requires native rejection with `root` (tilde root syntax). Hosted TEMP can contain
a short ancestor name. This is test setup only: production still opens and
checks each component by handle; path expansion is not containment evidence.
Native errors now print their structured code/index for diagnosis. The local
follow-up passed 30/32 filesystem cases with the same two symlink privilege
skips. Final hosted acceptance must be checked on PR #21's final head.

The original evidence JSON and its source hashes describe the pre-follow-up
audit and remain unchanged. Only the regression driver changed afterward;
production implementation and its recorded hashes are unchanged.

Hosted acceptance at source `5f23db4`:
[run 34802067514](https://github.com/RicardoKers/SimNodus/actions/runs/34802067514)
passed both Foundation jobs, all 18 Windows and 13 Linux CTest entries. Windows
executed all 32 native filesystem cases without skips, including both symlinks,
actual 8.3 alias handling and case-sensitive-directory rejection; typed cases
passed 41 on Windows and 42 on Linux. The unchanged Python resource suite retained
two Windows and 28 Linux skips. A local rerun with TEMP/TMP deliberately pointing
to an owned real 8.3 parent also passed 30 native cases/two symlink privilege skips.
Intermediate run 34801953611 was cancelled after the lexical-error expectation
was corrected; it is not acceptance. Documentation-only finalization follows
this source, with required checks still enforced on the final PR head.

```powershell
cmake -S . -B build/sn021-native-01
cmake --build build/sn021-native-01 --config Debug
ctest --test-dir build/sn021-native-01 -C Debug --output-on-failure --verbose
python tests/resources/native_regression.py --probe build/sn021-native-01/Debug/native_resource_probe.exe --hooks build/sn021-native-01/Debug/native_resource_hook_probe.exe
python tools/check_repository.py
git diff --check
```

`native_resource_probe` is a developer fixture with bounded length-prefixed
transport. Its hooked sibling exists only when testing is enabled; hooks are
absent from the normal library. Neither is a project file parser or user-facing
simulation command. The filesystem suite uses disposable owned data and cleans
up its processes/handles even when a test fails.

The API requires requests to remain stable during the call, as ordinary C++
borrowed input; untrusted filesystem substitutions are tested separately.
Only fixed local NTFS and queryable case-insensitive directories are accepted.
The root has a conservative UTF-16 limit. Synchronous OS calls still have no
hard wall deadline; host/kernel privilege attacks and a simultaneous multi-file
disk transaction remain outside the guarantee.

Next specify bounded native declaration ingress and graph construction before
connecting project opening. Full JSON semantics, interfaces, ELF/boot/device
compatibility, atomic saving, source-preserving compilation and runtime policy
remain separate gates. SN-044/UI, shared instrumentation, MCU/toolchain
independence, SN-017 Python/GDB/fixture ownership and all prior evidence remain.
