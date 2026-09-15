# Latest handoff: SN-021 native source connectivity graph

Read CURRENT, BACKLOG, [ADR 0062](../decisions/0062-native-source-graph.md),
the [contract](../architecture/NATIVE_SOURCE_GRAPH.md) and
[report](../experiments/SN-021-graph.md). Project 0.1 now loads an immutable
hierarchical connectivity graph with standard C++ domain types and a complete
record/terminal source map. The entire validated declaration remains owned,
including parameters and every non-connectivity field. Nothing is executed.

Acceptance: 20 cases/56 native-reference graph comparisons with exhaustive byte
span checks; all 34 Windows CTests passed. The report records full regressions,
historical hashes, local physical-test privilege skips and publication checks.

SN-021 remains in progress. Next implement bounded persistence of owned validated
source bytes, with explicit destination ownership, atomic replacement and failure
preservation. Path-based project acquisition and editing remain separate. Then
compile with stable source mappings and the required real-engine evidence. Do not
infer ground, flatten source on load, discard metadata or expand supported profiles.

Base main: `d78c7bef598b6fe65d300584c1da3709d84af1b7` (PR #28); work branch:
`codex/sn-021-source-graph`. Check the final PR/head/check/squash record and actual
remote state before continuing. Preserve twelve preexisting local SN-045 changes,
including the shared planning overlay; they remain excluded from publication.
Keep SN-044, shared instrumentation, MCU/toolchain independence, numeric bounds,
PDF/PID behavior and SN-017 Python preparation/GDB/fixture ownership unchanged.

Prepare the requested new-chat prompt only when full SN-021 is accepted and
integrated. No automatic resource execution, downloads, rendering, UI, other SN
implementation, issue synchronization, releases or binaries are selected.

[PR #29](https://github.com/RicardoKers/SimNodus/pull/29), starting at source
`c66ff52`, records final required checks and protected-main squash integration.
Confirm its final state and main identity before persistence work.

# Historical handoff: SN-021 native project declarations

Read CURRENT, BACKLOG, [ADR 0061](../decisions/0061-native-project-declarations.md),
the [contract](../architecture/NATIVE_PROJECT_VALIDATION.md) and
[report](../experiments/SN-021-project.md). Native project 0.1 now composes the
declarative baseline over one immutable source capture, including platform,
firmware, target/pin-map and exact temporal rules. No resources are opened and
all firmware/runtime/source-interface/readiness flags remain unverified.

Acceptance: 20 cases/106 reference comparisons and native ownership/source-offset
checks; all 32 Windows CTests passed. The report records hashes, configuration
failures, local filesystem privilege skips and final publication checks.

SN-021 stays in progress. Next implement bounded native source graph loading with
stable IDs and source mappings, then safe persistence and compilation with required
integration evidence. Preserve declared/physical/byte/interface/trust/execution
boundaries. No additional engine profile or generalized MCU support follows.

Base main is `c7ac6f4937b6277d9c12542b231eb0c48ccf7011`, the validated PR #27
squash; work branch is `codex/sn-021-native-project`. Verify the report's final
PR/check/merge record and actual local/remote state. Keep twelve preexisting local
SN-045 changes excluded from publication, including shared planning edits. Preserve
SN-044, shared instrumentation, MCU/toolchain independence, numeric profiles,
PDF/PID behavior and SN-017 Python preparation/GDB/fixture ownership.

Prepare the owner-requested new-chat prompt for the next cycle only when full
SN-021 acceptance is integrated. No issue synchronization, releases, binaries,
downloads, automatic resource execution, UI or other SN implementation is selected.

[PR #28](https://github.com/RicardoKers/SimNodus/pull/28), starting at source
`6d9f380`, records final required checks and protected-main squash integration.
Confirm its final state and main identity before source graph work.

# Historical handoff: SN-021 native typed resource links

Read CURRENT, BACKLOG, [ADR 0060](../decisions/0060-native-resource-links.md),
the [contract](../architecture/NATIVE_RESOURCE_LINKS.md) and
[report](../experiments/SN-021-links.md). Native resource-links 0.1 now composes
topology/lock validation over one owned capture, with typed assets, complete maps
and explicit nulls. No resource is opened or execution granted. Standalone version
gates remain strict, and nested values/source offsets preserve their identity.

Focused acceptance: 19 cases/57 comparisons and C++ shared ownership/value/offset
checks. Full regression, hashes and publication are in the report. SN-021 remains
in progress. Next compose project declarations, then native graph loading, safe
persistence and source-preserving compilation with the required evidence. Keep
actual source/ELF/boot interfaces and execution approval independent.

The owner asks to prepare a new-chat prompt for the next cycle only when the full
SN-021 scope is accepted and integrated. Include official state, remaining limits
and the selected next task from planning. Do not issue a completion prompt now.

Branch `codex/sn-021-native-links` starts at main
`a6daf1d2ccc3bf409a45b479407b276d7b1e2690`. Verify final PR/checks/squash and actual
local/remote state. Preserve all twelve preexisting local SN-045 changes, including
the shared planning overlay; they are excluded from publication. Keep SN-044,
instrumentation, MCU/toolchain independence, numerical profiles, PDF/PID fixes and
SN-017 Python/GDB ownership. No issues, releases, downloads or automatic execution.

[PR #27](https://github.com/RicardoKers/SimNodus/pull/27), starting at source
`5100aa3`, records hosted checks and squash integration. Confirm its final state
and main identity before continuing with project composition.

# Historical handoff: SN-021 native resource lock metadata

Read CURRENT, BACKLOG, [ADR 0059](../decisions/0059-native-resource-lock.md),
the [contract](../architecture/NATIVE_RESOURCE_LOCK.md) and
[report](../experiments/SN-021-lock.md). Lock 0.1 now has native field/reference,
lexical path, exact budget and inventory fingerprint validation. It retains owned
source syntax and immutable declared resource requests with original offsets.
No root or resource is opened; physical/trust/readiness flags remain false.

Focused acceptance: 23 cases/135 metadata comparisons/137 digest cases and native
ownership tests. The report records full regressions, historical hashes, one
Python host integer-cap diagnostic difference and the publication/check record.
SN-021 stays in progress. Next port typed resource links and project semantics
before native graph loading; do not repeat physical snapshot or descriptor work.
Actual source/ELF/boot interfaces, atomic saving, compilation and execution policy
remain separate gates. No public CLI, UI or engine-profile expansion is selected.

Branch `codex/sn-021-native-lock` starts at main
`bd1da0df6ad8a18b9884ca441660207cdf255fbe`. Confirm final PR/checks/squash and local/
remote state before continuing. Preserve all twelve local SN-045 changes, including
the shared planning overlay; they are excluded from this publication. Keep SN-044,
instrumentation, MCU/toolchain independence, numerical bounds, PDF/PID fixes and
SN-017 Python/GDB ownership. No issue sync, dependency download or binary release.

[PR #26](https://github.com/RicardoKers/SimNodus/pull/26), starting at source
`ead5b71`, records final hosted checks and the authorized squash. Verify its final
state and local/remote main identity before continuing with typed resource links.

# Historical handoff: SN-021 native descriptor bindings

Read CURRENT, BACKLOG, [ADR 0058](../decisions/0058-native-descriptor-bindings.md),
the [contract](../architecture/NATIVE_BINDINGS.md) and
[report](../experiments/SN-021-bindings.md). Topology 0.3 now has native descriptor,
explicit-null, bijective-map and exact interval validation with immutable source
ownership and parameter occurrence inspection. It never opens resources or engines.
The earlier public 0.1 and 0.2 operations remain separate and strict.

Focused acceptance: 22 cases/157 native-reference comparisons and native ownership/
source-position tests. See the report for complete regressions, retained initial
fixture-construction failures, historical hashes and final hosted checks/squash.
SN-021 remains in progress. Next port resource lock/link and project semantics
before native graph loading. No source importer, renderer, runtime authorization,
atomic saving or compilation is implemented by descriptor validation.

Branch `codex/sn-021-native-bindings` starts at main
`9e82f3af1cd16a9bce69fa7c4fa9f4a9e3aa09b2`. Verify actual local/remote state and
the report's final PR record. Twelve preexisting SN-045 changes remain local and
excluded; preserve their shared planning overlay. Keep SN-044, shared instrumentation,
MCU/toolchain independence, numerical profiles, PDF/PID fixes and SN-017 Python/GDB
ownership. No issue synchronization, dependency download or binary release.

[PR #25](https://github.com/RicardoKers/SimNodus/pull/25), starting at source
`1a6de52`, is the final hosted-check and protected-main squash record for this slice.
Confirm its final state and main identity before continuing with resource metadata.

# Historical handoff: SN-021 native exact parameters

Read CURRENT, BACKLOG, [ADR 0057](../decisions/0057-native-exact-parameters.md),
the [contract](../architecture/NATIVE_PARAMETERS.md) and
[report](../experiments/SN-021-parameters.md). Native topology 0.2 now validates
exact quantities, units, ranges, scoped overrides and interval-safe forwarding,
then returns immutable inspection rows with distinct occurrence paths and origins.
The API retains source bytes/offsets and performs no resource or engine I/O.
The topology 0.1 entry point remains separate and strict.

Focused acceptance: 22 cases/315 native-reference comparisons and C++ ownership,
signed-zero, namespace/source-offset checks. The report records full regressions,
historical hash checks and final publication. Next port topology 0.3 descriptor/
model declaration semantics; resource/project validation and graph loading remain
pending. Keep metadata, physical bytes, source interfaces, trust and execution
authorization independent. SN-021 remains in progress; saving/compilation/UI and
runtime readiness are not implemented by this slice.

Branch `codex/sn-021-native-parameters` starts at main
`f05b993a278e2372f97aa62bda3277b7703e26bc`. Follow the report's final PR/checks
and squash record. Twelve preexisting SN-045 documentation changes remain local
and excluded; preserve them when synchronizing main. Keep SN-044, MCU/toolchain
independence, engine tolerances, PDF/PID fixes and SN-017 Python/GDB ownership.
No issue synchronization or binary release.

[PR #24](https://github.com/RicardoKers/SimNodus/pull/24), starting at source
`a561149`, is the hosted-check and squash record. Confirm its final state and
local/remote main identity before continuing with descriptor declarations.

# Historical handoff: SN-021 native topology semantics

Read CURRENT, BACKLOG, [ADR 0056](../decisions/0056-native-topology-semantics.md),
the [native contract](../architecture/NATIVE_TOPOLOGY_VALIDATION.md) and
[report](../experiments/SN-021-topology.md). Native validation now covers the
complete preserved topology 0.1 semantic rules over immutable captured syntax.
It checks all definitions, namespaces, typed references, terminal uniqueness,
cycles, depth and expansion without opening any resource or invoking engines.
Later versions are rejected; this is not the full project/topology 0.3 loader.

Next port exact parameter/scoped-override semantics, then bindings, resource and
project rules before source graph publication. Preserve source IDs/offsets,
explicit unbound states, numeric budgets and independent physical/interface/
trust/execution gates. Full declaration validation remains in Python. SN-021
remains in progress; saving, compilation and runtime readiness remain pending.

Base main `386fef7586b8fa4ec34dbd6bddc559a109f27800`; work branch
`codex/sn-021-topology-semantics`. The report records tests and the final PR.
Twelve preexisting SN-045 documentation changes remain local and are excluded
from this slice. Preserve them; do not mistake them for SN-021 changes or delete
them to make the working tree clean. Confirm actual main/remote and PR status.
Keep SN-044, MCU/toolchain independence, engine restrictions/tolerances, PDF/PID
behavior and SN-017 Python/GDB fixture ownership. No issue sync or binary release.

[PR #23](https://github.com/RicardoKers/SimNodus/pull/23), starting at source
`4cccfe7`, is the hosted-check and protected-main squash record for this slice.

# Historical handoff: SN-021 native declaration syntax ingress

Read CURRENT, BACKLOG, [ADR 0054](../decisions/0054-declaration-ingress.md),
the [contract](../architecture/DECLARATION_INGRESS.md) and
[report/evidence](../experiments/SN-021-ingress.md). The API captures at most
1 MiB into immutable original bytes and lossless syntax tokens. It performs no
I/O and grants no schema validity, physical approval or execution authorization.

Local acceptance: 95 native assertions, 209 differential/adversarial cases,
94 existing schema tests, 20 Windows CTests and 55 preserved historical hashes.
Physical-resource privilege/platform skips remain explicitly recorded. Native
lone-surrogate rejection is stricter than the unchanged Python baseline.
Repository checker passed for 488 text files; `git diff --check` passed.

Next validate project/schema semantics over captured tokens before building an
immutable source graph. Use existing Python fixtures as semantic oracles,
including rejection cases. Preserve exact integers, source locations, stable IDs,
reference/expansion limits and explicit unbound states. Do not interpret syntax
success as permission to open resources or create a simulation session.

SN-021 remains in progress. Saving, compilation, actual source/firmware interfaces,
runtime policy and UI remain pending. Preserve SN-044, ADRs 0027/0028/0043/0044/0049,
Python/GDB fixture ownership, tolerances, PDF/PID behavior and all historical evidence.
Work branch `codex/sn-021-declaration-ingress` starts from synchronized main
`5fa212e970e0f363e846d82f81cdb48ec1e9a10c`; PR/checks/squash/push remain authorized.
Verify the final PR and local/remote main identity. No issue sync or binary release.
[PR #22](https://github.com/RicardoKers/SimNodus/pull/22) is the final hosted-check
and squash record, starting from source `0dfd974`. Use that record to confirm
integration before continuing; do not repeat the syntax extraction.

# Historical handoff: SN-021 native resource snapshots

Read CURRENT, BACKLOG, [ADR 0053](../decisions/0053-native-resource-verification.md),
the [native contract](../architecture/NATIVE_RESOURCE_VERIFICATION.md),
[report](../experiments/SN-021-native-resources.md) and
[evidence](../experiments/evidence/SN-021-native-resources-summary.json).
SN-021 remains **in_progress**. A typed C++20 API now returns immutable complete
snapshots or structured errors; Windows details are confined to platform code.
Complete declaration/lock JSON validation remains a caller-side Python gate.
Do not present the typed test transport as a project format or native graph loader.

Local results: 41 native typed cases; 32 native filesystem cases with 30 passes
and two unavailable symlink privileges; actual long-name/8.3 and case-sensitive
directory rejection measured. Existing schema/Python suites and 18 Windows
CTest entries passed. Native case-sensitive-directory rejection is deliberately
stricter than the preserved Python reference. The final C: temporary-file check
requires ordinary host access, not the restricted agent token; no ACL/settings
were changed. Preserve the initial build and fixture-setup failure records.
All 44 selected historical input hashes matched; the repository checker passed
for 479 text files and `git diff --check` passed.

Next specify bounded native declaration ingress and graph construction before
connecting project opening. Retain same-handle byte verification, immutable
snapshot consumption and separate metadata/interfaces/trust/execution gates.
Full JSON semantics, SVG/SPICE/ELF/boot compatibility, atomic saving, source
mapping/compilation and runtime negotiation remain pending. Preserve SN-044,
ADRs 0027/0028/0043/0044/0049, Python/GDB fixture ownership, engine restrictions,
tolerances, PDF suppression, PID retry and all historical evidence/hashes.

Work branch: `codex/sn-021-native-resources`, based on clean synchronized main
`0c5aa6df3a7d179ed71a41c3492b385de5155007`. Validated commits, PR/checks, squash
integration and push remain authorized. Verify the new PR and actual main state;
do not synchronize issues or publish releases/binaries. Older handoffs below
are historical and do not override current authorization.

Source `bf3e05b` was pushed on 2026-09-14. Use
[PR #21](https://github.com/RicardoKers/SimNodus/pull/21) to resolve the final
checks, merge status and squash identity, then compare local main with origin.
Do not repeat the completed native snapshot extraction or rewrite its evidence.
The native report preserves a hosted temporary-root fixture failure and its
follow-up. The corrected test distinguishes an aliased TEMP ancestor from the
long root spelling; production containment rules remain unchanged. Confirm the
final hosted checks rather than treating that earlier failed run as acceptance.
Follow-up `5f23db4` passed both jobs in hosted run 34802067514: all 32 native
filesystem cases executed on Windows, with 18 Windows/13 Linux CTest entries
passing. The native snapshot slice is accepted. Start the next bounded declaration
ingress/graph specification after confirming PR #21's final squash/main state.

# Historical handoff: SN-021 bounded local resource snapshots

Read CURRENT, BACKLOG, [ADR 0052](../decisions/0052-bounded-local-resource-snapshots.md),
the [physical contract](../architecture/LOCAL_RESOURCE_VERIFICATION.md),
[implementation](../../tests/resources/README.md) and
[report/evidence](../experiments/SN-021-local-resources.md). SN-021 is **in_progress**.
The explicit reference operation accepts lock bytes and an absolute Windows/NTFS
root, uses parent-relative handles and returns complete immutable byte snapshots.
Metadata validation remains inert; hashes do not establish interfaces, trust,
redistribution permission, execution authorization or simulation readiness.

The 32-case local resource suite has 28 passes and four skips: non-Windows test,
two missing symlink privileges and disabled 8.3 generation. Hosted Windows must
run the symlink cases before integration. The original 94 schema tests remain
separate. CTest adds `local-resource-snapshots` to the prior 15 Windows entries.
Fresh Debug build and all 16 CTest entries passed; 40 historical hashes matched.
Repository checker passed for 468 text files and `git diff --check` passed.
Do not claim skipped alias/symlink checks passed locally or Linux loading works.

Next review/extract this bounded verifier into native application code before
connecting project loading. Keep same-handle checking and snapshot consumption;
never substitute textual prefix or resolve-then-open checks. Source/SVG/SPICE,
ELF/boot compatibility, atomic saving, compilation and runtime negotiation remain
separate gates. Do not start UI/instrumentation or expand supported engine profiles.
Preserve SN-044, ADRs 0027/0028/0043/0044/0049/0051, Python preparation/GDB/fixture
ownership, historical evidence/hashes, tolerances, PDF suppression and PID retry.

Work branch: `codex/sn-021-local-resources`, from main/origin
`7307bd88e2d468d65363fa6a6f0776772cbb124a`. PR/checks/squash integration and push
of stable validated work are authorized. Source commit `cb6fc8d` was pushed and
both Foundation checks passed in run `34785300902`.
[PR #20](https://github.com/RicardoKers/SimNodus/pull/20) records final checks and
squash integration; verify its state and main identity when resuming. Foundation
prints verbose CTest summaries, including skips. Do not synchronize issues or
publish releases/binaries. Earlier
handoffs below are historical and do not override current authorization.

# Historical handoff: SN-020 declaration baseline accepted

SN-020 is **done for declaration schema/reference validation** under
[ADR 0051](../decisions/0051-project-declaration-baseline.md). Read CURRENT,
BACKLOG, the [project contract](../architecture/PROJECT_SCHEMA.md) and
[acceptance audit/evidence](../experiments/SN-020-acceptance.md).
All 94 schema tests and 15 CTest entries passed, native Debug build succeeded,
and 25 prior schema hashes match. Resource links, explicit model entrypoints,
board/MCU/firmware targets and requested temporal/fidelity policy are declarative.
All physical/resource/interface/firmware/runtime readiness flags remain false.

The next implementation task is SN-021, still planned and not started here.
Begin only within its authorized scope with bounded physical resource verification
before native loading/atomic saving/compilation. Do not equate valid declarations
with safe resource contents, real firmware compatibility or runtime capability.
Keep stable source identities and test missing files, mismatches, aliases,
symlinks/junctions and replacement races before consuming resources.

SN-044 is done and its [UI/UX decision](../decisions/0049-desktop-ux-and-measurements.md)
is preserved. Do not redo it or start Qt/UI/instrumentation tasks implicitly.
Preserve all prior engine evidence, bounded Python/GDB/fixture ownership,
capabilities, tolerances, PDF suppression, PID retry and ADRs 0027/0028.

The current user authorizes coherent validated commits and protected-main
integration/push. `codex/sn-020-schema` is the integration branch, with source
commit `1500898a7ad08b37d7e6fba15c53183fdb584875` published and
[PR #19](https://github.com/RicardoKers/SimNodus/pull/19) recording main integration
and hosted checks. Verify PR and remote state before claiming publication.
This supersedes older checkpoint-only
restrictions below. No issues or binary releases are requested; build data remains
ignored/local. Older handoffs describe historical task states.

# Historical handoff: SN-020 inert resource lock

Read CURRENT, BACKLOG, [ADR 0048](../decisions/0048-declarative-resource-lock.md),
the [resource lock contract](../architecture/RESOURCE_LOCK_DRAFT.md) and its
[report/evidence](../experiments/SN-020-resource-lock.md). SN-020 remains
**in_progress**. All 64 tests passed; the valid CLI accepts two owned resources,
and the traversal fixture fails with `path`. Nineteen prior hashes still match.

Next take the smallest **typed descriptor/resource reference** slice. Define
explicit dependency/resource IDs and roles without reading or executing resources.
The separate resource lock 0.1 validates metadata and lexical paths only; physical
containment against symlinks/junctions/replacement races and actual file hash
verification remain SN-021 work. No loader, permission approval or simulator
readiness follows from a valid lock. Topology 0.1/0.2/0.3 remain unchanged.

Board/MCU/firmware and temporal policies remain pending. Preserve earlier evidence,
capability limits, tolerances, PDF suppression, PID retry, Python preparation/GDB/
fixture ownership and ADRs 0027/0028. No new engine run is claimed.

Checkpoint `b3ce6748fcf18ca996dac655d54a9858e39c64a1` was pushed and verified on
`codex/checkpoint-2026-09-13`. The resource-lock slice is subsequent local work;
no new commit/push or issue/release publication occurred. The prior authorization
covered the completed checkpoint, not recurring publication. Build data is still
ignored and local.

# Historical handoff: SN-020 descriptor bindings and authorized checkpoint

SN-020 is **in_progress** through [0.3](../architecture/BINDINGS_DRAFT.md) under
[ADR 0047](../decisions/0047-declarative-bindings-draft.md). Read CURRENT, BACKLOG
and the [report/evidence](../experiments/SN-020-bindings.md).
All 46 tests passed. Symbol/model interface catalogs and explicit complete
pin/parameter maps are separate from topology. Replacing a symbol preserves
connectivity, model bindings and resolved values. Models are declarations only;
every result has `simulation_ready: false`. Old 0.1/0.2 files/hashes remain intact.

Next take the smallest **locked resource/dependency metadata and path containment**
slice, including origin/license records, without downloading or executing code.
Board/MCU/firmware and temporal policies remain pending. SN-021 native persistence
and compilation stays planned. Preserve the earlier drafts and engine evidence,
capability limits, PDF suppression, PID retry and ADRs 0027/0028.

The owner authorized a source checkpoint commit and GitHub push on 2026-09-13.
Checkpoint branch: `codex/checkpoint-2026-09-13`; verify local/remote tips for
transfer status. This authorization does not include merging main, release or
issue synchronization. Build outputs/raw runtime directories remain ignored and
local; this checkpoint is not a complete disk backup. Earlier no-publication
notes below are historical and do not override this checkpoint authorization.

# Historical handoff: SN-020 exact parameters/overrides passed

SN-020 is **in_progress**. Read CURRENT, BACKLOG,
[ADR 0046](../decisions/0046-exact-parameter-draft.md),
[0.2 contract](../architecture/PARAMETERS_DRAFT.md) and
[report/evidence](../experiments/SN-020-parameters.md).
All 29 tests passed (15 preserved 0.1 topology tests plus 14 parameter tests).
Exact decimal strings, closed units, inclusive ranges, literal instance overrides
and direct containing-circuit forwarding are implemented in the developer tool.
Forwarded source ranges must fit targets completely. Shared definitions remain
unchanged; left/right RC document values resolve independently. No simulation
or model loading occurs. All 0.1 source/fixture/contract hashes remain intact.

Next take the smallest **separate declarative symbol/model descriptors and explicit
pin/parameter mappings** slice, validating IDs and references without opening
resources or executing models. Preserve 0.1/0.2 contracts and evidence when
evolving versions. Full resource/dependency, board/MCU/firmware and temporal
contracts remain pending; SN-021 actual persistence/compilation stays planned.

Keep names, symbols, connectivity, electrical models and board representation
separate. No generic expression language, GUI or SDK is requested. Preserve
SN-017/SN-018 limitations, previous engine evidence, numerical tolerances, PDF
suppression, PID retry, Python GDB/fixture ownership and ADRs 0027/0028. No commit,
issue synchronization or remote publication is authorized. Older notes are historical.

# Historical handoff: SN-020 topology draft passed

SN-020 is **in_progress**. Read CURRENT, BACKLOG,
[ADR 0045](../decisions/0045-experimental-topology-draft.md),
[topology 0.1](../architecture/TOPOLOGY_DRAFT.md) and the
[report/evidence](../experiments/SN-020-topology.md).
The developer reference validator accepts component pin declarations, circuits,
instances, ports and explicit nets, with stable IDs and bounded hierarchy.
Fifteen tests and valid/invalid CLI fixtures passed. Two RC instances demonstrate
distinct structural paths only; no values, model or simulation are defined.

Next take the smallest **unit-bearing parameter declarations and per-instance
overrides** slice: define numeric representation, dimensions/units, ranges and
invalid/round-trip evidence. Preserve 0.1 fixtures/contract when evolving the
draft version. Do not load resources/models, start a GUI or build a broad SDK.
Full symbol/model, dependencies/resources, board/firmware and temporal contracts
remain pending; SN-021 native persistence/compilation stays planned.

Topology validity is not electrical solvability or engine capability. Keep
identity independent of names/symbols and hierarchy instance-specific. Opening
future project files must not execute host code, download or load native models.
SN-017/SN-018 bounded acceptance, numerical tolerances, PDF suppression, PID
retry, Python GDB/fixture ownership and ADRs 0027/0028 remain unchanged. No commit,
issue synchronization or remote publication is authorized. Older notes are historical.

# Historical handoff: SN-018 accepted; next select SN-020

SN-018 is **done for the bounded local Windows Debug baseline** under
[ADR 0044](../decisions/0044-bounded-baseline-acceptance.md). Read CURRENT,
BACKLOG, the [acceptance audit](../experiments/SN-018-acceptance.md) and
[evidence](../experiments/evidence/SN-018-acceptance-summary.json).
Stepping, paced pause/resume and recreated-session references share one fixed
circuit/firmware fixture. Latency/error/memory are measured, not product targets.
Read-only audit revalidated 3793 hashes, 36 sessions, 324 fixed checkpoints,
360 analog checks and 14122 ancestry rows. No new engines or IDE runs were made.

Next select **SN-020**: start the smallest coherent circuit/component schema and
validation specification. Read architecture/components, persistence and relevant
ADRs before proposing stable IDs, connectivity/symbol/model separation, hierarchy
and untrusted-input validation. Do not automatically build a GUI, loader or broad
SDK. SN-020 is still planned until selected; no production simulator is ready.

Keep SN-017's native/Python composition boundary explicit: Python owns preparation,
GDB transport and fixture sequencing. Preserve baseline-01, observer-01 and
identity-01 plus all prior evidence. The contaminated control remains inconclusive;
only the corrected identity campaign supplies accepted sampled memory attribution.
Mixed timing signs do not establish zero overhead. Portable setup, Release,
scaling, exact peaks/leaks, product budgets and classroom readiness are unvalidated.
Retain 10 microvolt/1 ps/2 s limits, pacing, PDF suppression, PID retry and ADRs
0014/0027/0028/0043. No commit, issue synchronization or publication is authorized.
Older handoffs below are historical.

# Historical handoff: SN-018 corrected observer control passed

SN-018 is **in_progress**. Read CURRENT, BACKLOG and the
[corrected observer report](../experiments/SN-018-identity-control.md),
[predeclared contract](../../tests/headless/OBSERVER_IDENTITY.md) and
[evidence](../experiments/evidence/SN-018-identity-summary.json).
Nine identity/adversarial/Windows-child tests passed before six real batches.
All 36 sessions passed 324 fixed checkpoints and 36 pauses, final virtual
4027000 ns and ADC 3541. Independent ancestry audit passed all 14122 sampled
process rows; zero query errors, 281 conservative candidate rejections.
The new sampler retains handles and verifies creation/exit chronology, without
name filtering. Original PID-only code and the inconclusive control remain intact.

Raw output: `build/sn018/identity-01`; preserve it, observer-01 and baseline-01.
S-minus-U wall differences were -0.695/-0.863/+0.558 s; mixed signs do not prove
stable causal slowdown or zero overhead. Memory remains a sampled lower bound.
Next perform a bounded SN-018 coverage/acceptance review: define whether the
existing fixed fixture/subcases and measurement evidence fulfill its criterion,
or name one concrete remaining gap. Do not automatically broaden capabilities
or run more identical batches. Preserve unresolved portability/Release/scaling
limits and distinguish existing reference evidence from new execution.

Python still owns preparation, GDB and fixture sequencing. Keep the exact engines,
native flags, 10 microvolt/1 ps/2 s limits, pacing, PDF suppression, PID retry and
ADRs 0014/0027/0028/0043. No autonomous all-C++ simulator, commit, issue
synchronization or publication is authorized. Older handoffs below are historical.

# Historical handoff: SN-018 observer attribution defect

SN-018 remains **in_progress**. Read CURRENT, BACKLOG, ADRs 0014/0043 and the
[observer control report](../experiments/SN-018-observer-control.md),
[predeclared contract](../../tests/headless/OBSERVER_CONTROL.md) and
[annotated evidence](../experiments/evidence/SN-018-observer-summary.json).
Six U,S / S,U / U,S batches passed all 36 real-engine sessions and 324 fixed
checkpoints, with unchanged final virtual time/ADC and numerical limits.
The measurement is nevertheless **inconclusive**: batch 3 included unrelated
desktop processes in 80 memory samples, with 12562 query errors. No retries or
discarded batches. Preserve all of `build/sn018/observer-01` and baseline-01.

Next correct/test the memory sampler's ownership detection. It currently follows
numeric parent PIDs without creation-time identity; stale ancestry/PID reuse is
plausible but cannot be proven from the old rows. Retain parent and creation
evidence, test stale parent/PID reuse and process exits, and do not substitute a
process-name allowlist. Preserve the original sampler/source hashes; create a
corrected implementation separately. Predeclare a new control only after tests
pass. Earlier baseline memory is provisional; a name audit found no unexpected
names there but cannot establish ownership. Broader SN-018 coverage review follows.

Keep engines/fixture/native flags, 10 microvolt/1 ps/2 s limits, pacing, PDF
suppression, PID retry and ADRs 0027/0028 unchanged. Python still owns preparation,
GDB transport and fixture sequencing. No general autonomous C++ simulator,
commit, issue synchronization or remote publication is authorized.
Older handoffs below are historical.

# Historical handoff: SN-018 first local baseline measured

SN-018 is **in_progress**. Read CURRENT, BACKLOG, ADRs 0014/0043 and the
[first baseline report](../experiments/SN-018-baseline.md),
[predeclared contract](../../tests/headless/BASELINE.md) and
[evidence](../experiments/evidence/SN-018-baseline-summary.json).
Three batches / 18 fresh real-engine sessions passed using unchanged cycle 27
inputs: 162 fixed checkpoints, 18 pauses, ADC 3541 and final virtual 4027000 ns.
Pause wall latency was 45.089-93.517 ms (median 68.609 ms); batch wall times
were 57.472/62.118/63.004 s. Sampled process-tree memory and query/sample gaps
are recorded, not exact peaks. Maximum RC error was 1.005721e-9 V.

Next take a bounded measurement refinement: predeclare an unsampled control
of this same fixture to assess observer overhead before drawing performance
conclusions or adding examples. The first run is one Windows Debug host, not
Release, clean-machine, scaling, leak or classroom evidence. Keep SN-018 open
until reference-example/measurement coverage is assessed explicitly.
Raw output is `build/sn018/baseline-01`; use fresh paths and retain all evidence.
SN-017 remains accepted only as native/Python headless composition: Python owns
preparation, GDB transport and fixture sequencing. Preserve the 10 microvolt,
1 ps and 2 s limits, paced profile, PDF suppression, PID retry and ADRs 0027/0028.
No commit, issue synchronization or remote publication is authorized.
Older handoffs below are historical.

# Historical handoff: SN-017 accepted; SN-018 ready

SN-017 is done for the bounded native/Python headless composition under
[ADR 0043](../decisions/0043-bounded-headless-extraction-acceptance.md). Read latest
CURRENT, the [acceptance audit](../experiments/SN-017-acceptance.md) and its
[evidence](../experiments/evidence/SN-017-acceptance-summary.json).
Native contracts, adapters and application session are validated; Python still
owns GDB transport, preparation, fixture scheduling and evidence collection.
The audit revalidated unchanged cycle 27 inputs/results: 14 CTest targets,
358 protocol/process cases, 18 Python tests, six recovery sessions/54 checkpoints
and four real loss controls. It does not claim new engine or CubeIDE execution.
Next select SN-018: establish reproducibility/performance measurements for the
same declared profile, fixing inputs and latency/error/memory methodology before
measurement. Preserve all evidence, numeric tolerances, PDF suppression, PID retry
and ADRs 0014/0027/0028. Do not expand capabilities or claim a general product.
No commit, remote issue synchronization or publication is authorized.
Older handoffs below are historical.

# Latest handoff: SN-017 application session composed

The twenty-seventh cycle is validated; SN-017 remains in progress. Read latest
CURRENT, [ADR 0042](../decisions/0042-fixture-application-session.md), the
[protocol](../../tests/headless/APPLICATION_SESSION.md), and the
[evidence](../experiments/evidence/SN-017-application-session-summary.json).
`FixtureSession` owns joint state, coordinators and global dispatch/pending/commit
gates. CLI owns startup, endpoints and text I/O. Fourteen CTest targets, 358
protocol/process cases and 18 Python tests passed. Six recovery sessions passed
54 checkpoints. Two voltage differences below 2e-14 V are recorded within the
existing 10 microvolt tolerance; times and fixed digital state matched cycle 26.
Next audit SN-017 acceptance against the extracted runner/contracts and evidence.
Identify remaining Python orchestration and decide whether bounded extraction
can close or needs one concrete gap addressed. Do not infer general application
readiness or expand capabilities. Preserve all reports, historical failures,
PDF suppression, PID retry and ADRs 0014/0027/0028. No commit or remote publication
is authorized. Older handoffs below are historical.

# Latest handoff: SN-017 execution coordinator extracted

The twenty-sixth cycle is validated; SN-017 remains in progress. Read latest
CURRENT, [ADR 0041](../decisions/0041-fixture-execution-coordinator.md), the
[protocol](../../tests/headless/EXECUTION_COORDINATOR.md), and the
[evidence](../experiments/evidence/SN-017-execution-coordinator-summary.json).
`FixtureExecution` owns readiness/cancellation/debug state and borrows the CLI
channel. Thirteen CTest targets, 358 protocol/process cases and 18 Python tests
passed. Six recovery sessions passed 54 checkpoints. Progress poll counts may
vary; fixed boundaries and start/cancel/notify counters matched cycle 25.
Next extract remaining command dispatch and global pending/commit gates into a
bounded application session, preserving protocol, endpoint ownership, deadlines
and separate commit. Retain the failed ingress setup report and corrected raw
grant reference, all prior evidence, PDF suppression, PID retry and ADRs
0014/0027/0028. No commit or remote publication is authorized. Older handoffs
below are historical.

# Latest handoff: SN-017 analog worker coordinator extracted

The twenty-fifth cycle is validated; SN-017 remains in progress. Read latest
CURRENT, [ADR 0040](../decisions/0040-fixture-analog-coordinator.md), the
[protocol](../../tests/headless/ANALOG_COORDINATOR.md), and the
[evidence](../experiments/evidence/SN-017-analog-coordinator-summary.json).
`FixtureAnalog` owns worker sequencing/acceptance and borrows CLI-owned endpoints.
Twelve CTest targets, 251 protocol/process cases and 18 Python tests passed.
Six recovery sessions passed 54 checkpoints with unchanged analog counters.
Worker adapters, ADC and inspection coordinators remain unchanged.
Next extract bounded Renode execution coordination (start/readiness, cancellation
and result acceptance) from the CLI, retaining grant accounting, debug interaction
and transport deadlines. Keep fixture scope and ADRs 0014/0027/0028.
Preserve all reports, PDF suppression and startup PID retry. No commit or remote
publication is authorized. Older handoffs below are historical.

# Latest handoff: SN-017 ADC coordinator extracted

The twenty-fourth cycle is validated; SN-017 remains in progress. Read latest
CURRENT, [ADR 0039](../decisions/0039-fixture-adc-coordinator.md), the
[protocol](../../tests/headless/ADC_COORDINATOR.md), and the
[evidence](../experiments/evidence/SN-017-adc-coordinator-summary.json).
`FixtureAdc` owns preparation, transfer state, original deadline and helper
lifetime. Eleven CTest targets, 194 protocol/process cases and 18 Python tests
passed. Six recovery sessions passed 54 checkpoints and exactly one ADC transfer
each. Helper and final-inspection adapter sources remain unchanged.
Next extract bounded analog worker coordination (initialization, catch-up and
high/inspection sequencing) from the CLI, retaining raw reply ingress, RC checks,
deadlines and backend ownership. Keep all capability restrictions and ADRs
0014/0027/0028. Preserve reports, PDF suppression and startup PID retry.
The prior cycle's aggregate was 173 cases, not 183; CURRENT records the correction
without changing historical raw evidence. No commit or remote publication is
authorized. Older handoffs below are historical.

# Latest handoff: SN-017 final inspection coordinator extracted

The twenty-third cycle is validated; SN-017 remains in progress. Read latest
CURRENT, [ADR 0038](../decisions/0038-fixture-inspection-coordinator.md), the
[protocol](../../tests/headless/INSPECTION_COORDINATOR.md), and the
[evidence](../experiments/evidence/SN-017-inspection-coordinator-summary.json).
`src/application/fixture_inspection.hpp` owns final validation state and its
existing protocol; CLI transport, global pending guards, abort and actual commit
remain. Ten CTest targets, 183 pipe cases and 18 Python tests passed. Six recovery
sessions passed 54 checkpoints with final diagnostics matching cycle 22.
Next extract bounded ADC preparation/helper coordination from the CLI into an
application coordinator. Preserve the existing protocol, single-transfer rule,
deadlines and helper process cleanup. Do not add general backend capabilities.
Retain all reports, PDF suppression, startup PID retry and ADRs 0014/0027/0028.
No commit or remote publication is authorized. Older handoffs below are historical.

# Latest handoff: SN-017 native post-pair time confirmation complete

The twenty-second cycle is validated; SN-017 remains in progress. Read latest
CURRENT, [ADR 0037](../decisions/0037-native-post-inspection-time.md), the
[protocol](../../tests/headless/INSPECTION_TIME.md), and the
[evidence](../experiments/evidence/SN-017-inspection-time-summary.json).
`--native-inspection-time` requires raw time confirmation after the second
register reply and before final commit, under the original 1900 ms deadline.
Six recovery sessions passed 54 checkpoints; all post-pair replies match GDB logs.
Host GDB transport and untagged-stream association remain.
Next extract bounded final readback/inspection state from the CLI into a testable
native coordinator, preserving the wire protocol, shared deadline and separate
commit. Keep the existing scope; do not add general backends or capabilities.
Preserve all reports, PDF suppression, PID retry and ADRs 0014/0027/0028.
No commit or remote publication is authorized. Older handoffs below are historical.

# Latest handoff: SN-017 native observation interval complete

The twenty-first cycle is validated; SN-017 remains in progress. Read latest CURRENT,
[ADR 0036](../decisions/0036-native-observation-interval.md), the
[protocol](../../tests/headless/INSPECTION_INTERVAL.md), and the
[evidence](../experiments/evidence/SN-017-inspection-interval-summary.json).
`--native-inspection-interval` withholds the second request until at least 100 ms
of native monotonic time, without renewing the mailbox deadline. Six recovery
sessions passed; release durations were 100.77-101.30 ms and all register results
matched GDB logs. Host transport, polling and the post-pair time assertion remain.
Next extract bounded raw post-pair time confirmation before final commit,
preserving the shared deadline and allowlist. Retain all reports, PDF suppression,
PID retry and ADRs 0014/0027/0028. No commit or publication is authorized.
Older handoffs below are historical.

# Latest handoff: SN-017 native register-pair correlation complete

The twentieth cycle is validated; SN-017 remains in progress. Read latest CURRENT,
[ADR 0035](../decisions/0035-native-register-pair-correlation.md), the
[protocol](../../tests/headless/INSPECTION_REQUEST.md), and the
[evidence](../experiments/evidence/SN-017-inspection-request-summary.json).
`--native-inspection-request` issues two sequential fixed register requests with
distinct tokens under the original mailbox deadline. Six recovery sessions
passed; all 12 correlated replies matched GDB logs in order. The host still
enforces the 100 ms gap and owns GDB transport. Next extract bounded native
observation-interval gating without renewing that deadline. Preserve the two
failed test-setup reports and corrected isolated setup, all earlier evidence,
PDF suppression, PID retry, the allowlist and ADRs 0014/0027/0028. No commit or
publication is authorized. Older handoffs below are historical.

# Latest handoff: SN-017 native final register stability complete

The nineteenth cycle is validated; SN-017 remains in progress. Read latest CURRENT,
[ADR 0034](../decisions/0034-bounded-final-register-stability.md), the
[protocol](../../tests/headless/INSPECTION.md), and the
[evidence](../experiments/evidence/SN-017-inspection-summary.json).
`--native-inspection` compares the final r0/sp/lr records after timed readback
and requires confirmation before final commit. Six recovery sessions passed;
all 12 register records matched GDB logs. Host ownership of the two reads,
association and 100 ms observation interval remains; native equality alone
cannot prove distinct observations. Next extract bounded ownership/correlation
of the final register pair while retaining host GDB transport, the allowlist
and separate commit scheduling. Preserve all reports, PDF suppression, PID retry
and ADRs 0014/0027/0028. No commit or publication is authorized. Older handoffs
below are historical.

# Latest handoff: SN-017 raw elapsed-time validation complete

The eighteenth cycle is validated; SN-017 remains in progress. Read latest CURRENT,
[ADR 0033](../decisions/0033-native-raw-stopped-time-validation.md), the
[protocol](../../tests/headless/READBACK_TIME.md), and the
[evidence](../experiments/evidence/SN-017-readback-time-summary.json).
`--native-readback-time` validates raw elapsed-time and completion records with
the mailbox under one deadline, rejecting normalized timestamps. Six recovery
sessions passed, with 18 raw records matching GDB logs. MI output streams remain
untagged; host request association and GDB transport are still required. Next
extract bounded final inspection stability checks, retaining the allowlist and
separate commit scheduling. Preserve all reports, PDF suppression, PID retry and
ADRs 0014/0027/0028. No commit or publication is authorized. Older handoffs below
are historical.

# Latest handoff: SN-017 mailbox request correlation complete

The seventeenth cycle is validated; SN-017 remains in progress. Read latest CURRENT,
[ADR 0032](../decisions/0032-bounded-mailbox-request-correlation.md), the
[protocol](../../tests/headless/READBACK_REQUEST.md), and the
[evidence](../experiments/evidence/SN-017-readback-request-summary.json).
`--native-readback-request` arms one native final mailbox request, matches its
reserved MI token and enforces a 1900 ms acceptance deadline. Six recovery
sessions passed; all six request/reply pairs matched actual GDB log results.
Host GDB ownership and raw time decoding remain. Correlation is session-scoped;
tokens may repeat after recreation and do not authenticate the host. Next extract
bounded raw stopped-time validation for the mailbox request, retaining the GDB
allowlist and separate commit scheduling. Preserve all reports, PID retry, PDF
suppression and ADRs 0014/0027/0028. No commit or publication is authorized.
Older handoffs below are historical.

# Latest handoff: SN-017 native raw MI mailbox parsing complete

The sixteenth cycle is validated; SN-017 remains in progress. Read latest CURRENT,
[ADR 0031](../decisions/0031-native-raw-mi-mailbox-ingress.md), the
[protocol](../../tests/headless/READBACK_MI.md), and the
[evidence](../experiments/evidence/SN-017-readback-mi-summary.json).
`--native-readback-mi` sends the unchanged final GDB memory result to native
exact-format parsing and little-endian decoding. It rejects normalized bypass.
Six final recovery sessions passed, and all six raw lines matched GDB logs.
Host time association and the reused token still do not independently establish
freshness or origin. Next extract bounded mailbox request/result correlation,
retaining host GDB ownership, the allowlist and separate commit scheduling.
Preserve the initial reports predating strict timestamp parsing, all earlier
evidence, PID retry, PDF suppression and ADRs 0014/0027/0028. No commit or
publication is authorized. Older handoffs below are historical.

# Latest handoff: SN-017 native final mailbox verification complete

The fifteenth cycle is validated; SN-017 remains in progress. Read latest CURRENT,
[ADR 0030](../decisions/0030-bounded-native-mailbox-readback.md), the
[protocol](../../tests/headless/READBACK.md), and the
[evidence](../experiments/evidence/SN-017-readback-summary.json).
`--native-readback` verifies the final host-normalized mailbox against native
prepared ADC state before the final commit. Host GDB collection and stability
proof remain required; raw origin/freshness are not independently established.
Final recovery passed six sessions, 54 checkpoints and six native readbacks.
Next extract bounded raw readback ingress, retaining host GDB ownership, the
allowlist and separate commit scheduling. Preserve all evidence, including the
historical snapshot comparison failure and its explicit zero-diagnostic fix,
ADR 0014 capabilities, PID retry, PDF suppression and ADRs 0027/0028.
No commit or publication is authorized. Older handoffs below are historical.

# Latest handoff: SN-017 native RC analytical validation complete

The fourteenth cycle is validated; SN-017 remains in progress. Read latest CURRENT,
[ADR 0029](../decisions/0029-native-rc-trajectory-validation.md), the
[protocol](../../tests/headless/RC_TRAJECTORY.md), and the
[evidence](../experiments/evidence/SN-017-rc-trajectory-summary.json).
`--native-rc` validates advance replies before analog acceptance using native
high-command state. The original Python checks remain an independent oracle.
Final recovery passed six real sessions, 54 fixed checkpoints and 60 native RC
checks, with four fault controls and nine new pipe cases. The initial failed
producer-format report is preserved. Next select the smallest bounded native
readback-verification step; preserve the GDB allowlist and separate joint commit
scheduling. Retain all evidence, ADR 0014 capabilities, harness reference, PID
retry, PDF suppression and ADRs 0027/0028. No commit or publication is authorized.
Older handoffs below are historical.

# Latest handoff: SN-017 native ADC helper process complete

The thirteenth cycle is validated; SN-017 remains in progress. Read latest CURRENT,
[ADR 0026](../decisions/0026-native-adc-helper-process.md), the
[protocol](../../tests/headless/ADC_PROCESS.md), and the
[evidence](../experiments/evidence/SN-017-adc-process-summary.json).
`--native-adc-process` owns fixed helper invocation, bounded result ingress and
job cleanup before confirmation, sharing the preparation deadline. Six real
sessions and 21 process cases passed, with fault/recovery and reference controls.
Next extract bounded analytical RC validation into native coordination before
further GDB/commit scheduling. Preserve all evidence, ADR 0014 capabilities,
harness reference, PID retry and PDF suppression. No commit or publication is
authorized. Older handoffs below are historical.

# Latest handoff: SN-017 bounded ADC coordination complete

The twelfth cycle is validated; SN-017 remains in progress. Read latest CURRENT,
[ADR 0025](../decisions/0025-bounded-adc-input-coordination.md) and
[protocol](../../tests/headless/ADC_COORDINATION.md). `--native-adc` derives integer
microvolts from native analog state at 4010750 ns and gates matching confirmation
before commit. The host still invokes the existing ADC helper and supplies its
microsecond-resolution confirmation. Next extract bounded helper invocation and
result ingress. Preserve all evidence, capabilities, harness reference, PID retry
and PDF suppression. No commit or publication is authorized. Older handoffs are
historical.

# Latest handoff: SN-017 native high/inspection coordination complete

The eleventh cycle is validated; SN-017 remains in progress. Read latest CURRENT,
[ADR 0024](../decisions/0024-native-fixture-exchange-inspection.md) and
[protocol](../../tests/headless/EXCHANGE_INSPECTION.md). `--native-exchange` routes
the one measured GPIO high and stopped/analog-ready inspections through the native
worker channel, preserving snapshots before commit. Next extract bounded ADC
boundary-input coordination; retain host GPIO/GDB verification and separate joint
commit. Preserve all evidence, capabilities, harness reference, PID retry and PDF
suppression. No commit or publication is authorized. Older handoffs are historical.

# Latest handoff: SN-017 native analog reply ingress complete

The tenth cycle is validated; SN-017 remains in progress. Read latest CURRENT,
[ADR 0023](../decisions/0023-native-analog-reply-ingress.md) and
[protocol](../../tests/headless/ANALOG_INGRESS.md). `--native-analog-results` gives
the runner sole stdout ownership for initial, advance and inspection/exchange
replies. The host retains analytical checks, exchange, GDB verification and commit
scheduling. Next select the smallest bounded exchange/inspection coordination
step. Preserve all evidence, including unsuccessful test attempts, the harness
reference, ADR 0014 capabilities, bounded PID retry and PDF suppression. No commit
or publication is authorized. Older handoffs below are historical.

# Latest handoff: SN-017 native analog command slice complete

The ninth cycle is validated; SN-017 remains in progress. Read latest CURRENT,
[ADR 0022](../decisions/0022-native-analog-command-channel.md) and
[protocol](../../tests/headless/ANALOG_COMMANDS.md). `--native-analog` emits advance
through an inherited worker stdin using confirmed CPU time; replies remain
host-read. Next extract bounded analog reply ingress while keeping exchange,
inspection and commit separate. The first recovery hit transient PID-file access
denial; its failed report is preserved and startup reads now retry within the
original deadline. Preserve all evidence, capabilities, reference and PDF fix.
No commit or publication is authorized. Older handoffs below are historical.

# Latest handoff: SN-017 composite grant transition slice complete

The eighth cycle is validated; SN-017 remains in progress. Read the latest CURRENT
entry, [ADR 0021](../decisions/0021-composite-fixture-grant-transitions.md) and
[protocol](../../tests/headless/GRANT_TRANSITIONS.md). `--native-grants` combines
begin/start and observed-stop/cancel in the runner, using existing readers and
deadlines. Next extract bounded analog catch-up after CPU acknowledgement; keep
exchange, inspection and joint commit separate. Complete orchestration is pending.
Preserve the harness reference, all evidence, ADR 0014 capabilities and PDF
suppression. No commit or publication is authorized. Older handoffs are historical.

# Latest handoff: SN-017 native debug reply ingress complete

The seventh cycle is validated; SN-017 remains in progress. Read the latest
CURRENT entry, [ADR 0020](../decisions/0020-native-debug-reply-ingress.md) and
[ingress protocol](../../tests/headless/DEBUG_INGRESS.md). Complete progress and
notification replies now run natively behind `--native-debug-results`, with
Windows writer-closure checks and non-renewing phase deadlines. Next consolidate
the smallest coherent bounded orchestration step; preserve the harness reference,
all evidence, ADR 0014 capability restrictions and PDF suppression. No commit
or publication is authorized. Older handoffs below are historical.

# Latest handoff: SN-017 debug command emission slice complete

The sixth extraction cycle is validated; SN-017 remains in progress. Read the
latest CURRENT entry, [ADR 0019](../decisions/0019-native-debug-command-emission.md)
and [debug command protocol](../../tests/headless/DEBUG_COMMANDS.md). Progress and
joint-notification emission now run through the native control channel. Their
response files are still read by Python. Next extract complete-response ingress,
including non-atomic publication handling, before consolidating orchestration.
Preserve all evidence, bounded capabilities, the reference and PDF suppression.
No commit or publication is authorized. Older handoffs below are historical.

# Latest handoff: SN-017 native backend lifecycle slice complete

The fifth extraction cycle is validated; SN-017 remains in progress. Read the
latest CURRENT entry, [ADR 0018](../decisions/0018-native-fixture-process-lifecycle.md)
and [lifecycle protocol](../../tests/headless/PROCESS_LIFECYCLE.md). Native Windows
supervisors now create and reap both backend trees on the opt-in path. Real
engine/supervisor loss and fresh recovery passed. Next extract the remaining
bounded progress/joint-notification commands before native orchestration.
Preserve the harness reference, all evidence, capability limits and PDF startup
suppression. No commit or publication is authorized. Older handoffs are historical.

# Latest handoff: SN-017 native command/ready slice complete

The fourth extraction cycle is validated; SN-017 remains in progress. Read the
latest CURRENT entry, [ADR 0017](../decisions/0017-native-cancellation-channel.md)
and [command protocol](../../tests/headless/COMMAND_CHANNEL.md). Start/cancel,
ready/error and result ingress now run in C++ on the opt-in path. Real-engine
fault/fresh-recovery and native pipe tests passed. Next extract fixed-fixture
process lifecycle, preserving the harness reference, all evidence, capability
limits and PDF suppression. No commit or publication is authorized. Older
handoffs below are historical.

# Latest handoff: SN-017 native result-ingress slice complete

The third extraction cycle is validated; SN-017 remains in progress. Read the
latest CURRENT entry, [ADR 0016](../decisions/0016-bounded-cancellation-ingress.md)
and [ingress protocol](../../tests/headless/RESULT_INGRESS.md). Native parsing,
real Windows read locks and one result deadline passed real-engine and fault
checks. Next extract cancellation commands and ready/error handshake under a
single phase deadline, then process ownership and orchestration. Keep the
experimental reference, prior evidence, capability limits and PDF suppression.
No commit or publication is authorized. Earlier handoffs below are historical.

# Latest handoff: SN-017 CPU/joint-state slice complete

The second extraction cycle is validated; SN-017 remains in progress. Read the
latest CURRENT entry, [ADR 0015](../decisions/0015-incremental-session-contract.md)
and [session protocol](../../tests/headless/SESSION_CONTRACT.md). The opt-in C++
state gate ran against real engines with fault/fresh-recovery checks. Next extract
Renode result parsing and bounded transport/deadline ownership. Preserve the
experimental reference, prior evidence, capability limits and PDF suppression.
No commit or publication is authorized. Earlier handoffs below are historical.

# Latest handoff: SN-017 first slice complete

Updated: 2026-09-10 after the RC extraction cycle. SN-016 remains done for ADR
0014; SN-017 is now in progress. Read the latest CURRENT entry and the
[extraction protocol](../../tests/headless/README.md), including reproduction
commands and [evidence](../experiments/evidence/SN-017-rc-extraction-summary.json).
The extracted C++20 persistent RC runner passed real Renode/ngspice/GDB,
differential boundary and fault/fresh-recovery checks. Next extract CPU
grant/result accounting and joint-state transitions; the joint orchestration
still lives in the experimental harness. Preserve all earlier evidence, pacing
restrictions and PDF suppression. No commit or publication is authorized.

The preceding E-05 handoff follows as historical context; its statements that
SN-017 has not started are superseded by this entry.

# SimNodus conversation handoff

Updated: 2026-09-10. The owner requested a completed-cycle handoff and a prompt
for a new conversation in this same project. No new task, commit or remote
publication was requested.

## Workspace and rules

Actual repository: `D:/03_Projects/01_Actives/SimNodus`. Keep repository text
in English and owner communication in Portuguese. Read AGENTS, README,
[CURRENT](CURRENT.md), architecture and relevant decisions before editing.
Preserve existing uncommitted work and every historical failed-run record.
No milestone dates were advanced; January remains stabilization and February
2027 remains the classroom target.

## Completed gate

**SN-016 is done for the declared bounded experimental profile. SN-017 is ready
and has not started.** [ADR 0014](../decisions/0014-bounded-cooperative-debugging.md)
records the capability/command/lifecycle decision. The
[case-by-case E-05 review](../experiments/E-05-gate-review.md) maps the original
acceptance cases to measured evidence.

The final direct-GDB runner now supports `--guarded` with `--steps --lifecycle
--pause`. All traffic uses CooperativeGuard and the actual MI interrupt is
retained instead of forwarded raw. Three repetitions passed six sequences,
54 fixed checkpoints and six joint pauses. Acknowledgement ranged from 41.5932
to 91.5682 ms, with stable inspection, ADC code 3541, recreated zero-state
detach/reattach and full process/listener cleanup.

The earlier final IDE matrix passed 35 sessions: three arbitration
fault/recovery pairs per fault (analog loss, live response timeout, CPU loss,
debugger loss), four continuously paced pairs, extended lifecycle, shorter
arbitration and exact default lifecycle controls. Initial pacing is explicitly
released only after retained MI interruption and pre-ADC observation.
Fully unpaced behavior is still unapproved. CPU acknowledgement is never a joint
commit; reset is full recreation; active failure requires fresh processes.

The final cycle also passed three IDE startup controls, three original four-stop
GDB regressions with exact reference comparison, and all 14 relay/accounting tests.

Evidence:
- [Final gate](../experiments/evidence/E-05-final-gate-summary.json).
- [35-session matrix](../experiments/evidence/E-05-steps-fault-matrix-summary.json).
- [Shared-bridge direct regression](../experiments/evidence/E-05-steps-fault-plain-regression-summary.json).
- [Final protocol](../../tests/experiments/debugging/FINAL_GATE.md).

The raw partial flags remain unchanged; the bounded gate result composes measured
subprofiles. Do not relabel historical failures or claim arbitrary workloads,
physical ADC acquisition, solver nonconvergence recovery, mouse-driven UI
coverage, rollback or production readiness.

## PDF startup annoyance fixed

Each disposable IDE configuration previously opened release notes
`DM00603738.pdf`. Inspection of the installed ReleaseNotesOpener identified its
ConfigurationScope already-seen preference. The driver now gets the active
documentation bundle version from bundles.info and seeds major.minor.micro=true
before startup; the owned startup plugin verifies it. The current key is
`2.3.500`. Three real IDE controls passed with this preference.

Keep this preparation when creating future IDE test configurations. The installed
IDE and existing PDF/browser windows were not changed; Defender scanning remains
enabled. This fix suppresses automatic startup opening, not access to documentation.

## Next bounded work: SN-017

1. Read ADR 0014, the gate review and SN-017 acceptance criteria before selecting
   the smallest extraction slice.
2. Begin a C++20 headless fixture runner and explicit backend contracts. Keep Qt
   out of the kernel and third-party types behind adapters. Preserve granted,
   observed, CPU-acknowledged and jointly committed state, unused-time accounting,
   pacing capability restrictions and fresh-session failure recovery.
3. Reuse the owned firmware, persistent RC fixture and pinned engine artifacts.
   Verify each extracted slice against real engines and invariant/adversarial
   cases; fake backends alone are insufficient. Keep experiments available as
   references. Do not build a broad GUI, SDK or generalized component layer to
   avoid the integration work.
4. Update CURRENT/backlog with actual implementation and test evidence. Add an
   ADR for any significant new design choice. Stop at a coherent completed cycle
   and report a clear handoff boundary.

No production simulator has been implemented by closing the experiment gate.
Local task status is updated; remote issue synchronization and publication were
not performed and must not be inferred as authorized.

## Reproduction and local inputs

Use a fresh directory for each run; never overwrite earlier evidence.

```powershell
python tests/experiments/debugging/joint_persistent.py --renode build/sn016/source-notification --ide C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE --native build/sn016/persistent-native/Debug --steps --lifecycle --pause --guarded --output build/sn016/<fresh-gdb-directory>
python -m unittest discover -s tests/experiments/debugging -p 'test_*.py'
python tools/check_repository.py
git diff --check
```

Experimental managed Renode: `build/sn016/source-notification`; native workers:
`build/sn016/persistent-native/Debug`; ngspice 47:
`build/deps/ngspice/Spice64_dll/dll-vs`. Keep the
[managed-build recipe](../../tests/experiments/debugging/SOURCE_BUILD.md).
The installed CubeIDE reports 2.2.0 despite its directory name; bundled GDB
reports 15.2.90.20241229. Exact hashes are in the evidence.

Latest raw runs: `final-guarded-gdb-01`, `final-plain-control-01`,
`no-release-notes-joint-01`, `no-release-notes-joint-02`,
`no-release-notes-default-01` under `build/sn016`.
Do not rerun the 35-session IDE matrix for documentation-only changes; choose
regressions that exercise the changed behavior.
