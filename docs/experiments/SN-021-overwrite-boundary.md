# SN-021 overwrite ownership boundary acceptance

Date: 2026-09-15. Base main: `53755a0ca232db45c17e4a16d37d96228d11942f`.
Status: candidate overwrite protocols rejected; no overwrite API implemented.
See [ADR 0064](../decisions/0064-overwrite-ownership-boundary.md).
The [audit](evidence/SN-021-overwrite-boundary-summary.json) retains exact source
hashes, the sanitized complete CTest log and historical preservation checks.

## Selected step and criteria

Before extending [atomic creation](SN-021-save-create.md), test whether the
candidate publication operations can preserve a concurrently replaced destination
while retaining the identity/bytes that were checked. A successful overwrite would
have to bind its atomic publication to that expected identity/version, preserve
the old entry on ordinary failure and never expose a partial or absent destination.
This slice accepts reproducible counterexamples and keeps unsafe variants disabled.
It does not accept successful replacement alone as proof of the required protocol.

The [test](../../tests/resources/windows_overwrite_boundary.py) uses real Windows
system APIs and NTFS. Every write/rename targets an exclusively created fixture
directory. The test driver deliberately schedules a second writer between check
and replacement; it does not mock the filesystem. No project, model, DLL chosen
by a document, firmware or engine is loaded. This is physical filesystem evidence,
not simulation integration evidence.

## Observations

Seven physical cases passed, each asserting the rejection or counterexample:

Configuration and the MSVC/Windows SDK build passed. All 37 Windows CTests passed,
including 94 unchanged schema tests and all previous native/project/resource/save
regressions. Local save tests retain thirteen passes/one symlink privilege skip;
native resource tests retain thirty passes/two privilege skips and Python resource
tests retain twenty-eight passes/four explicit skips. No test failure occurred in
this slice; the adverse filesystem outcomes below are required passing assertions.

| Candidate or premise | Observed result |
|---|---|
| Retain a read handle without delete sharing | Legacy rename fails with `0xc0000022`; POSIX rename fails with `0xc0000043`; original remains intact |
| Close the checked handle, then rename with replacement | A deterministic intervening writer's entry is overwritten |
| Retain a handle with delete sharing and use POSIX replacement | Old handle still reads original bytes; name refers to a different file identity |
| Reopen/recheck identity immediately before POSIX replacement | An intervening replacement still loses its entry to the subsequent save |
| Treat matching bytes/hash as identity | Original and replacement have identical hashes but different file identities |
| Rename original to backup, then publish | Destination is absent between operations and another writer can create it |
| ReplaceFile with/without retained no-delete-share lock | Locked call fails with error 32; unlocked call overwrites the intervening owner |

Status numbers describe this measured host; the regression requires failed locked
operations and exact identity/byte outcomes, not universal numeric error codes.
Both rename variants use a retained parent and a simple leaf. That binds the
directory, not the expected destination identity. Textual prefix checking and
resolve-then-open are not used as an ownership argument.

The original API remains create-only. Existing resource tests continue covering
Windows path syntax, missing entries, bounds, aliases, symlinks/junctions/reparse
points, size/hash mismatches and capture-versus-consumption. These counterexamples
do not broaden those policies. Metadata validity, physical containment, byte
identity, interface compatibility, provenance, redistribution and execution
authorization are independent. Matching hashes never grant the latter properties.

## Reproduction and next step

The audit verified 135 historical hashes and the previous CMake hash in the base;
all twelve preexisting SN-045 files remain preserved and excluded. Three current
source hashes are recorded. The repository checker passed for 576 workspace text
files. The README's duplicate create-only summary was consolidated; historical
evidence files were not rewritten.

```powershell
python tests/resources/windows_overwrite_boundary.py
cmake -S . -B build/sn021-save
cmake --build build/sn021-save --config Debug
ctest --test-dir build/sn021-save -C Debug --output-on-failure
python tools/check_repository.py
```

The physical probe is Windows-only and checks NTFS explicitly. It uses Python's
standard library and Windows system APIs; no dependency download is required.
Acceptance of the counterexamples does not establish that all possible Windows
overwrite protocols fail. ACL ownership, oplocks, transactions and recovery are
unproven alternatives, not selected implementations. Advisory coordination alone
does not meet this directory's current concurrency contract.

Next implement bounded physical acquisition of one explicitly selected project
document into an owned validated graph/source capture. Its output must not imply
that the pathname still identifies those bytes later or authorize overwriting it.
Safe overwrite, editing and source-preserving compilation remain pending. SN-021
is not complete; the requested next-cycle prompt remains pending full acceptance.

## Platform references

Microsoft documents the name-based target and POSIX open-handle behavior in
[FILE_RENAME_INFORMATION](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/ns-ntifs-_file_rename_information).
[ReplaceFileW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-replacefilew)
documents its pathname arguments and target access/sharing. The ownership gaps
above are measured counterexamples, not a claim that those APIs promise a
destination-identity comparison. No such comparison parameter is used by them.

## Publication record

The exact publication tree passed the checker for 574 text files; all three
audited source hashes match it. Shared planning review and staged whitespace
checks passed. Source `f19e901` was pushed on `codex/sn-021-overwrite-boundary`.
[PR #31](https://github.com/RicardoKers/SimNodus/pull/31) records final required
Foundation checks and the authorized protected-main squash. Confirm its final
head, CI results and main identity before native project acquisition.
