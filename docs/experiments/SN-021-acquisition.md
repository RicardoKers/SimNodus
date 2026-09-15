# SN-021 native project acquisition acceptance

Date: 2026-09-15. Base main: `f483fded668f589dbc19c4f684070bbbce775347`.
Status: bounded acquisition into owned validated source graph; SN-021 in progress.

[ADR 0065](../decisions/0065-project-acquisition.md) and the
[contract](../architecture/PROJECT_ACQUISITION.md) define the selected scope and
acceptance criteria. The [audit](evidence/SN-021-acquisition-summary.json) records
environment, commands, source/binary hashes and historical preservation.

The application validates root/leaf arguments, captures one document through
retained physical handles, then invokes the existing complete graph loader using
owned bytes. Errors distinguish physical conditions from declaration errors;
success keeps full source/provenance without a pathname lease or save authority.
No referenced project resource is opened or executed.

## Acceptance

Configuration and build passed with MSVC 19.51 and Windows SDK 10.0.26100.0.
All 38 Windows CTests passed. The acquisition suite ran 17 cases: 15 passed,
one Linux-only platform check skipped and one symlink privilege check skipped.
Hosted Windows should exercise symlinks when its privileges permit; Linux runs
three argument/platform checks and explicitly skips the fourteen physical cases.
Previous graph, project, resource, save and overwrite regressions passed, including
94 schema tests. No configuration, compilation or test failure occurred in this
slice. Prior local privilege skips remain recorded in the complete audit log.

The native probe retains a graph copy after releasing its result wrapper, then
returns exact source bytes and graph counts. The physical driver independently
compares those bytes and fixture topology counts. Existing source graph tests
continue checking every source span and connectivity record.

Cases cover invalid roots/leaves before I/O, Unicode roots, case and filename
bounds, exact and over-limit 1 MiB input, missing resources, empty/malformed/invalid
declarations, missing entries, wrong types, hardlinks, root/ancestor/destination
junctions, symlink rejection, readonly documents and conflicting open writers.
Paused real I/O tests check write/delete/rename denial while handles are retained,
replacement before opening and replacement after close before validation. The
last returns the original exact bytes while the pathname holds a different file.
This is real Windows filesystem evidence, not engine integration evidence.

## Reproduction and limits

All 138 selected historical hashes matched; the previous CMake hash was verified
in the base. Nine source hashes are recorded. Twelve preexisting SN-045 changes
remain local and excluded, including shared planning edits. The repository checker
passed for 586 workspace text files. Historical evidence is unchanged.

```powershell
cmake -S . -B build/sn021-save
cmake --build build/sn021-save --config Debug
ctest --test-dir build/sn021-save -C Debug --verbose
python tools/check_repository.py
```

The existing Windows/NTFS policy and declaration/entity limits are unchanged.
Linux checks path arguments and returns `platform` for valid requests. No hard OS
quotas, cancellation, writable-mapping defense, privileged mutation defense or
device-failure injection is claimed. No expected hash is supplied for this selected
document; capture is not a trust, interface, redistribution or execution gate.
Historical failures and privilege skips remain explicit. No new dependency, UI,
engine profile, SN-017 scheduling, instrumentation, PDF/PID or MCU/toolchain change.

Next define minimal source-preserving revision/edit semantics on owned documents,
with revalidation and stable source identities. Safe overwrite remains pending
ADR 0064's ownership gate; compilation requires its own real-engine evidence.
Prepare the requested next-cycle prompt only after full SN-021 acceptance.
