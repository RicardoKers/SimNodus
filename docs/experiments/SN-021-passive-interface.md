# SN-021 selected passive interface correspondence

Date: 2026-09-15. Status: bounded slice; full SN-021 in progress.
Base main: `0f9cf8d665ad2abfdf146c600592277b2bd9fcb0` (PR #35).
Branch: `codex/sn-021-passive-interface`.

The [contract](../architecture/PASSIVE_INTERFACE.md) and
[ADR 0069](../decisions/0069-passive-interface-correspondence.md) define the scope
and acceptance boundary. The operation revalidates resource-links metadata and
connects one descriptor's maps to an owned recognized R/C source whose bytes
match its lock. Explicit source order determines the returned terminal order;
the R/C primitive determines the unit. Complete metadata/source and exact range
strings remain owned with source offsets. No I/O or readiness grant occurs.

Eight regression cases passed: both owned descriptors, explicit reversed maps
and reordered arrays, source-name case, missing/null selections and map mismatch,
primitive/unit mismatch, byte identity and source grammar/budget, complete metadata
validation and the deliberately unverified numerical-default boundary.
All 42 Windows CTests passed. See the
[audit](evidence/SN-021-passive-interface-summary.json) for results, hashes and
existing privilege/platform skips. No new test skip. An initial configure failed
because the new CMake target block was duplicated; removing the duplicate restored
the build. This failed attempt remains recorded.

```powershell
cmake --build build/sn021-save --config Debug
ctest --test-dir build/sn021-save -C Debug --verbose
python tools/check_repository.py
```

This is inert correspondence testing, not new engine integration evidence.
The existing [real owned RC acceptance](SN-021-passive-source.md) is preserved;
no engine or numerical behavior changed and no wider electrical profile is claimed.
Physical acquisition, default/effective-value range verification, occurrence
binding and execution permission remain separate. All readiness flags stay false.
Next bind exact effective values and ranges, then explicit reference/stimulus/
analysis authority and backend lowering. Safe overwrite remains pending ADR 0064.
Preserve all historical evidence and twelve unrelated local SN-045 files.
Full SN-021 stays **in_progress**; the next-cycle prompt is pending full acceptance.

## Publication record

Source `57c32ce` was pushed in [PR #36](https://github.com/RicardoKers/SimNodus/pull/36),
which records final required hosted checks and protected-main squash integration.
Seven source hashes match the publication tree; 170 historical hashes and twelve
local SN-045 files are preserved. The checker passed for 620 workspace / 618
publication files. Verify final PR/head/checks and main identity before continuation.
