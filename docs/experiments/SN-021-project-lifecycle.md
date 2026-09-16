# SN-021: native project lifecycle composition

Date: 2026-09-16. Status: bounded Windows/NTFS integration accepted locally;
publication and hosted checks are recorded by the associated pull request.
Full SN-021 remains in progress.

## Acceptance defined before the run

Compose the existing APIs without changing their production contracts: create a
new project document, acquire its owned bytes, rename its display name, save a new
copy, reacquire that copy and explicitly compile the accepted standalone ideal RC
profile. Preserve original bytes, graph identities, resource declarations and
source spans. Require the reopened artifact to match pre-save compilation and
pass the unchanged real E-01 engine acceptance. No automatic execution is added.

Reject collisions at either destination without changing existing data. An invalid
document must never be published; an invalid edit must preserve the original.
Replacing the original after acquisition must not substitute the captured bytes.
Missing or changed resources must permit inert document operations but stop
compilation. These are actual filesystem tests under a controlled repository build
parent, not synthetic substitutes for NTFS behavior. Existing symlink, junction,
reparse-point, alias, byte-limit and race tests remain in the full suite.

## Implementation and results

The test-only [native probe](../../tests/schema/native_lifecycle_probe.cpp) links
the unchanged acquisition, revision, create-only save and compiler libraries. Its
capture barrier lets the [Python tests](../../tests/schema/native_lifecycle_regression.py)
replace only fixture-owned files between operations. The barrier grants no
production lease or overwrite authority. Compilation retains the existing
handle-based containment and byte-capture gates; no textual-prefix or
resolve-then-open containment test is introduced.

All eight lifecycle cases passed with no skips on Windows. All 45 Windows CTests
passed. The lifecycle target is Windows-only; no Linux physical acceptance is
claimed. Each operation is independent: a later compilation failure leaves the
successfully saved inert document available. This is not a multi-file transaction.

The explicit [engine runner](../../tests/experiments/ngspice/project_lifecycle_acceptance.py)
derives the same single RC branch used by ADR 0071 from the unchanged owned fixture.
It verifies the nine pinned ngspice files, performs the native persistence flow,
compares the reopened artifact with pre-save compilation and invokes the unchanged
E-01 host in a 30-second isolated process. The source resource and engine paths
are controlled experiment inputs, not execution authority from project metadata.

Two local engine attempts passed. After the first, runner executable paths were
initialized before asset verification so an early failure can still be recorded;
the final runner was then rerun. Each attempt produced 5012 samples, final time
0.004999999999999989 s and maximum analytical error 9.889724283951296e-08 V.
The unchanged 0.0165 V E-01 bound, 10 uV project bound and 1 ps endpoint tolerance
passed; callbacks matched vectors and shutdown was idle with exit status zero.
No negative engine result occurred in this slice. Prior negative/inconclusive
evidence is unchanged. The [audit](evidence/SN-021-project-lifecycle-summary.json)
records source, fixture, historical evidence, raw-log and executable hashes.

Reproduce after the existing local build/dependencies are available:

```sh
cmake -S . -B build/sn021-save
cmake --build build/sn021-save --config Debug
ctest --test-dir build/sn021-save -C Debug --output-on-failure
python tests/experiments/ngspice/project_lifecycle_acceptance.py --output build/sn021-lifecycle-new-run
python tools/check_repository.py
git diff --check
```

The engine runner requires a fresh output directory and downloads nothing.
Generated packages, binaries and raw engine output remain ignored local artifacts.

## Remaining acceptance gates

| Concern | Evidence now available | Remaining boundary |
|---|---|---|
| Declarative metadata | Complete native validation and owned graph | Does not imply execution readiness |
| Physical containment and bytes | Existing bounded NTFS capture, also composed here | No portable filesystem or pathname lease claim |
| Persistence | Create/open/name-edit/save-copy/reopen and collision preservation | Safe overwrite still requires ADR 0064 identity/version ownership proof; no crash-durability claim |
| Model interface and compilation | Recognized R/C source, exact numeric binding, source maps and explicit ideal RC artifact | No broader circuit, symbol rendering or configured co-simulation support |
| Firmware/platform | Metadata remains inert | Bounded owned-byte ELF inspection, boot/device/interface checks and real target integration still pending |
| Runtime authority | Explicit owned-fixture ngspice acceptance | No project-driven engine permission, capability negotiation or generic runner |

Next select a bounded inert firmware-byte inspection contract from captured
resources before any ELF loading, boot or target/runtime claim. Define byte bounds,
segment/address arithmetic and unsupported-format rejection first; passing an ELF
header alone cannot establish the MCU/board, boot mapping or runtime compatibility.
Keep safe overwrite explicitly pending; do not repeat the rejected pathname races.

ADRs 0027/0028/0043/0049/0051/0064/0071 remain authoritative. This slice adds no
production API or architectural decision and requires no new ADR. Preserve
SN-017 Python preparation/GDB/fixture ownership, SN-044, instrumentation, numerical
tolerances, PDF suppression and PID retry. No UI, dependency download, issue sync,
release, firmware loading or profile expansion. Prepare the next-cycle chat prompt
only after full SN-021 acceptance, not after this lifecycle slice.
