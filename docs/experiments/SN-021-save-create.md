# SN-021 atomic project creation acceptance

Date: 2026-09-15. Base main: `6d88455be400520d918c26406b210ad062567976`.
Status: create-only persistence accepted locally; SN-021 remains in progress.

[ADR 0063](../decisions/0063-atomic-project-create.md) and the
[contract](../architecture/ATOMIC_PROJECT_CREATE.md) define explicit persistence
of exact owned, validated project bytes. The [audit](evidence/SN-021-save-create-summary.json)
records commands, environment, source/binary hashes, failures and preservation.

## Acceptance

All 36 Windows CTests passed with MSVC 19.51 and SDK 10.0.26100.0. The save API
passed 25 boundary checks. Fourteen physical filesystem cases produced thirteen
passes and one explicit symlink-privilege skip. Cases cover exact bytes, Unicode
roots, the 1 MiB boundary, filename bounds, existing files and case aliases,
directories, junctions, hardlinks, root ancestry, invalid input without writes,
failure cleanup, locked handles, concurrent destination creation, two competing
writers and a killed writer. Existing schema, graph and resource regressions pass.
Prior native resource tests retain thirty passes/two privilege skips; Python
resource tests retain twenty-eight passes/four explicit skips.

Publication uses an exclusive temporary, bounded writes, a flush and a single
handle-relative native rename with replacement disabled. Existing or concurrently
created destinations remain intact. No textual prefix or resolve-then-open check
is used as containment proof. Tests use the real Windows filesystem; they make
no engine integration claim. No project resource is imported or executed.

The killed writer left one 65536-byte orphan temporary and no partial destination;
the existing original remained intact. Ordinary injected failures clean up their
owned temporary. Process termination recovery, orphan scavenging and power-loss
durability are not implemented or claimed.

The audit confirms 121 historical hashes; three changed source hashes are verified
in the base. Fourteen final source hashes are recorded. Shared path/handle helper
bodies are identical to the base apart from namespace and inline linkage.
All twelve preexisting SN-045 changes remain local and excluded from publication.

## Negative evidence and correction

The initial Win32 rename wrapper failed with system error 87: the first full run
passed 35 of 36 CTests, with one failure and three errors in the physical save
suite. Its source, tests and log are preserved with verified hashes in the audit.
The first native API correction failed to compile because the installed user-mode
SDK omits the setter declaration and information-class name. No test result is
claimed for that build. The documented native signature and class value corrected
the build; the focused physical suite and subsequent complete regression passed.
An earlier automatic approval review rejected an attempt at the usage limit;
that attempt made no mutation. A later authorized retry proceeded normally.

## Reproduction and remaining scope

```powershell
cmake -S . -B build/sn021-save
cmake --build build/sn021-save --config Debug
ctest --test-dir build/sn021-save -C Debug --output-on-failure
python tools/check_repository.py
```

Linux performs boundary validation and rejects valid persistence requests as an
unsupported platform. Hosted Windows/Linux checks must be verified on the final
PR head before integration. Work branch: `codex/sn-021-atomic-create`.

Next define and prove safe save-over-existing with explicit destination ownership
and concurrent-change protection. Project path acquisition, editing and compilation
with source mappings remain pending. Full SN-021 is not complete. Valid metadata,
contained bytes and matching hashes do not prove interface compatibility, trust,
redistribution permission or execution authorization. Preserve supported profiles
and use real engines when subsequent compilation requires integration evidence.

## Publication record

The repository checker passed for 572 workspace and 570 publication-tree text
files. All fourteen audited source hashes match both trees; staged whitespace
and shared planning review passed. Source `ed50b39` was pushed on
`codex/sn-021-atomic-create`. [PR #30](https://github.com/RicardoKers/SimNodus/pull/30)
records final required checks and the authorized protected-main squash. Confirm
its final head, CI results and main identity before the next persistence slice.
