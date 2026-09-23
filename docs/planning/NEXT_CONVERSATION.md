# Latest handoff: SN-021 observed transaction isolation

Read CURRENT, BACKLOG, ADR 0064 and the [TxF report](../experiments/SN-021-txf-boundary.md).
Six disposable fixture cases observed transaction-level exclusion across writer
handle close, commit/rollback, stale identity/bytes and existing writer/mapping
conflicts. Eight separate competitor write/rename attempts rejected. The final
probe pins all ancestors; two earlier ordinary-path probes claim no containment.
No production API, TxF dependency or supported-platform expansion was selected.

Microsoft recommends alternatives to a new TxF dependency. The owner explicitly
selected continued research without TxF, keeping SN-021 open and overwrite in its
acceptance scope. Do not adopt TxF or reduce completion to create-only persistence.
Keep create-only persistence supported while investigating a maintained protocol. A product protocol still needs identity/version authority,
physical failure coverage, host support and uncertain-commit handling. No crash or
power-loss acceptance is claimed. Do not repeat ADR 0064's rejected pathname races.

Preserve 106 historical evidence files, both preliminary probes, twelve local SN-045
files, SN-044, instrumentation, MCU/toolchain independence, SN-017 Python/GDB/fixture
ownership, all profiles/tolerances and PDF/PID. No UI, downloads, issues or release.
Next-cycle prompt awaits final acceptance; none is prepared for an incomplete SN.
Base main: `5b2cf26aa6fa1564410bec8368e7e619936dffa3` (PR #46); branch
`codex/sn-021-txf-boundary`. Confirm final hosted checks, squash and main identity.

# Historical handoff: SN-021 configured replay lifecycle

Read CURRENT, BACKLOG, the [lifecycle report](../experiments/SN-021-replay-lifecycle.md)
and [acceptance matrix](../experiments/SN-021-acceptance.md). Existing native create/
acquire/name-edit/save-copy/reacquire now compose with explicit fixed replay in the
test-only probe. Nineteen Windows cases and three targeted CTests passed. Real
ngspice consumed the post-reopen artifact, matching pre-save compilation, with
5012 samples within unchanged 10 microvolt/1 ps bounds. Name-only bytes, temporal
policy/schedule and revised source maps were checked. Production APIs are unchanged.

Next address safe overwrite: read ADR 0064 and its physical counterexamples. Define
a candidate expected-identity/version ownership and concurrency protocol before
implementation. Do not repeat check-then-replace, advisory-lock-only or path-prefix
proofs. Create-only save/copy remains supported. After that gate, perform final
bounded SN-021 acceptance and prepare the next-cycle prompt. Other runtime modes
remain unsupported; do not add UI, arbitrary models, new MCUs or native-only GDB
orchestration as implicit completion requirements.

Preserve 105 historical evidence files, prior negative runs and twelve local SN-045
files, SN-044, shared instrumentation, MCU/toolchain independence, SN-017 Python/
GDB/fixture ownership, tolerances and PDF/PID. No downloads, issues or release.
SN-021 remains in progress. Base main: `aaf0760f5fa508b174e8c6ccc9835500d04c6936`
(PR #45); branch `codex/sn-021-replay-lifecycle`. Confirm final hosted checks,
squash and local/remote main identity before continuing.

# Historical handoff: SN-021 fixed configured RC replay

Read CURRENT, BACKLOG, [ADR 0075](../decisions/0075-fixed-e01-project-replay.md), the
[contract](../architecture/FIXED_RC_REPLAY.md) and [report](../experiments/SN-021-fixed-replay.md).
A separate explicit native operation binds original configured replay metadata
and exact captured schedule to the existing ideal RC compiler. One 5 ms interval,
known 3.3 V drive and disabled debugging only; existing standalone gates remain.
All flags stay false. No compilation/open path launches an engine.

Eight cases (seven Windows passes/one skip), 50 Windows CTests and real ngspice
passed. The first engine attempt swapped fixture/output arguments; preserve its
failure and the corrected fresh run. The successful run produced 5012 samples,
9.889724283951296e-08 V maximum error and endpoint within 1 ps. Original model/
schedule were replaced after capture; staged artifact writes/rename were denied.

Next compose configured acquisition/edit/save-copy/reopen with this same replay,
then address ADR 0064 safe-overwrite ownership without repeating rejected pathname
races. Other runtime modes remain unsupported; do not infer mixed-signal or GDB
compatibility from this analog-only binding. Preserve SN-017 Python/GDB/fixture
ownership, SN-044, instrumentation, MCU independence, profiles/tolerances, PDF/PID,
104 historical evidence files and twelve local SN-045 files. No UI, downloads,
issues or release. SN-021 remains in progress; prompt for the next cycle awaits
full acceptance. Consult the updated acceptance matrix rather than older next-steps.

Base main: `c6faf05dd7da55169f232f9888c4ece8ee761570` (PR #44); branch
`codex/sn-021-fixed-replay`. Verify final PR/check/squash and local/remote main.

# Historical handoff: SN-021 acceptance gates

Read the [acceptance matrix](../experiments/SN-021-acceptance.md), CURRENT and BACKLOG.
The matrix maps remaining work; earlier slice next-steps are historical. Loading,
independent graph/source mapping, create-only persistence, explicit ideal RC
compilation and isolated reference boot have bounded implementation and real evidence.
Configured project/runtime correspondence and safe overwrite remain pending.

Two targeted CTests passed 48 new configured-policy rejection requests. Native code
and engine inputs are unchanged; prior real results are reused. Preserve 103 historical
evidence files, 14 schema fixtures and twelve local SN-045 files. An audit correction
for a superseded CMake hash is recorded. Hosted checks belong to the branch PR.

Next predeclare exact project-policy/input mapping for one already measured fixture,
positive real consumption and mismatch acceptance. Do not substitute E-03 tolerances,
infer execution from labels/hashes or invent a general schedule format. Keep SN-017
Python preparation/GDB/fixture ownership. Safe overwrite must satisfy ADR 0064,
not repeat rejected pathname protocols. The matrix requires no UI, new MCUs/models
or all-native orchestration. Preserve SN-044, instrumentation, MCU independence,
profiles, PDF/PID and historical negatives. No downloads, issues or release.
SN-021 stays in progress; prepare the next-cycle prompt after full bounded acceptance.

Base main: `04f0174e922ca87db85213d9d184692b276862d8` (PR #43); branch
`codex/sn-021-acceptance-gates`. Confirm final PR/check/squash and main identity.

# Historical handoff: SN-021 reference target and real boot

Read CURRENT, BACKLOG, [ADR 0074](../decisions/0074-explicit-reference-target-boot.md),
the [contract](../architecture/REFERENCE_TARGET.md) and [report](../experiments/SN-021-reference-target.md).
Native explicit target/platform/firmware association remains inert. A separate
Python experiment consumed owned snapshots through pinned staging and passed real
Renode E-02 assertions for both existing 100/1000 us profiles. Seven gate cases
(six Windows passes/one skip), 49 Windows CTests. Preserve the initial fixture
failure, all 102 earlier evidence files and twelve unrelated local SN-045 files.

Next audit the remaining SN-021 acceptance matrix before implementing another
slice: configured temporal/runtime correspondence and safe overwrite (ADR 0064)
remain pending. Isolated reference boot does not implement arbitrary project
execution or electrical wiring. Preserve SN-017 Python preparation/GDB/fixture
control, SN-044, instrumentation, MCU/toolchain independence, numerical profiles,
PDF/PID and all historical negatives. No UI, downloads, issues or release.
SN-021 is in progress; prepare the next-cycle prompt only after full acceptance.

Base main: `fe32b2591ec4e5d611082684a116a2f5e6e82908` (PR #42); branch
`codex/sn-021-target-boot`. Confirm final hosted checks, squash and main identity.

# Historical handoff: SN-021 static reference boot candidate

Read CURRENT, BACKLOG, [ADR 0073](../decisions/0073-static-boot-candidate.md), the
[contract](../architecture/BOOT_CANDIDATE.md) and [report](../experiments/SN-021-boot-candidate.md).
An explicit adapter-local SN-012 profile now checks ELF/header, memory-region,
overlap, stack reserve and reset/vector correspondence. Eight cases and 48 Windows
CTests passed. The historical image passed read-only hash/symbol comparison;
program order and `load_ordered: false` are preserved. No image was loaded and
no static result grants device/runtime/section/instruction or execution approval.

Next bind the explicitly requested profile to project target/platform identity
and trusted fixture resources, then use the existing real Renode fixture to prove
loader/boot integration. Keep Python preparation/GDB/fixture scheduling in SN-017.
Do not infer a profile, normalize load order, reopen verified paths or broaden
accepted runtime/electrical profiles. Safe overwrite remains pending ADR 0064.
Preserve twelve local SN-045 files, historical negatives, SN-044, instrumentation,
MCU/toolchain independence, tolerances and PDF/PID. No UI, downloads, issues or
release. SN-021 is in progress; next-cycle prompt awaits full acceptance.

Base main: `e5306affc1c8a892c22096383d7f466238e9246c` (PR #41); branch
`codex/sn-021-boot-candidate`. Confirm final PR/check/squash and main identity.

# Historical handoff: SN-021 selected firmware capture

Read CURRENT, BACKLOG, the [contract](../architecture/FIRMWARE_INSPECTION.md) and
[report](../experiments/SN-021-firmware-inspection.md). Explicit native composition
now validates project metadata, selects firmware by ID, physically captures all
locked resources and inspects retained ELF bytes. Nine cases (eight Windows passes,
one platform skip), 47 CTests and historical real compiler-output capture passed.
The returned bytes survive replacement of the copied file. All readiness stays
false; declared architecture and observed machine/ABI/flags remain separate.

Next define architecture/device/boot correspondence for the owned fixture before
real target/runtime integration. Do not infer compatibility from labels, hashes
or ELF numbers; do not reopen verified paths, map firmware or normalize unordered
segments. Preserve ADR 0072's diagnostic and prior failed attempt. Safe overwrite
remains pending ADR 0064; full SN-021 is in progress. Prepare the next-cycle prompt
only upon full acceptance. Preserve twelve local SN-045 files, historical evidence,
SN-044, instrumentation, MCU/toolchain independence, SN-017 Python/GDB ownership,
accepted profiles/tolerances and PDF/PID. No UI, downloads, issues or release.

Base main: `8122767d58d57570740c9bc948432b161b0e7e65` (PR #40); branch
`codex/sn-021-firmware-capture`. Confirm final PR/check/squash and main identity.

# Historical handoff: SN-021 inert ELF inspection

Read CURRENT, BACKLOG, [ADR 0072](../decisions/0072-inert-elf-inspection.md), the
[contract](../architecture/ELF_INSPECTION.md) and [report](../experiments/SN-021-elf-inspection.md).
The pure reader owns ELF32 little-endian bytes and bounded program records with
raw architecture/ABI/entry/flags and source offsets. Twelve adversarial cases and
46 Windows CTests passed. Historical SN-012 compiler-output comparison passed;
retain the first load-order failure and the explicit final `load_ordered: false`
diagnostic. Success is not full ELF, memory-map, ABI, boot or runtime validation.
No firmware is loaded and no project-opening path invokes the reader implicitly.

Next define selected firmware/resource association using retained physical byte
snapshots and explicit architecture evidence. Do not reopen verified paths or
infer MCU support from machine numbers. Device/boot validation and real target
integration remain pending, as does overwrite ownership under ADR 0064.
SN-021 stays in progress; prepare the next-cycle prompt only upon full acceptance.
Preserve historical/negative evidence, twelve local SN-045 files, SN-044,
instrumentation, SN-017 Python/GDB/fixture ownership, MCU/toolchain independence,
accepted numerical/runtime profiles and PDF/PID. No UI, downloads, issues or release.

Base main: `922f364c4464aa191cb0be214f2e0f67c65ba15a` (PR #39); branch
`codex/sn-021-elf-inspection`. Confirm final PR/check/squash and main identity.

# Historical handoff: SN-021 project lifecycle acceptance

Read CURRENT, BACKLOG and the [lifecycle report](../experiments/SN-021-project-lifecycle.md).
Eight Windows cases and 45 CTests passed; two explicit real ngspice runs validated
the reopened artifact with unchanged tolerances. Existing APIs now have combined
create/open/name-edit/save-copy/reopen/compile evidence; no production API changed.
SN-021 is still in progress. Safe overwrite remains pending ADR 0064. Next define
bounded inert ELF inspection of physically captured resource bytes, including
limits, address/segment overflow and unsupported input. Do not infer boot/device
compatibility, trusted origin, licensing or execution permission from valid bytes.
No firmware load, broader engine profile or automatic execution is authorized by
opening a project. Keep SN-017 preparation/GDB/fixture scheduling in Python.
Preserve all historical/negative evidence, twelve local SN-045 files, SN-044,
instrumentation, MCU/toolchain independence, numerical tolerances and PDF/PID.
Prepare the next-cycle prompt only after full SN-021 acceptance.

Base main: `a5f0c53a82092b12ecc086ddcc8639d3feb66e90` (PR #38). Branch:
`codex/sn-021-project-lifecycle`. Confirm final PR/check/squash and main identity.

# Historical handoff: SN-021 explicit ideal RC compilation

Read CURRENT, BACKLOG, [ADR 0071](../decisions/0071-explicit-ideal-rc-compilation.md),
the [contract](../architecture/IDEAL_RC_COMPILATION.md) and
[report](../experiments/SN-021-ideal-rc-compilation.md). Complete project validation,
structural connectivity and physical snapshots now feed an explicit fixed E-01
compiler with root reference/drive/output selection and full source maps. Only
1 kohm/1 uF, three-node standalone RC is accepted; no configured co-simulation.
The returned artifact is inert and all declaration readiness flags stay false.

Eight cases: seven Windows passes/one platform skip; all 44 Windows CTests passed.
Real pinned ngspice produced 5012 samples and 9.889724283951296e-08 V maximum error
under unchanged E-01 and fixture tolerances. Postcompile copied-model replacement
rejected on recompilation; the owned artifact ran without path reopening.

Next review remaining SN-021 project/runtime acceptance and safe overwrite
ownership under ADR 0064 before selecting the next bounded slice. Do not expand
electrical profiles or implicitly connect opening a project to compilation/run.
Preserve twelve local SN-045 files, historical/negative evidence, SN-044,
instrumentation, SN-017 Python/GDB/fixture ownership, MCU/toolchain independence,
tolerances and PDF/PID. No UI, downloads, issues or binary release.

Base main: `edbb87fa4182f7b5c27174936b1fef832f177c7c` (PR #37); branch:
`codex/sn-021-ideal-rc-compile`. Confirm final PR/head/check/squash and remote.
Full SN-021 remains in progress; prepare the next-cycle prompt upon completion.

[PR #38](https://github.com/RicardoKers/SimNodus/pull/38), starting at source
`d605827`, records final required checks and authorized protected-main squash.
Confirm final state and main identity before reviewing remaining acceptance gates.

Preserve the initial Linux root-versus-platform test failure and its correction
[evidence](../experiments/evidence/SN-021-ideal-rc-ci-correction.json). Only the test
expectation changed; final source hashes are recorded in that supplemental audit.

Also preserve the subsequent Windows fixture-root failure and the
[root correction audit](../experiments/evidence/SN-021-ideal-rc-root-correction.json).
It contains the final source hashes; no production/path-policy relaxation occurred.

# Historical handoff: SN-021 exact passive numerical binding

Read CURRENT, BACKLOG, [ADR 0070](../decisions/0070-passive-numeric-binding.md),
the [contract](../architecture/PASSIVE_NUMERIC_BINDING.md) and
[report](../experiments/SN-021-passive-numeric.md). Source defaults now convert
exactly and must be positive/in-range even if overridden or unused. Reachable
component effective values and ordered logical pins retain paths and provenance.
Eight cases, 40 Decimal comparisons and 43 Windows CTests passed. All readiness
flags remain false; no netlist, engine or physical acquisition occurs here.

Next define explicit reference/stimulus/analysis authority and the smallest bounded
backend lowering with complete project validation, structural connectivity and
physically captured resources. Obtain real-engine acceptance without broadening
profiles. Mathematical values do not certify binary solver rounding or stability.
Safe overwrite remains pending ADR 0064; no automatic execution or path reopening.

Base main: `8e4c3e2850bc73753fd3a600676c0a235a351b09` (PR #36); branch:
`codex/sn-021-passive-numeric`. Confirm final PR/head/check/squash and remote.
Preserve twelve local SN-045 files, all historical/negative evidence, SN-044,
instrumentation, SN-017 Python/GDB/fixture ownership, MCU/toolchain independence,
numerical tolerances and PDF/PID behavior. No UI, downloads, issues or release.
Full SN-021 remains in progress; prepare the next-cycle prompt upon completion.

[PR #37](https://github.com/RicardoKers/SimNodus/pull/37), starting at source
`3f67324`, records final required checks and authorized protected-main squash.
Confirm final state and main identity before analysis authority/backend lowering.

# Historical handoff: SN-021 passive interface correspondence

Read CURRENT, BACKLOG, [ADR 0069](../decisions/0069-passive-interface-correspondence.md),
the [contract](../architecture/PASSIVE_INTERFACE.md) and
[report](../experiments/SN-021-passive-interface.md). Selected descriptors now
match owned recognized R/C bytes by exact resource size/hash, entrypoint, explicit
ordered terminal/parameter maps and primitive unit. All readiness flags stay false.
Eight cases and 42 Windows CTests passed; this operation is pure and inert.

Next select exact effective-value/range binding; source defaults are not yet
range-certified. Define reference/stimulus/analysis authority before full backend
lowering. Physical containment remains separate; consume retained captured bytes,
never reopen a verified pathname. Preserve the existing real RC evidence without
claiming a wider profile. Safe overwrite remains pending ADR 0064.

Base main: `0f9cf8d665ad2abfdf146c600592277b2bd9fcb0` (PR #35); branch:
`codex/sn-021-passive-interface`. Confirm final PR/head/check/squash and remote.
Preserve twelve local SN-045 files, all historical hashes and negative evidence,
SN-044, instrumentation, SN-017 Python/GDB/fixture ownership, MCU/toolchain
independence, numerical tolerances and PDF/PID behavior. No UI, automatic execution,
downloads, issue synchronization or release. Full SN-021 remains in progress;
prepare the next-cycle prompt only after full acceptance and integration.

[PR #36](https://github.com/RicardoKers/SimNodus/pull/36), starting at source
`57c32ce`, records final required checks and authorized protected-main squash.
Confirm the final state and main identity before numerical binding work.

# Historical handoff: SN-021 bounded passive source inspection

Read CURRENT, BACKLOG, [ADR 0068](../decisions/0068-passive-source-inspection.md),
the [contract](../architecture/PASSIVE_SPICE_SOURCE.md) and
[report](../experiments/SN-021-passive-source.md). The pure reader owns recognized
R/C source/interface spans and rejects unsupported constructs under fixed budgets.
Seven cases and 41 Windows CTests passed. The explicit pinned ngspice E-01 run
used physically captured owned fixture bytes: 5012 samples, maximum error
9.889724283951296e-08 V, unchanged tolerances. Preserve the failed first harness
attempt and all historical evidence. No automatic engine/resource execution.

Next select descriptor/parameter binding for recognized models with ordered maps,
units/ranges and exact effective values; explicitly define reference/stimulus/
analysis authority before backend lowering. No complete interface flag or runtime
readiness is granted yet. Safe overwrite remains pending ADR 0064. Do not expand
profiles, change SN-017 Python/GDB/fixture ownership, implement UI or other SNs.
Preserve twelve local SN-045 changes, SN-044, instrumentation, numeric tolerances,
PDF/PID and MCU/toolchain independence. No downloads, issues or binary release.

Base main: `de8e1438cf9b51101465d64f83d5e0451e8777f3` (PR #34); branch:
`codex/sn-021-passive-source`. Confirm final PR/head/check/squash and actual remote.
Full SN-021 stays in progress; prepare the next-cycle prompt only upon completion.

[PR #35](https://github.com/RicardoKers/SimNodus/pull/35), starting at source
`68360ac`, records final required checks and protected-main squash integration.
Confirm its final state and main identity before descriptor/parameter binding.

# Historical handoff: SN-021 structural connectivity compilation

Read CURRENT, BACKLOG, [ADR 0067](../decisions/0067-connectivity-compilation.md),
the [contract](../architecture/CONNECTIVITY_COMPILATION.md) and
[report](../experiments/SN-021-connectivity-compilation.md). The structural compiler
expands reachable occurrences and partitions only explicit connections. It retains
singleton terminals, source-net aliases and full original provenance/metadata.
No backend netlist, implicit ground or runtime approval is produced.

Acceptance: 10 cases/18 requests and 40 Windows CTests. The 65536-record operation
budget does not change declaration validity. Next select the smallest supported
model/interface and parameter lowering from verified captured resources, with
explicit reference/stimulus/analysis authority and required real-engine evidence.
Do not infer source interfaces from declared maps or adopt fixture model text as
an accepted electrical profile. Safe overwrite remains pending ADR 0064.

Base main: `3b377e39ecb76f9941cc64872c23e6b0509ad513` (PR #33); branch:
`codex/sn-021-connectivity-compilation`. Check final PR/head/check/squash and actual
remote state. Preserve twelve local SN-045 changes, all historical evidence,
SN-044, instrumentation, numerical tolerances, PDF/PID, MCU/toolchain independence
and SN-017 Python preparation/GDB/fixture ownership. No UI, automatic resource
execution, downloads, issue synchronization or release. Full SN-021 remains in
progress; prepare the next-cycle prompt only after full acceptance/integration.

[PR #34](https://github.com/RicardoKers/SimNodus/pull/34), starting at source
`aa8cd9f`, records final required checks and protected-main squash integration.
Confirm its final state and main identity before model/interface lowering work.

# Historical handoff: SN-021 source-preserving display-name revision

Read CURRENT, BACKLOG, [ADR 0066](../decisions/0066-project-name-revision.md),
the [contract](../architecture/PROJECT_REVISION.md) and
[report](../experiments/SN-021-revision.md). Renaming changes only the top-level
display-name token after validating the base, then revalidates/rebuilds the graph
and provenance. Semantic no-ops preserve every original byte. No I/O or save
authority is granted. Acceptance: 11 cases/33 requests and 39 Windows CTests.

Next define compiler ingress/readiness and the smallest lowering contract with
stable source maps under existing graph/resource/profile constraints. Obtain
real-engine evidence for integration claims. Further editor features are not
selected. Safe overwrite remains pending ADR 0064's destination ownership gate.
Do not infer runtime readiness from metadata or a revised source graph.

Base main: `8a60f6bc870b2a277749d24f309204cca2dbed7f` (PR #32); branch:
`codex/sn-021-project-rename`. Check final PR/head/check/squash and actual remote
state. Preserve twelve local SN-045 changes, historical evidence, SN-044,
instrumentation, numeric tolerances, PDF/PID, MCU/toolchain independence and
SN-017 Python preparation/GDB/fixture ownership. No UI, automatic resource
execution, downloads, issue synchronization or release. Full SN-021 remains
in progress; prepare the next-cycle prompt only after full acceptance/integration.

[PR #33](https://github.com/RicardoKers/SimNodus/pull/33), starting at source
`6d4f7cf`, records final required checks and protected-main squash integration.
Confirm its final state and main identity before compiler contract work.

# Historical handoff: SN-021 native project acquisition

Read CURRENT, BACKLOG, [ADR 0065](../decisions/0065-project-acquisition.md),
the [contract](../architecture/PROJECT_ACQUISITION.md) and
[report](../experiments/SN-021-acquisition.md). Acquisition captures one selected
document through retained Windows/NTFS handles, then validates/loads its owned
graph. No declared resources open, and no pathname lease or overwrite authority
is returned. The post-close replacement test preserves the original captured graph.

Acceptance: 17 cases, 15 local passes/two explicit skips; all 38 Windows CTests.
Next define minimal source-preserving revision/edit semantics with revalidation
and stable IDs/provenance on owned documents. Safe overwrite remains pending
ADR 0064's identity/version gate. Compilation needs separate real-engine evidence.
Do not weaken the concurrency contract or infer runtime readiness from metadata.

Base main: `f483fded668f589dbc19c4f684070bbbce775347` (PR #31); branch:
`codex/sn-021-project-acquisition`. Check final PR/head/check/squash and actual
remote state. Preserve twelve local SN-045 changes and all historical evidence,
SN-044, instrumentation, numerical bounds, PDF/PID, MCU/toolchain independence
and SN-017 Python preparation/GDB/fixture ownership. No UI, resource execution,
downloads, issue synchronization or release. Full SN-021 is still in progress;
prepare the requested next-cycle prompt only after full acceptance/integration.

[PR #32](https://github.com/RicardoKers/SimNodus/pull/32), starting at source
`87e23cd`, records final required checks and protected-main squash integration.
Confirm its final state and main identity before source revision work.

# Historical handoff: SN-021 overwrite ownership boundary

Read CURRENT, BACKLOG, [ADR 0064](../decisions/0064-overwrite-ownership-boundary.md)
and the [report](../experiments/SN-021-overwrite-boundary.md). Seven physical NTFS
counterexamples reject unlocking/checking then replacing, POSIX handle-as-path
ownership and a two-rename gap. All 37 local Windows CTests passed. No production
overwrite API was added; create-only saving remains the supported operation.

Next implement bounded native acquisition of one selected project document using
retained physical handles and an owned byte capture, then the existing validator/
graph loader. This must not produce pathname ownership, overwrite authority or
resource/interface/runtime approval. Preserve limits and reject reparse/alias
paths. Safe overwrite remains pending a proven identity/version-bound protocol;
do not repeat check-then-replace variants or silently weaken the concurrency model.
Editing and source-preserving compilation remain pending; engine claims require
real-engine evidence. Full SN-021 is not complete.

Base main: `53755a0ca232db45c17e4a16d37d96228d11942f` (PR #30); branch:
`codex/sn-021-overwrite-boundary`. Check final PR/head/check/squash and actual
remote state. Preserve all twelve local SN-045 changes, all historical evidence,
SN-044, instrumentation, MCU/toolchain independence, numeric bounds, PDF/PID
behavior and SN-017 Python preparation/GDB/fixture ownership. No UI, downloads,
resource execution, issue synchronization or release is selected. Prepare the
requested next-cycle prompt only after full SN-021 acceptance and integration.

[PR #31](https://github.com/RicardoKers/SimNodus/pull/31), starting at source
`f19e901`, records final required checks and protected-main squash integration.
Confirm its final state and main identity before native acquisition work.

# Historical handoff: SN-021 atomic creation of project documents

Read CURRENT, BACKLOG, [ADR 0063](../decisions/0063-atomic-project-create.md),
the [contract](../architecture/ATOMIC_PROJECT_CREATE.md) and
[report](../experiments/SN-021-save-create.md). Explicit create-only persistence
validates project 0.1 and publishes its exact owned bytes using an exclusive
temporary, flush and handle-relative native rename with replacement disabled.
Existing/concurrent entries are never overwritten. No resource is imported/executed.

Acceptance: 25 boundary checks, 14 physical cases (13 local passes/one symlink
privilege skip), all 36 Windows CTests. Read the audit for initial Win32 rename
and SDK declaration failures, subsequent native correction and preserved hashes.
Killed writers may leave orphan temporaries; no power-loss recovery is promised.

SN-021 remains in progress. Next define/prove safe overwrite with destination
ownership and concurrent-change protection, then path acquisition/editing and
source-preserving compilation with required real-engine evidence. Never treat
resolve-then-open or textual prefixes as containment/overwrite ownership proof.
Keep physical bytes, source/firmware interfaces, trust and execution independent.

Base main: `6d88455be400520d918c26406b210ad062567976` (PR #29); work branch:
`codex/sn-021-atomic-create`. Verify final PR/head/check/squash and actual remote
state before proceeding. Preserve all twelve local SN-045 changes, including
shared planning edits. Keep SN-044, instrumentation, MCU/toolchain independence,
numeric bounds, PDF/PID behavior and SN-017 Python/GDB/fixture ownership unchanged.

Prepare the requested new-chat prompt only when full SN-021 is accepted and
integrated. No downloads, automatic resource execution/rendering, UI, other SN,
issue synchronization, releases or binaries are selected.

[PR #30](https://github.com/RicardoKers/SimNodus/pull/30), starting at source
`ed50b39`, records final required checks and protected-main squash integration.
Confirm its final state and main identity before overwrite work.

# Historical handoff: SN-021 native source connectivity graph

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
