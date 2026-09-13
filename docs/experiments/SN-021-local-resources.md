# SN-021 bounded local resource verification

Date: 2026-09-13. SN-021 is **in_progress**. This first coherent slice implements
the explicit [local resource snapshot contract](../architecture/LOCAL_RESOURCE_VERIFICATION.md)
under [ADR 0052](../decisions/0052-bounded-local-resource-snapshots.md).
It is a Python reference using real Windows/NTFS handles, before native loading,
atomic saving and circuit compilation. No backend or executable profile changes.

## Implementation and acceptance boundary

`tests/resources/local_resources.py` accepts immutable lock bytes and an explicit
absolute local root. Existing SN-020 parsers/fixtures are unchanged. After metadata
validation, it traverses one component per parent handle from the volume root,
rejects reparse/offline objects, alternate names, multiple hard links and wrong
types, and retains directory handles denying write/delete sharing. Resources are
read from their inspected handle into bounded snapshots; exact size and SHA-256
must match for every entry before the immutable inventory is returned.

The result preserves dependency/resource IDs. Consumers must use its immutable
bytes. The summary never authorizes execution, origin, redistribution, interfaces,
firmware compatibility or runtime readiness. Even executable-looking or script
bytes are treated only as bytes. No rendering, library loading from resources,
firmware loading, compiler invocation or dependency download occurs.

## Validation record

The initial 28-case local trial completed with 25 passes and three skips: the
non-Windows rejection case and two symlink cases lacking Windows privilege.
After additional limit/substitution cases, the 31-case trial completed with
27 passes and four skips, adding unavailable 8.3 name generation on the local
volume. An unrestricted-token rerun produced the same four skips. These are
coverage limitations, not successful symlink or short-alias tests.

Final commands and per-case outcomes are recorded in the
[final machine-readable evidence](evidence/SN-021-local-resources-final-summary.json).
The [initial audit](evidence/SN-021-local-resources-summary.json) is preserved;
review then tightened volume opening to use the captured direct device rather
than repeating the drive-letter lookup. The final audit records that source.
The first attempt to write the initial audit was denied by sandbox filesystem
permissions, leaving its report links temporarily missing; the authorized retry
wrote the evidence. This was evidence-file creation, not a filesystem-test failure.
The separate 94 declaration tests remain the SN-020 regression authority.
CTest now includes `local-resource-snapshots` in addition to its prior 15 Windows
entries. The existing CI workflow discovers it without a new dependency.

The final local run passed all 94 schema tests, 27 resource cases (four skips)
and all 16 CTest entries after a fresh native Debug build. Forty historical
SN-020/SN-044 source/fixture/document hashes matched. The repository checker
passed for 467 text files, and `git diff --check` passed. No engine was rerun.

Actual local filesystem checks include:

- Existing owned lock and embedded project inventory, exact binary bytes and
  immutable results; zero size, chunk boundaries and maximum file/total/count.
- Missing root/files/parents; directory/file role errors; actual oversize, size
  mismatch and same-size hash mismatch; invalid metadata before OS access.
- Hard links inside/outside; junctions inside/outside, similar-prefix escape,
  selected-root and ancestor junctions; junction substitution just before open.
- Existing writer/exclusive reader conflicts; writes, deletes and ancestor/root
  renames denied during reading; malicious replacement before open rejected by
  byte verification; replacement after return leaves the verified snapshot intact.
- All-or-nothing failure and handle cleanup; inert executable/script-like bytes.

Hosted Windows must execute both symlink cases; unavailable privilege fails that
job. The short-alias case explicitly skips when NTFS does not create aliases;
portable lexical rejection of tilde aliases remains separately tested. Hosted
Ubuntu tests only input/platform rejection and existing foundation invariants.
It does not validate Linux resource traversal. Hosted outcomes and PR integration
must be checked separately from local results.

Both hosted Foundation jobs passed for source commit `cb6fc8d` in
[run 34785300902](https://github.com/RicardoKers/SimNodus/actions/runs/34785300902).
The Windows test policy requires both symlink cases to run successfully there;
their local privilege skips remain preserved. The follow-up enables verbose
CTest logs for explicit hosted case/skip counts without changing verifier code.
[PR #20](https://github.com/RicardoKers/SimNodus/pull/20) records final checks and
authorized protected-main squash integration.

```powershell
python -m unittest discover -s tests/schema -p 'test_*.py' -v
python -m unittest discover -s tests/resources -p 'test_*.py' -v
python tests/resources/local_resources.py tests/schema/fixtures/owned-resource-lock.json --root D:/03_Projects/01_Actives/SimNodus
cmake -S . -B build/sn021-foundation-01
cmake --build build/sn021-foundation-01 --config Debug
ctest --test-dir build/sn021-foundation-01 -C Debug --output-on-failure
python tools/check_repository.py
git diff --check
```

The starting main/origin tip was verified as
`7307bd88e2d468d65363fa6a6f0776772cbb124a`, with clean worktree and no unpublished
commits. An initial elevated GitHub read was temporarily rejected by automatic
approval-service usage limits; a later authorized read succeeded. No failure or
skip is relabelled as an acceptance pass. No issues/releases were synchronized.

## Limitations and next step

Direct fixed local NTFS volumes only; no UNC, SUBST, archives, symlinks/junctions,
hard-linked resources, other filesystems or Linux loading. Byte/count limits do
not impose a hard synchronous-I/O wall deadline. Snapshot hashes do not prove
trusted provenance or a simultaneous multi-file disk transaction. Privileged
host/kernel/driver interference is outside the boundary. No new third-party
resource is incorporated; all test resources are owned disposable bytes.

Next review/extract the tested snapshot boundary into native application code
before connecting native project loading. Source/SPICE/SVG and ELF/boot checks,
atomic saving, source-preserving compilation and runtime negotiation need later
bounded work. SN-021 is not complete. SN-044/UI, shared instrumentation, MCU and
toolchain independence, SN-017 Python preparation/GDB/fixture ownership, engine
tolerances, PDF suppression, PID retry and all historical evidence are preserved.
