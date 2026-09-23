# GitHub setup and publication record

The owner authorized the first public source publication at `RicardoKers/SimNodus`, with Ricardo Kerschbaumer as author. The first commit, `b3163a1`, was published on 2026-08-31. No raw private conversations or binary simulator release are included. See [current state](../planning/CURRENT.md) for ongoing work.

## Source publication gate

- [x] English README, architecture, roadmap, backlog, and contribution guidance.
- [x] MIT license for original material; third-party review policy.
- [x] Git ignore/attributes/editor conventions and issue/PR templates.
- [x] Structural CI workflow with read-only permissions and pinned checkout action.
- [x] Local validation results recorded in the quality document.
- [x] GitHub account/organization and repository name confirmed.
- [x] Copyright attribution updated to Ricardo Kerschbaumer.
- [x] Public metadata and files reviewed for personal paths, credentials, and student information.
- [x] Preliminary GitHub name search completed; no matching repository found before publication.
- [x] Commit identity confirmed against the authenticated account and existing Git configuration.
- [x] Initial commit reviewed and recorded.
- [x] Remote created as public and initial push authorized/performed.
- [x] Hosted CI passed on Windows and Ubuntu; private vulnerability reporting configured.
- [x] Nine initial issues and four milestones created; `main` protection enabled.

Do not publish raw chat transcripts. The research synthesis is sufficient and does not expose private conversation identifiers.

## Remote setup

Repository: [RicardoKers/SimNodus](https://github.com/RicardoKers/SimNodus). Description: “Open-source mixed-signal and embedded systems simulator, starting with STM32.”

Suggested topics: `simulation`, `electronics`, `stm32`, `ngspice`, `renode`, `education`, `cpp`, `qt`. Default branch: `main`.

Use a reviewed initial commit and an empty remote, then add the remote and push explicitly. Do not generate an unrelated remote README/license that creates an avoidable history conflict. The owner has authorized these steps. Formal trademark/domain clearance remains open; the preliminary public-name check is not legal clearance.

## Collaboration setup

Initial issues cover SN-010 through SN-018. See the [backlog mapping](../planning/BACKLOG.md) and [GitHub issues](https://github.com/RicardoKers/SimNodus/issues). Labels identify work type, priority, platform, and ready/waiting status. No contributors were invited and no separate project board was created.

Milestones: [M1](https://github.com/RicardoKers/SimNodus/milestone/1) (September 30), [M2](https://github.com/RicardoKers/SimNodus/milestone/2) (October 31), [M3](https://github.com/RicardoKers/SimNodus/milestone/3) (December 15), and [classroom readiness](https://github.com/RicardoKers/SimNodus/milestone/4) (January 31, 2027). These are planning targets, not delivery guarantees.

`main` requires pull requests, an up-to-date branch, resolved conversations, and successful `Foundation (windows-latest)` and `Foundation (ubuntu-latest)` checks from GitHub Actions. Separate reviewer approval count is zero for the single-maintainer phase. Rules include administrators; force pushes and branch deletion are disabled. Squash merging is the enabled merge method and merged source branches are automatically deleted.

The wiki is disabled so documentation stays versioned with code. Private reports use the [security advisory form](https://github.com/RicardoKers/SimNodus/security/advisories/new). Dependency-update PRs should be reviewed, not automatically merged.

## Authorized source checkpoint: 2026-09-13

The owner authorized a local commit and GitHub push of accumulated source and
versionable evidence on `codex/checkpoint-2026-09-13`. This is a recovery point,
not a main-branch merge, issue synchronization or release. Verify the remote tip
matches the local commit before considering the transfer complete. Ignored
`build/` engines, binaries and raw runtime logs are not included in this source
checkpoint; preserve them separately for a complete local-data backup.

Historical evidence JSON is stored without Git line-ending conversion so recorded
hashes retain their original byte meaning. C#, Java, XML, manifest, patch and
PowerShell sources explicitly use LF on checkout. This prevents new checkout
normalization from invalidating fixture/source hashes; it does not certify a
clean-machine engine setup. Existing evidence contents are not rewritten.
Git whitespace checks recognize CRLF in these historical JSON records; preserved
unified-diff patch artifacts retain their required blank context lines.
The already measured `tests/experiments/adc/stm32f103_adc.cs` also retains its
original CRLF bytes explicitly, because real-engine evidence hashes that source.
This exception preserves the input rather than rewriting historical hashes.

The checkpoint review found no common credential-token/private-key patterns in
the versionable files and no binaries selected by the ignore rules. This is a
bounded source-publication check, not a security audit or binary licensing gate.

## Ongoing validated-source policy: 2026-09-13

The owner subsequently authorized coherent validated commits, integration into
protected `main`, and push so GitHub represents the official development state.
This supersedes the checkpoint-only limit. Preserve unrelated valid local work;
never reset, clean, force-push or rewrite published history for housekeeping.
Use the existing pull-request/squash flow with required checks. Do not merge
unresolved conflicts, failing checks or unfinished experiments. Acceptance of a
schema specification does not claim its runtime application is implemented.

The SN-020 integration unit includes the accepted bounded prior headless/baseline
evidence, completed declaration specification/reference tests, and the preserved
SN-044 documentation. No new engine profile, issue synchronization or binary
release is included. Older experimental reports stay labelled and preserved.

Source commit `1500898a7ad08b37d7e6fba15c53183fdb584875` was published on
`codex/sn-020-schema`. [PR #19](https://github.com/RicardoKers/SimNodus/pull/19)
is the integration and hosted-check record. Follow that record for the resulting
squash commit and final status; source-branch publication alone does not update main.

## SN-021 source integration: 2026-09-13

The first bounded local-resource reference slice is published on
`codex/sn-021-local-resources`, starting with source commit
`cb6fc8d977b725d2d2c60d7aa59c2fb8efa23d2c`. Both Foundation jobs passed in
[run 34785300902](https://github.com/RicardoKers/SimNodus/actions/runs/34785300902).
[PR #20](https://github.com/RicardoKers/SimNodus/pull/20) is the final hosted-check
and squash integration record. Confirm its final state and main identity before
claiming integration. Foundation exposes verbose CTest summaries to distinguish
filesystem passes from OS/privilege skips. This slice does not close SN-021 or
publish a simulator, binary, new supported engine profile or release.

## SN-021 native source integration: 2026-09-14

Native snapshot source commit `bf3e05b` was published on
`codex/sn-021-native-resources` after local acceptance on 2026-09-13.
[PR #21](https://github.com/RicardoKers/SimNodus/pull/21) is the final required
Foundation-check and protected-main squash record. Confirm the final PR head,
successful checks and resulting main identity before reporting integration.
The native API verifies bounded snapshots; it does not implement native project
JSON/graph loading or close SN-021. No issues, releases or binaries are published.
Source `5f23db4` passed both Foundation jobs in
[run 34802067514](https://github.com/RicardoKers/SimNodus/actions/runs/34802067514),
including all 32 Windows native filesystem cases without skips. The native report
preserves the preceding fixture failure and correction; final documentation-only
changes still require checks before integration.

## SN-021 native topology integration: 2026-09-14

Source `4cccfe7` was pushed on `codex/sn-021-topology-semantics` after local
semantic/foundation validation and review of the exact staged tree.
[PR #23](https://github.com/RicardoKers/SimNodus/pull/23) records final required
checks and authorized squash integration. Preexisting local SN-045 documentation
is excluded, including its edits to shared planning files. Preserve that overlay
while synchronizing main; do not reset or discard it for a clean-status report.
This is topology 0.1 semantic validation only, not full project/graph loading.

## SN-021 native parameter integration: 2026-09-14

Source `a561149` was pushed on `codex/sn-021-native-parameters` after exact
parameter, topology and foundation acceptance. [PR #24](https://github.com/RicardoKers/SimNodus/pull/24)
records final required checks and the authorized squash. Preexisting SN-045
documentation remains local and excluded. Preserve it while synchronizing main.
The new snapshots are inert parameter inspection, not full project/graph loading,
resource interpretation or execution approval. No binary release is included.

## SN-021 native descriptor integration: 2026-09-14

Source `1a6de52` was pushed on `codex/sn-021-native-bindings` after native topology
0.3 acceptance and exact publication-tree review. [PR #25](https://github.com/RicardoKers/SimNodus/pull/25)
records final required Foundation checks and the authorized squash. Preexisting
SN-045 documentation remains local and excluded, including shared planning edits.
The immutable result validates declared maps and intervals, not physical resource
interfaces, source trust, graph loading or execution readiness. No binary release.

## SN-021 native resource lock integration: 2026-09-14

Source `ead5b71` was pushed on `codex/sn-021-native-lock` after native metadata,
digest and foundation acceptance. [PR #26](https://github.com/RicardoKers/SimNodus/pull/26)
records final required Foundation checks and the authorized protected-main squash.
Preexisting SN-045 documentation remains local and excluded, including planning
edits. The new immutable requests are declarations only; physical verification,
source interfaces, trust and execution approval remain separate. No binary release.

## SN-021 native resource-link integration: 2026-09-14

Source `5100aa3` was pushed on `codex/sn-021-native-links` after native composition
and full foundation acceptance. [PR #27](https://github.com/RicardoKers/SimNodus/pull/27)
records final required Foundation checks and the authorized squash. The exact
publication tree excludes twelve preexisting SN-045 changes, including shared
planning edits. Shared captured declarations do not verify source interfaces or
authorize resources, graph loading, compilation or execution. No binary release.

## SN-021 native project declaration integration: 2026-09-15

Source `6d9f380` was pushed on `codex/sn-021-native-project` after native project
and full foundation acceptance. [PR #28](https://github.com/RicardoKers/SimNodus/pull/28)
records final required checks and the authorized protected-main squash. Ten audited
source hashes match the workspace and exact publication tree. Twelve preexisting
SN-045 changes remain local and excluded, including shared planning edits.
Declaration acceptance does not verify firmware, boot/source interfaces or runtime
capabilities. Source graph loading, saving and compilation remain pending. No
issue synchronization, engine-profile expansion, UI or binary release is included.

## SN-021 native source graph integration: 2026-09-15

Source `c66ff52` was pushed on `codex/sn-021-source-graph` after native graph and
full foundation acceptance. [PR #29](https://github.com/RicardoKers/SimNodus/pull/29)
records final required checks and authorized protected-main squash. Ten source
hashes match the workspace and exact publication tree; twelve preexisting SN-045
changes remain local and excluded, including shared planning edits.
Source connectivity retains complete metadata and original provenance; it does
not authorize resources or execution. Atomic persistence, project path acquisition
and compilation remain pending. No issue synchronization or binary release.

## SN-021 atomic project creation integration: 2026-09-15

Source `ed50b39` was pushed on `codex/sn-021-atomic-create` after all 36 Windows
CTests and filesystem acceptance. [PR #30](https://github.com/RicardoKers/SimNodus/pull/30)
records final required checks and authorized protected-main squash. Fourteen
source hashes match the workspace and publication tree; twelve preexisting
SN-045 changes remain local and excluded, including shared planning edits.
This slice creates new project documents only. Safe overwrite, path acquisition,
editing and compilation remain pending. Killed writers may leave temporary files;
no power-loss durability is claimed. No resource execution or binary release.

## SN-021 overwrite ownership boundary integration: 2026-09-15

Source `f19e901` was pushed on `codex/sn-021-overwrite-boundary` after seven physical
counterexamples and all 37 Windows CTests passed.
[PR #31](https://github.com/RicardoKers/SimNodus/pull/31) records final required
checks and the authorized protected-main squash. Three source hashes match the
publication tree; 135 historical hashes and twelve local SN-045 changes are
preserved. The local overlay is excluded. Production persistence remains
create-only; tested overwrite candidates are rejected under ADR 0064.
Native project acquisition is next, without save authority. Safe overwrite,
editing and compilation remain pending. No resource execution or binary release.

## SN-021 native project acquisition integration: 2026-09-15

Source `87e23cd` was pushed on `codex/sn-021-project-acquisition` after all 38
Windows CTests passed. [PR #32](https://github.com/RicardoKers/SimNodus/pull/32)
records final required checks and authorized protected-main squash. Nine source
hashes match the publication tree; 138 historical hashes and twelve preexisting
local SN-045 changes are preserved. The local overlay is excluded.
Acquisition owns validated document bytes/graph without granting pathname or
overwrite authority. Editing, safe overwrite and compilation remain pending.
No resource execution, UI, issue synchronization or binary release is included.

## SN-021 project name revision integration: 2026-09-15

Source `6d4f7cf` was pushed on `codex/sn-021-project-rename` after 11 cases/33
revision requests and all 39 Windows CTests passed.
[PR #33](https://github.com/RicardoKers/SimNodus/pull/33) records final required
checks and authorized protected-main squash. Seven source hashes match the
publication tree; 147 historical hashes and twelve preexisting local SN-045
changes are preserved. The local overlay remains excluded.
The pure name edit preserves unrelated bytes and rebuilds provenance; it grants
no save, overwrite or execution authority. Compiler contract/integration and safe
overwrite remain pending. No UI, issue synchronization or binary release.

## SN-021 structural connectivity compilation integration: 2026-09-15

Source `aa8cd9f` was pushed on `codex/sn-021-connectivity-compilation` after ten
cases/eighteen requests and all 40 Windows CTests passed.
[PR #34](https://github.com/RicardoKers/SimNodus/pull/34) records final required
checks and authorized protected-main squash. Eight source hashes match the
publication tree; 154 historical hashes and twelve preexisting local SN-045
changes are preserved. The local overlay remains excluded.
Structural connectivity grants no model/interface, reference-ground, parameter
or runtime approval. Backend lowering with real-engine evidence and safe overwrite
remain pending. No UI, issue synchronization or binary release is included.

## SN-021 passive source inspection integration: 2026-09-15

Source `68360ac` was pushed on `codex/sn-021-passive-source` after seven reader
cases, all 41 Windows CTests and explicit real ngspice owned ideal RC acceptance
passed. [PR #35](https://github.com/RicardoKers/SimNodus/pull/35) records final
required checks and authorized protected-main squash. Eight source hashes match
the publication tree; 162 historical hashes and twelve local SN-045 changes are
preserved. The unrelated overlay remains excluded. Initial failed harness
evidence is retained. The reader grants no interface/readiness or execution
authority; descriptor/parameter binding, full lowering and safe overwrite remain
pending. No UI, issue synchronization or binary release is included.

## SN-021 passive interface correspondence integration: 2026-09-15

Source `57c32ce` was pushed on `codex/sn-021-passive-interface` after eight
correspondence cases and all 42 Windows CTests passed.
[PR #36](https://github.com/RicardoKers/SimNodus/pull/36) records final required
checks and authorized protected-main squash. Seven source hashes match the
publication tree; 170 historical hashes and twelve local SN-045 changes are
preserved. The unrelated overlay remains excluded. The inert result grants no
numerical/default, physical containment, complete interface or execution approval.
Full numerical binding/lowering and safe overwrite remain pending. No UI, issue
synchronization or binary release is included.

## SN-021 exact passive numerical binding integration: 2026-09-16

Source `3f67324` was pushed on `codex/sn-021-passive-numeric` after eight cases,
40 Decimal-oracle comparisons and all 43 Windows CTests passed.
[PR #37](https://github.com/RicardoKers/SimNodus/pull/37) records final required
checks and authorized protected-main squash. Seven source hashes match publication;
177 historical hashes and twelve local SN-045 files are preserved and the overlay
is excluded. Exact positive/range checks grant no solver-rounding, physical,
complete-interface or execution approval. Analysis authority, full backend lowering
and safe overwrite remain pending. No UI, issue synchronization or binary release.

## SN-021 explicit ideal RC compilation integration: 2026-09-16

Source `d605827` was pushed on `codex/sn-021-ideal-rc-compile` after eight compiler
cases (seven Windows passes/one platform skip), all 44 Windows CTests and explicit
real ngspice artifact acceptance passed with unchanged E-01/fixture tolerances.
[PR #38](https://github.com/RicardoKers/SimNodus/pull/38) records final required
checks and authorized protected-main squash. Eight source hashes match publication;
184 historical hashes and twelve local SN-045 files are preserved; the overlay
is excluded. Compilation uses captured resources and preserves full provenance;
execution remains a separate explicit action. Remaining project/runtime acceptance
and safe overwrite stay pending. No UI, issue synchronization or binary release.

## SN-021 project lifecycle integration: 2026-09-16

Branch `codex/sn-021-project-lifecycle` contains test-only composition of native
create/open/name-edit/save-copy/reopen/explicit ideal RC compilation. Eight Windows
cases, all 45 Windows CTests and two real pinned-ngspice attempts passed with
unchanged tolerances. The repository checker passed for 644 workspace and 642
publication files. Four source hashes, 98 historical evidence files and 14 schema
fixtures are recorded in the [audit](../experiments/evidence/SN-021-project-lifecycle-summary.json).
Twelve preexisting local files remain excluded. The branch pull request records
final hosted checks and the authorized protected-main squash; verify its final
head and local/remote main identity. Full SN-021, safe overwrite and firmware/
platform/runtime acceptance remain pending. No production API, UI, downloads,
issue synchronization or binary release is added.

## SN-021 inert ELF inspection publication: 2026-09-22

Branch `codex/sn-021-elf-inspection` contains the bounded pure ELF32 reader and
its 2026-09-16 acceptance. Six source hashes still match that acceptance: twelve
adversarial cases and all 46 Windows CTests passed. The historical compiler image
matched its original digest and independent field decoding. Preserve the initial
load-order failure; the final diagnostic does not approve loading or ABI conformance.
The [audit](../experiments/evidence/SN-021-elf-inspection-summary.json) preserves
99 historical evidence files, 14 schema fixtures and twelve unrelated local files,
which are excluded from publication. The repository checker covers 653 workspace /
651 publication files. The branch PR records final hosted checks and authorized
protected-main squash; verify its exact head and local/remote main identity.
SN-021 stays in progress: physical firmware association, architecture/device/boot/
runtime gates and safe overwrite remain pending. No firmware loading, UI, local
dependency downloads, issue synchronization or binary release is included.

## SN-021 firmware capture publication: 2026-09-22

Branch `codex/sn-021-firmware-capture` composes complete declaration validation,
physical byte capture and selected inert ELF inspection. Nine cases (eight Windows
passes/one platform skip), 47 Windows CTests and historical compiler-output capture
passed. Five source hashes and preserved historical/fixture hashes are recorded
in the [audit](../experiments/evidence/SN-021-firmware-inspection-summary.json).
Twelve unrelated local files remain excluded. The branch PR records final hosted
checks and authorized protected-main squash; verify final head and main identity.
Architecture correspondence, device/boot/runtime acceptance and safe overwrite
remain pending. No firmware loading, engine profile expansion, UI, dependency
download, issue synchronization or binary release is included.

## SN-021 static boot candidate publication: 2026-09-22

Branch `codex/sn-021-boot-candidate` adds the explicit adapter-local static SN-012
candidate under ADR 0073. Eight adversarial cases, 48 Windows CTests and historical
ELF/hash/symbol comparison passed; no firmware loading or engine run is claimed.
The [audit](../experiments/evidence/SN-021-boot-candidate-summary.json) records five
source hashes, unchanged fixture/evidence hashes and preservation of twelve local
files excluded from publication. The branch PR records final hosted checks and
authorized protected-main squash; confirm final head and local/remote main identity.
Project target/platform association, real loader/boot/runtime acceptance and safe
overwrite remain pending. No UI, downloads, issues or binary release is included.

## SN-021 reference target and real boot publication: 2026-09-22

Branch `codex/sn-021-target-boot` binds explicit reference target identity and
captures owned platform/firmware without automatic execution. The separate Python
experiment passed real Renode E-02 assertions at both existing quantum profiles
through retained staging leases. Seven gate cases (six Windows passes/one skip)
and 49 Windows CTests passed; the initial malformed test fixture is preserved.
The [audit](../experiments/evidence/SN-021-reference-target-summary.json) records
source/binary hashes, full engine observations and 102 unchanged historical files.
Twelve unrelated local files remain excluded. The branch PR records hosted checks
and authorized protected-main squash; verify final head and main identity.
Full SN-021, configured runtime correspondence and safe overwrite remain pending.
No automatic project execution, UI, download, issue synchronization or release.

## SN-021 acceptance gates publication: 2026-09-22

Branch `codex/sn-021-acceptance-gates` records the remaining acceptance matrix and
48 configured-policy rejection requests in two existing native-operation suites.
Both targeted CTests passed; native code and engine inputs are unchanged, so prior
real integration is reused without claiming another run. The
[audit](../experiments/evidence/SN-021-acceptance-gates-summary.json) records source,
fixture and historical hashes, including the superseded-CMake audit correction.
Twelve unrelated local files remain excluded. The PR records final hosted checks
and authorized protected-main squash; verify final head and local/remote main.
SN-021 is incomplete pending configured runtime correspondence and safe overwrite.
No new runtime profile, UI, dependency download, issue synchronization or release.

## SN-021 fixed configured replay publication: 2026-09-22

Branch `codex/sn-021-fixed-replay` binds one explicit configured replay to the
existing E-01 RC profile. Eight cases (seven Windows passes/one skip), 50 Windows
CTests and real ngspice passed unchanged project bounds. The
[audit](../experiments/evidence/SN-021-fixed-replay-summary.json) retains both the
initial harness-argument failure and corrected run, seven source hashes and 104
unchanged historical files. Twelve unrelated local files remain unpublished.
The branch PR records hosted checks and authorized protected-main squash; confirm
final head and local/remote main identity. Configured lifecycle composition and
safe overwrite remain pending. No new numerical/MCU profile, UI, download, issue
synchronization or binary release; project opening and compilation remain inert.

## SN-021 configured replay lifecycle publication: 2026-09-22

Branch `codex/sn-021-replay-lifecycle` composes existing native persistence and
fixed replay through test-only orchestration. Nineteen Windows cases, three targeted
CTests and the first real ngspice run passed, preserving configured policy/schedule
and source mapping through name-edit/save-copy/reopen. The
[audit](../experiments/evidence/SN-021-replay-lifecycle-summary.json) records three
changed source hashes, unchanged prior sources and 105 historical evidence files.
Twelve local files remain unpublished. The PR records hosted checks and authorized
protected-main squash; verify final head and main identity. Safe overwrite remains
pending ADR 0064. No production API/profile expansion, UI, download, issue sync or release.

## SN-021 transaction isolation investigation: 2026-09-22

Branch `codex/sn-021-txf-boundary` records a manual six-case fixture probe and
[observations](../experiments/SN-021-txf-boundary.md), not an adopted TxF product
API. Both preliminary reports and 106 historical evidence files are preserved;
twelve unrelated local files remain excluded. The PR records repository checks
and authorized source-evidence squash; verify final head and local/remote main.
Microsoft's recommendation to investigate alternatives remains explicit. No new
save support, engine profile, UI, download, issue synchronization or release.
The owner selected continued research without TxF and retained overwrite acceptance.
SN-021 remains in progress pending a validated overwrite protocol.

## SN-021 oplock boundary investigation: 2026-09-22

Branch `codex/sn-021-oplock-boundary` records three manual physical observations,
the first invalid-bit request failure and the [audit](../experiments/evidence/SN-021-oplock-boundary-summary.json).
No production overwrite support or TxF adoption. Preserve 107 historical evidence
files; twelve unrelated local files remain excluded. The PR records final hosted
checks and authorized squash; confirm final source/tree and local/remote main.
SN-021 remains in progress pending an enforceable ownership/publication protocol.
No engine/profile, UI, dependency, issue or release changes.

## SN-021 publication authority proposal: 2026-09-23

Branch `codex/sn-021-publication-authority` publishes a [research proposal](../architecture/PUBLICATION_AUTHORITY_PROPOSAL.md),
not an adopted service or storage architecture. It preserves 108 historical evidence
files and excludes twelve preexisting local files. Documentation checks and the
required hosted checks gate source publication; the PR records final results/squash.
The owner selected feasibility evaluation without service installation. SN-021
remains open pending physical evidence and eventual architecture/scope selection.
No runtime, permission, deployment, issue or release changes.

## Binary release gate


The SN-021 native syntax slice was published on 2026-09-14 at source `0dfd974`
on `codex/sn-021-declaration-ingress`. [PR #22](https://github.com/RicardoKers/SimNodus/pull/22)
records final hosted checks and the authorized squash. This is inert syntax
ingress, not full native schema/graph loading or a binary release. The local audit
and prior evidence are preserved; confirm the final PR head and main identity.

Record backend/runtime revisions and licenses, supported Windows versions, installer provenance, checksums, and tested lesson projects. Add required notices and source/relinking materials for dependencies as applicable. Do not redistribute CubeIDE, vendor firmware, model packs, or documentation by assumption.

Use `0.x` prereleases until compatibility expectations and project-format migrations are established. Publish limitations beside the download. Keep January's classroom candidate fixed except for reviewed fixes.
