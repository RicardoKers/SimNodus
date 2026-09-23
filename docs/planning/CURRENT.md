# Current state

Updated: 2026-09-14.

## Latest investigation: SN-021 oplock replacement boundary

The [oplock probe](../experiments/SN-021-oplock-boundary.md), dated 2026-09-22,
measured directory RH, file RW and file RWH behavior. Directory RH and file RW
allowed replacement before release; file RWH held even our same-process replacement
until release. Directory ACK_REQUIRED did not imply child-entry exclusion.
The initial invalid-bit request failure and corrected three-case run are retained.
This rejects these simple candidates; it does not prove all oplock protocols impossible.

SN-021 remains **in_progress**; no overwrite API or TxF adoption. Next formulate
an enforceable ownership/concurrency proposal, assessing a host-owned store or
broker against the selected-directory contract before implementation. No such
architecture or scope change is selected. Do not repeat release/recheck/replace.
All 107 historical evidence files and twelve preexisting local files are preserved.
No engines, profiles, UI, downloads, issues or releases changed. Preserve SN-017,
SN-044, instrumentation, MCU independence, tolerances and PDF/PID. Final acceptance
and the next-cycle prompt remain pending. Base main: `2a02a8396b990f8739c88f6e30358e1dcf518292`;
branch `codex/sn-021-oplock-boundary`. Verify final PR checks and squash identity.

## Latest investigation: SN-021 transaction isolation boundary

The [TxF probe](../experiments/SN-021-txf-boundary.md), dated 2026-09-22, observed
six owned-fixture cases: commit/rollback, stale identity/bytes, existing writer and
writable mapping. Separate competing writes/renames reject even after the writer
handle closes and before transaction finalization. This differs from the rejected
ADR 0064 rename protocols, but is not product acceptance or a new save API.
Microsoft recommends alternatives to a new TxF dependency; no adoption is selected.

SN-021 stays **in_progress**. Retain create-only persistence and investigate a
maintained alternative without TxF, as explicitly selected by the owner.
The overwrite acceptance criterion remains in scope. Successful small-file probes do not close
identity/version, failure, supported-host and uncertain-commit acceptance work.
The owner declined TxF adoption and a reduced create-only completion baseline.
All 106 prior evidence files and twelve local SN-045 files are preserved. No engine
profile/API changes, UI, downloads, issues or release. Preserve SN-017 Python/GDB,
SN-044, instrumentation, MCU independence, tolerances and PDF/PID. Next-cycle prompt
awaits full acceptance. Base main: `5b2cf26aa6fa1564410bec8368e7e619936dffa3`;
branch `codex/sn-021-txf-boundary`. Verify final PR/check/squash and main identity.

## Latest acceptance: SN-021 configured replay lifecycle

The [configured lifecycle](../experiments/SN-021-replay-lifecycle.md), dated
2026-09-22, composes existing native create/acquire/name-edit/save-copy/reacquire
with explicit fixed replay. Nineteen Windows cases (eleven configured) and three
targeted CTests passed. Real ngspice consumed the native post-reopen artifact and
produced 5012 samples within unchanged 10 microvolt/1 ps project bounds. Only the
name token changed; policy, schedule/inventory and revised source mapping survived.
Collisions preserve other owners; missing/changed resources stop explicit compile,
not inert save/open. Production APIs and engine inputs are unchanged.

The first engine attempt in this slice passed. All 105 earlier evidence files,
including the prior replay failure, and twelve local SN-045 files are preserved.
SN-021 stays **in_progress**: safe overwrite under ADR 0064 is the remaining
implementation gate before final bounded acceptance. Define an ownership/concurrency
candidate before physical competing-writer/failure tests; do not repeat rejected
pathname protocols. Other runtime modes remain unsupported. Preserve SN-017 Python/
GDB/fixture control, SN-044, instrumentation, MCU independence, profiles and PDF/PID.
No UI, downloads, issues or release. Next-cycle prompt awaits full acceptance.
Base main: `aaf0760f5fa508b174e8c6ccc9835500d04c6936`; branch
`codex/sn-021-replay-lifecycle`. Confirm final PR/check/squash and main identity.

## Latest implementation: SN-021 fixed configured RC replay

The [fixed replay](../experiments/SN-021-fixed-replay.md), dated 2026-09-22, binds
original configured policy and captured schedule bytes to the unchanged ideal RC
analysis through a separate explicit inert operation. It accepts one known 3.3 V
source and one 5 ms interval, with debugging disabled. Other modes/parameters reject;
existing standalone APIs still reject configured requests. See ADR 0075.

Eight cases (seven Windows passes/one skip) and all 50 Windows CTests passed. Real
ngspice produced 5012 samples with maximum error 9.889724283951296e-08 V and a 5 ms
endpoint within 1 ps. Model/schedule replacement preserved the artifact; retained
staging handles denied writes/rename during consumption. The first engine attempt
failed from swapped harness directory arguments; it is preserved beside the corrected
run. All 104 earlier evidence files and twelve local SN-045 files are preserved.

SN-021 stays **in_progress**. Next compose configured acquisition/edit/save-copy/
reopen with this replay, then satisfy safe overwrite under ADR 0064. Broader runtime
modes remain unsupported; no new numerical/MCU profile or UI is implied. Preserve
SN-017 Python/GDB/fixture control, SN-044, instrumentation, tolerances and PDF/PID.
Next-cycle prompt awaits full acceptance. Base main:
`c6faf05dd7da55169f232f9888c4ece8ee761570`; branch `codex/sn-021-fixed-replay`.
Confirm final PR/check/squash and main identity before continuing.

## Latest audit: SN-021 acceptance gates

The [acceptance matrix](../experiments/SN-021-acceptance.md), dated 2026-09-22,
separates completed loading/graph/create-only persistence and isolated real-engine
compilation/boot from remaining configured runtime and safe-overwrite gates.
Two targeted CTests passed 48 new policy-rejection requests: valid configured
replay/sampled documents cannot authorize standalone RC or reference-target work,
even with measured quanta and debug labels. Native code and engine inputs are unchanged.
The audit revalidated 24 prior source hashes, 103 historical evidence files and
14 schema fixtures; twelve local SN-045 files are preserved and unpublished.

SN-021 stays **in_progress**. Next predeclare one existing measured fixture's exact
configured project-to-runtime mapping and real consumption acceptance; preserve
SN-017 Python/GDB/fixture ownership and numerical bounds. Do not substitute E-03
tolerances for project bounds. Safe overwrite still requires ADR 0064 proof.
No new profile, UI or other SN. Next-cycle prompt awaits full bounded acceptance.
Base main: `04f0174e922ca87db85213d9d184692b276862d8`; branch
`codex/sn-021-acceptance-gates`. Confirm final PR/check/squash and main identity.

## Latest implementation: SN-021 reference target and real boot

The [reference target](../experiments/SN-021-reference-target.md), dated 2026-09-22,
binds an explicit occurrence to the owned SN-012 platform/firmware and static boot
contract. Native inspection remains inert. A separate explicit Python experiment
consumed captured bytes through retained filesystem leases and passed unchanged
real Renode assertions at 100 and 1000 us. Source replacement preserved snapshots;
staged writes/rename were denied. All 49 Windows CTests passed. The first synthetic
fixture failure and 102 historical evidence files are preserved. See ADR 0074.

SN-021 remains **in_progress**. Next review the accumulated acceptance matrix,
configured temporal/runtime correspondence and safe overwrite (ADR 0064). Do not
infer general project execution or broaden supported profiles from this isolated
fixture. Preserve twelve local SN-045 files, SN-044, instrumentation, MCU/toolchain
independence, SN-017 Python/GDB/fixture control, tolerances and PDF/PID. No UI,
downloads, issues or release. Next-cycle prompt awaits full SN-021 completion.
Base main: `fe32b2591ec4e5d611082684a116a2f5e6e82908`; branch
`codex/sn-021-target-boot`. Verify final PR/check/squash and main identity.

## Latest implementation: SN-021 static boot candidate

The [boot candidate](../experiments/SN-021-boot-candidate.md), dated 2026-09-22,
requires an explicit adapter-local SN-012 reference profile and inspects owned ELF
header fields, regions, overlap, stack reserve and reset/vector correspondence.
Eight cases and all 48 Windows CTests passed. The original 9712-byte SN-012 image
matches its hash and stack/reset symbols; its unordered-load diagnostic is retained.
This is static inspection, not firmware loading, device/runtime approval or a new
executable profile. No instruction or section contents are interpreted. See ADR 0073.

SN-021 remains **in_progress**. Next bind the explicit profile to project target/
platform identity and trusted fixture resources before real-loader/boot acceptance.
Safe overwrite remains pending ADR 0064. Preserve twelve local SN-045 files,
historical/negative evidence, SN-044, instrumentation, MCU/toolchain independence,
SN-017 Python/GDB/fixture control, profiles/tolerances and PDF/PID. No UI, downloads,
issues or release. Next-cycle prompt awaits full SN-021 completion. Base main:
`e5306affc1c8a892c22096383d7f466238e9246c`; branch `codex/sn-021-boot-candidate`.
Confirm final PR/check/squash and remote identity before continuing.

## Latest implementation: SN-021 selected firmware capture

The [firmware composition](../experiments/SN-021-firmware-inspection.md), dated
2026-09-22, validates the full project, selects a firmware ID, physically captures
the complete inventory and inspects retained ELF bytes without reopening paths.
Declared architecture and observed ELF metadata stay separate; no equivalence,
boot/device/runtime or execution approval follows. All readiness flags stay false.
Nine cases (eight Windows passes/one platform skip), 47 CTests and physical
inspection of the unchanged historical SN-012 ELF passed. Its 9712 bytes remain
owned after replacing the copied file; the unordered-load diagnostic is preserved.

SN-021 remains **in_progress**. Next define explicit architecture/device/boot
correspondence for the owned fixture before real target/runtime integration.
Safe overwrite still requires ADR 0064 ownership evidence. Preserve historical/
negative evidence, twelve local SN-045 files, SN-044, shared instrumentation,
MCU/toolchain independence, SN-017 Python/GDB/fixture ownership, tolerances and
PDF/PID. No UI, downloads, issues or binary release. Next-cycle prompt awaits
full SN-021 acceptance. Base main: `8122767d58d57570740c9bc948432b161b0e7e65`;
branch `codex/sn-021-firmware-capture`. Confirm final PR/check/squash and remote.

## Latest implementation: SN-021 inert ELF32 inspection

The [ELF envelope reader](../experiments/SN-021-elf-inspection.md), dated 2026-09-16,
copies at most 16 MiB and inspects bounded little-endian ELF32 executable headers
and program records. It retains raw fields/source offsets without file access,
section-content interpretation, memory mapping, boot approval or execution.
Twelve adversarial cases and all 46 Windows CTests passed. The historical SN-012
compiler-produced ELF matches its original hash and independent binary decoding.
An initial load-order rejection is preserved; the final reader reports unordered
loads without rewriting or approving them. See ADR 0072 and the audit.

SN-021 remains **in_progress**. Next compose selected firmware declarations with
physically captured bytes and explicit architecture evidence before device/boot
validation and real target integration. No image loader or new supported MCU/
electrical/runtime profile is introduced. Safe overwrite remains pending ADR 0064.
Preserve twelve local SN-045 files, historical/negative evidence, SN-044, shared
instrumentation, SN-017 Python/GDB/fixture ownership, MCU/toolchain independence,
numerical tolerances and PDF/PID. Next-cycle prompt awaits full SN-021 acceptance.
Base main: `922f364c4464aa191cb0be214f2e0f67c65ba15a`; branch
`codex/sn-021-elf-inspection`. Confirm final PR/check/squash and remote identity.

## Latest acceptance: SN-021 project lifecycle composition

The [lifecycle acceptance](../experiments/SN-021-project-lifecycle.md), dated
2026-09-16, composes native create/open/name-edit/save-copy/reopen/explicit RC
compilation. Eight real Windows filesystem cases and all 45 CTests passed.
Two explicit pinned-ngspice runs passed: 5012 samples, maximum error
9.889724283951296e-08 V, unchanged E-01/project tolerances. The reopened artifact
matches pre-save compilation; collisions and postcapture substitutions preserve
the documented ownership boundary. No production API or automatic execution added.

SN-021 remains **in_progress**. The report maps remaining gates: safe overwrite
under ADR 0064, owned firmware-byte inspection, boot/device/interface validation
and configured runtime integration. Next define bounded inert ELF inspection of
captured bytes; do not load firmware or infer compatibility from headers/hashes.
Keep the accepted electrical profiles and all readiness flags unchanged. Preserve
historical/negative evidence, twelve unrelated local SN-045 files, SN-044,
instrumentation, MCU/toolchain independence, SN-017 Python/GDB ownership and PDF/PID.
The next-cycle prompt remains pending full SN-021 acceptance. Base main is
`a5f0c53a82092b12ecc086ddcc8639d3feb66e90`; branch `codex/sn-021-project-lifecycle`.
Consult the report and PR for final publication and hosted-check status.

## Latest implementation: SN-021 explicit ideal RC compilation

The [explicit RC compiler](../experiments/SN-021-ideal-rc-compilation.md), dated
2026-09-16, validates a complete project, captures resources through retained
NTFS handles and consumes owned bytes to emit a fixed standalone E-01 artifact.
An explicit request selects distinct reference/drive/output root ports. Three
nodes and correctly oriented 1 kohm/1 uF primitives are required; full source maps
and inputs are retained. All declaration readiness flags remain false. See ADR 0071.

Eight cases: seven Windows passes/one non-Windows-only skip; all 44 Windows CTests
passed. Real pinned ngspice: 5012 samples, error 9.889724283951296e-08 V, unchanged
E-01 and stricter fixture tolerances passed. Replacing the copied model after
compilation caused recompilation to reject while the owned artifact still ran.
No automatic engine call, source directive forwarding, download or path reopening.

SN-021 remains **in_progress**. Review remaining project/runtime acceptance and
safe overwrite ownership (ADR 0064) before the next bounded slice. This is one
standalone RC preset, not configured co-simulation or general electrical support.
Preserve twelve local SN-045 files, all historical/negative evidence, SN-044,
instrumentation, SN-017 Python/GDB/fixture ownership, MCU/toolchain independence,
numeric tolerances and PDF/PID behavior. No UI or release. Next-cycle prompt remains
pending full acceptance. Branch `codex/sn-021-ideal-rc-compile` starts at
`edbb87fa4182f7b5c27174936b1fef832f177c7c` (PR #37). Consult the report's PR record.

## Latest implementation: SN-021 exact passive numerical binding

The [numerical binding slice](../experiments/SN-021-passive-numeric.md), dated
2026-09-15, revalidates selected R/C correspondence, converts source defaults
exactly and enforces positive inclusive ranges. Reachable component values/pins
retain full paths, original effective strings, origins and offsets in source order.
Eight cases, including 40 Decimal-oracle comparisons, and all 43 Windows CTests
passed. See ADR 0070. All readiness flags stay false; no engine/netlist is emitted.

SN-021 remains **in_progress**. Next define explicit reference/stimulus/analysis
authority and bounded backend lowering using complete project validation,
structural connectivity and physically captured resources; obtain real-engine
acceptance before integration claims. Exact decimals do not certify binary solver
rounding or expand electrical profiles. Safe overwrite remains pending ADR 0064.
Preserve twelve local SN-045 files, all historical evidence, SN-044, instrumentation,
SN-017 Python/GDB/fixture ownership, MCU/toolchain independence, tolerances and
PDF/PID behavior. No UI or automatic execution. The next-cycle prompt is pending
full acceptance. Branch `codex/sn-021-passive-numeric` starts at
`8e4c3e2850bc73753fd3a600676c0a235a351b09` (PR #36). Consult the report's PR record.

## Latest implementation: SN-021 passive interface correspondence

The [selected correspondence slice](../experiments/SN-021-passive-interface.md),
dated 2026-09-15, matches complete resource-links metadata to owned recognized
R/C source bytes by lock size/hash, entrypoint, explicit ordered maps and unit.
Eight regression cases and all 42 Windows CTests passed. Metadata/source ownership,
exact limits and provenance survive caller release. All readiness flags stay false.
See ADR 0069; this adds no engine behavior or new electrical profile.

SN-021 remains **in_progress**. Next bind exact effective values and numerical
ranges, then explicit reference/stimulus/analysis authority and backend lowering.
Source defaults are not yet range-certified. Physical containment stays a separate
retained-handle acquisition gate. Safe overwrite remains pending ADR 0064.
Preserve twelve local SN-045 changes, historical evidence, SN-044, instrumentation,
SN-017 Python/GDB/fixture ownership, numerical tolerances and PDF/PID behavior.
No UI or automatic execution. The next-cycle prompt remains pending completion.
Branch `codex/sn-021-passive-interface` starts at
`0f9cf8d665ad2abfdf146c600592277b2bd9fcb0` (PR #35). Consult the report's PR record.

## Latest implementation: SN-021 bounded passive source inspection

The [passive reader slice](../experiments/SN-021-passive-source.md), dated
2026-09-15, inspects owned R/C source bytes without I/O or execution authority.
ADR 0068 defines its narrow grammar and budgets. Seven reader cases and all
41 Windows CTests passed. Explicit real ngspice acceptance of the physically
captured owned fixture produced 5012 samples with maximum error
9.889724283951296e-08 V under unchanged E-01 tolerances. The initial harness
failure and its hashes remain preserved alongside the successful second run.

SN-021 remains **in_progress**. Next bind recognized interfaces to descriptor
maps, units/ranges and effective parameters, then define explicit reference,
stimulus/analysis authority and backend lowering. No complete interface/readiness
flag is granted. Safe overwrite remains pending ADR 0064. No profile expansion,
UI, automatic project execution, SN-017 boundary or instrumentation changes.
Twelve preexisting local SN-045 files remain preserved and excluded from publication.
The next-cycle prompt is pending full SN-021 acceptance. Branch
`codex/sn-021-passive-source` starts at `de8e1438cf9b51101465d64f83d5e0451e8777f3`.
Consult the report's final PR record and verify the actual remote state.

## Latest implementation: SN-021 structural connectivity compilation

The [structural compiler slice](../experiments/SN-021-connectivity-compilation.md),
dated 2026-09-15, expands reachable occurrence/terminal identities and partitions
only explicit connections, preserving singleton terminals and full source provenance.
See ADR 0067. The original document and graph remain owned and unchanged. No
backend netlist, implicit ground, resource I/O or runtime readiness is produced.

Acceptance: 10 cases/18 requests with independent adjacency and complete source
comparisons; all 40 Windows CTests passed. Exactly 65536 expansion records pass;
one extra record fails the operation without changing declarative validity.
All historical evidence and twelve local SN-045 changes remain preserved/excluded.
Hosted checks determine final Windows/Linux acceptance separately.

SN-021 stays **in_progress**. Next establish the smallest supported model/interface
and parameter lowering from verified captured resources, with explicit reference,
stimulus and analysis authority; obtain real-engine evidence before executable
compilation claims. Preserve accepted profiles and SN-017 Python/GDB/fixture
ownership. Safe overwrite remains pending ADR 0064. No UI, instrumentation,
MCU/toolchain, numerical tolerance or PDF/PID changes. The next-cycle prompt remains
pending full SN-021 acceptance. Branch `codex/sn-021-connectivity-compilation`
starts at main `3b377e39ecb76f9941cc64872c23e6b0509ad513` (PR #33).
Consult the report's final PR record.

## Latest implementation: SN-021 source-preserving display-name revision

The [minimal revision slice](../experiments/SN-021-revision.md), dated 2026-09-15,
validates a complete base document, replaces only its top-level name token, then
revalidates/rebuilds the graph and source map. Semantic no-ops preserve original
escape spelling. See ADR 0066. Every other byte and stable ID remains unchanged.
No filesystem/resource I/O, pathname lease or save authority is introduced.

Acceptance: 11 cases/33 revision requests with independent exact-source and full
provenance comparisons; all 39 Windows CTests passed. Historical evidence and all
twelve local SN-045 changes remain preserved and excluded. Hosted checks determine
final Windows/Linux acceptance separately. Existing validators/profiles are unchanged.

SN-021 stays **in_progress**. Minimal editing is accepted; further editor features
are not selected. Next define compiler ingress/readiness and the smallest lowering
contract with stable source maps under existing profiles, then obtain required
real-engine evidence. Safe overwrite remains pending ADR 0064's ownership gate.
Keep instrumentation, MCU/toolchain independence, numeric tolerances, PDF/PID and
SN-017 Python preparation/GDB/fixture ownership. The next-cycle prompt remains
pending full SN-021 acceptance. Branch `codex/sn-021-project-rename` starts at main
`8a60f6bc870b2a277749d24f309204cca2dbed7f` (PR #32). Consult the report's final PR record.

## Latest implementation: SN-021 native project acquisition

The [acquisition slice](../experiments/SN-021-acquisition.md), dated 2026-09-15,
reads one selected project document through retained Windows/NTFS handles with a
1 MiB ceiling, then loads its owned validated source graph. See ADR 0065 and the
contract. No declared resource is opened; no pathname lease or save authority
is returned. Existing path helpers, schema limits and source semantics are unchanged.

Acceptance: 17 acquisition cases (15 local passes, one Linux-only skip and one
symlink privilege skip), all 38 Windows CTests. Post-close replacement leaves the
returned bytes/graph unchanged. Physical errors remain separate from declaration
errors. Hosted checks determine final CI acceptance. Historical evidence and all
twelve local SN-045 changes remain preserved and excluded from publication.

SN-021 stays **in_progress**. Next define minimal source-preserving revision/edit
semantics on owned documents with revalidation and stable source identities.
Safe overwrite remains pending ADR 0064's identity/version gate; compilation
requires its own real-engine evidence. No new UI, engine profile, instrumentation,
MCU/toolchain dependency, PDF/PID or SN-017 Python/GDB ownership change.
The requested next-cycle prompt remains pending full SN-021 acceptance.
Branch `codex/sn-021-project-acquisition` starts at main
`f483fded668f589dbc19c4f684070bbbce775347` (PR #31). Consult the report's final PR record.

## Latest acceptance: SN-021 overwrite ownership boundary

The [physical overwrite counterexamples](../experiments/SN-021-overwrite-boundary.md),
dated 2026-09-15, passed seven real NTFS cases and all 37 Windows CTests. See ADR
0064. Retaining no-delete-share handles prevents replacement; unlocking or
identity-checking before replacement admits another owner. POSIX handles retain
old bytes without owning the pathname. Two renames expose a destination gap.
No overwrite API is accepted; production create-only saving remains unchanged.

SN-021 stays **in_progress**. Next implement bounded native project acquisition
through retained physical handles into an owned validated graph/source capture,
without pathname lease, save authority or resource/runtime approval. Safe overwrite
remains pending a proven destination identity/version protocol; editing and
compilation remain pending. These counterexamples reject tested candidates, not
all possible Windows protocols. No engine/profile or SN-017 boundary is changed.

All twelve local SN-045 changes and historical evidence remain preserved and
excluded from publication. The next-cycle prompt remains pending full SN-021.
Branch `codex/sn-021-overwrite-boundary` starts at main
`53755a0ca232db45c17e4a16d37d96228d11942f` (PR #30). Consult the report's final PR record.

## Latest implementation: SN-021 atomic creation of project documents

The [create-only persistence slice](../experiments/SN-021-save-create.md), dated
2026-09-15, validates project 0.1 then publishes its exact owned bytes through an
exclusive temporary and handle-relative native rename with replacement disabled.
Existing and concurrently created destinations remain untouched. Internal helpers
reuse the accepted Windows/NTFS path and handle policy. See ADR 0063 and the audit.

Acceptance: 25 native boundary checks, 14 physical cases (13 local passes/one
symlink privilege skip), and all 36 Windows CTests passed. Hosted checks determine
CI acceptance separately. The first Win32 rename failure and missing-SDK-symbol
compile failure are preserved. A killed writer can leave an orphan temporary;
no power-loss durability or automatic recovery is claimed. No resource is imported.

SN-021 remains **in_progress**. Native graph loading and create-only persistence
are implemented. Next define/prove safe save-over-existing with destination
ownership and concurrent-change protection. Path acquisition/editing and compilation
with stable source mappings and required real-engine evidence remain pending.
Do not substitute path checking followed by overwrite or infer execution approval.

All twelve preexisting SN-045 changes remain local and excluded. Keep historical
hashes/failures, SN-044, instrumentation, MCU/toolchain independence, numerical
bounds, PDF/PID behavior and SN-017 Python preparation/GDB/fixture ownership.
Prepare the next-cycle prompt only after full SN-021 acceptance is integrated.
Branch `codex/sn-021-atomic-create` starts at main
`6d88455be400520d918c26406b210ad062567976` (PR #29). Follow the report's final PR/check record.

## Latest implementation: SN-021 native source connectivity graph

The [source graph slice](../experiments/SN-021-graph.md), dated 2026-09-15,
loads a complete validated project into immutable hierarchical connectivity,
standard C++ domain values and a graph-record/terminal source map. It preserves
the full original declaration, parameters and metadata without resource I/O.
See ADR 0062, the contract and audit for identity and ownership boundaries.

Acceptance: 20 cases/56 native-reference graph comparisons with exhaustive source
span checks; all 34 Windows CTests passed. Prior declaration and physical-resource
tests remain passing with explicit local privilege skips. The report records
hashes and final publication checks. Twelve preexisting SN-045 files stay local.

SN-021 remains **in_progress**. Definition graph loading is implemented; editing,
portable path-based project acquisition, safe atomic persistence and compilation
remain pending. Next implement bounded persistence of owned validated source
bytes with destination ownership, failure preservation and replacement tests,
then compilation with stable source mappings and required real-engine evidence.
Do not infer ground, flatten on load, drop metadata or grant runtime capabilities.

Keep physical containment, verified bytes, source/firmware interfaces, origin,
redistribution and execution independent. Preserve SN-044, shared instrumentation,
MCU/toolchain independence, numeric bounds, PDF/PID behavior and SN-017 Python/GDB
ownership. Prepare the next-cycle chat prompt only when full SN-021 is integrated.
Branch `codex/sn-021-source-graph` starts at main
`d78c7bef598b6fe65d300584c1da3709d84af1b7` (PR #28). Follow the report's PR/check record.

## Latest implementation: SN-021 native project declarations

The [native project slice](../experiments/SN-021-project.md), dated 2026-09-15,
composes the project 0.1 baseline over one immutable captured document. Platforms,
firmware, concrete target occurrences, pin maps and exact temporal policies are
validated without opening resources or approving execution. ADR 0061 and the
contract retain all independent physical/interface/trust/readiness gates.

Focused acceptance: 20 cases/106 native-reference comparisons plus shared ownership
and original source-position/value checks. All 32 Windows CTests passed. The report
records historical hashes, explicit local physical-test skips, initial configuration
failures and publication checks. Twelve preexisting SN-045 changes remain local.

SN-021 remains **in_progress**. Native declaration composition is implemented;
next load a bounded source graph with stable identities/source mappings, then safe
persistence and compilation with required real-engine evidence. No new engine,
MCU, toolchain, UI, instrumentation or execution profile is selected. Preserve
SN-044, numeric tolerances, PDF/PID behavior and SN-017 Python/GDB ownership.

The requested new-chat prompt for the next cycle is due only when full SN-021
acceptance is integrated. This declaration slice does not close the task.
Branch `codex/sn-021-native-project` starts at main
`c7ac6f4937b6277d9c12542b231eb0c48ccf7011` (validated PR #27 squash).
Follow the report for final required checks and the resulting main identity.

## Latest implementation: SN-021 native typed resource links

The [native resource-link slice](../experiments/SN-021-links.md) composes topology
0.3 and lock 0.1 over one immutable syntax capture, validates typed asset roles,
explicit nulls, entrypoints and complete source maps. Nested diagnostics preserve
absolute positions. No resource is opened and no interface/runtime approval is
granted. See ADR 0060, the contract and acceptance audit in the report.

Focused acceptance: 19 cases/57 native-reference comparisons plus shared ownership,
source offsets and preserved occurrence values/origins. Full regressions, hashes
and publication are recorded in the report. Twelve preexisting SN-045 changes
remain local and excluded, including their shared planning overlay.

SN-021 remains **in_progress**. Next compose project declarations, then native
source graph loading, safe saving and compilation with source mapping. Keep actual
interfaces, physical bytes, trust and execution as independent gates; preserve
SN-044, instrumentation, MCU/toolchain independence, numerical profiles, PDF/PID
and SN-017 Python/GDB ownership. No UI or engine-profile expansion is authorized.

The owner requested a new-chat prompt for the next cycle when SN-021 is fully
accepted and integrated. Prepare it at that point from official repository state;
do not treat intermediate declaration slices as completion of SN-021.

Work branch: `codex/sn-021-native-links`, based on main
`a6daf1d2ccc3bf409a45b479407b276d7b1e2690`. Follow the report's final PR/check record.

## Latest implementation: SN-021 native resource lock metadata

The [native lock slice](../experiments/SN-021-lock.md) validates resource lock 0.1
fields, IDs, provenance/license declarations, lexical paths, budgets and canonical
inventory fingerprints. It returns immutable declared requests, source offsets
and owned syntax; every physical/trust/readiness flag remains false. No resources
are opened. See ADR 0059, the contract and audit in the report.

Focused acceptance: 23 cases, 135 native/reference metadata comparisons, 137 digest
cases and native ownership/source-offset checks. One Python host integer-cap
diagnostic difference is explicit; both validators reject. Full regressions,
historical hashes and final publication are recorded in the report. Twelve
preexisting SN-045 changes remain local and excluded, including planning edits.

SN-021 remains **in_progress**. Next port typed resource links, then project
semantics before native source graph loading. Physical snapshots, actual interfaces,
source trust, redistribution and execution remain independent gates. No saving,
compilation or UI is implemented here. Preserve shared instrumentation, SN-044,
MCU/toolchain independence, numerical profiles, PDF/PID and SN-017 Python/GDB.

Work branch: `codex/sn-021-native-lock`, based on synchronized main
`bd1da0df6ad8a18b9884ca441660207cdf255fbe`. Follow the report for required checks
and squash identity. Earlier entries below are historical checkpoints.

## Latest implementation: SN-021 native descriptor bindings

The [native 0.3 slice](../experiments/SN-021-bindings.md) validates separate symbol
and model catalogs, explicit nulls, bijective maps and exact full-range agreement.
It retains owned source tokens and immutable parameter occurrences, with false
simulation readiness. No resources or engines are opened. Public 0.1/0.2 APIs
remain strict. See ADR 0058, the contract and acceptance evidence in the report.

Focused acceptance: 22 cases/157 native-reference comparisons plus C++ ownership,
map/null source positions, error offsets and version isolation. The report records
full regressions, initial test-construction failures, historical hashes and the
publication/check record. Twelve preexisting SN-045 documentation edits remain
local and excluded, including their shared planning additions.

SN-021 remains **in_progress**. Next port resource lock/link metadata and project
semantics before native source graph loading. Physical snapshots, actual source
interfaces, origin/redistribution and execution approval are independent gates.
Saving, compilation, UI and runtime readiness remain pending. Preserve SN-044,
MCU/toolchain independence, numerical profiles, PDF/PID fixes and SN-017 Python/GDB.

Work branch: `codex/sn-021-native-bindings`, based on synchronized main
`9e82f3af1cd16a9bce69fa7c4fa9f4a9e3aa09b2`. Follow the report's publication record
for final required checks and squash identity. Older entries are historical.

## Latest implementation: SN-021 native exact parameters

The [native parameter slice](../experiments/SN-021-parameters.md) implements
topology 0.2 validation and immutable inspection snapshots: exact decimal strings,
closed units, inclusive ranges, scoped overrides and per-occurrence values/origins.
Source bytes and offsets remain owned. No resources or engines are opened.
The topology 0.1 public entry point remains strict; full project semantics and
an editable domain graph remain pending. See ADR 0057 and the audit in the report.

Focused acceptance: 22 cases and 315 native/reference comparisons, plus native
ownership/source-origin checks. Full local/hosted regression results and the final
PR are recorded in the report. Original validators and historical evidence remain
preserved; shared structural code is extended intentionally and regressed against
0.1. Preexisting SN-045 documentation remains local and excluded from publication.

SN-021 remains **in_progress**. Next port descriptor/model interface declarations
for topology 0.3, followed by resource and project semantics before graph loading.
Keep declaration validity, physical snapshots, source compatibility, trust and
execution authorization separate. Saving, compilation, UI and runtime readiness
remain pending. Preserve SN-044, engine profiles/tolerances, PDF/PID fixes and
SN-017 Python/GDB fixture ownership.

Work branch: `codex/sn-021-native-parameters`, based on main
`f05b993a278e2372f97aa62bda3277b7703e26bc`. Follow the report's PR for required
checks and squash identity. Earlier entries below are historical checkpoints.

## Latest implementation: SN-021 native topology semantics

The [native topology slice](../experiments/SN-021-topology.md) validates the
complete preserved topology 0.1 contract over captured syntax: IDs/namespaces,
typed references, connections and bounded hierarchy, including unused definitions.
It preserves immutable original bytes and source offsets. No resources or engines
are opened. Later versions are rejected; project 0.1/topology 0.3 semantics remain
in Python until their native validation is implemented. See ADR 0056 in the report.

The report records native ownership/diagnostic checks, the reused Python suite,
exact limits, generated hierarchies and nested field mutations, plus full local
and hosted acceptance. Historical evidence and local SN-045 documentation remain
preserved; the latter is not included in this SN-021 publication.

SN-021 remains **in_progress**. Next port exact parameter/scoped-override semantics,
then descriptor/resource/project rules before publishing an editable source graph.
Keep metadata, physical containment, verified bytes, interfaces and execution
authorization separate. Saving, compilation, UI and runtime readiness are pending.
SN-044, numerical profiles, PDF/PID fixes and SN-017 Python/GDB ownership are unchanged.

Work branch: `codex/sn-021-topology-semantics`, based on main
`386fef7586b8fa4ec34dbd6bddc559a109f27800`. Follow the report's PR for final
required checks and squash identity. Earlier entries below are historical.

## Latest implementation: SN-021 native declaration syntax ingress

The [syntax ingress slice](../experiments/SN-021-ingress.md) implements
[ADR 0054](../decisions/0054-declaration-ingress.md): bounded pure C++20 parsing,
owned immutable original bytes/tokens, exact number spelling, decoded duplicate
key rejection and structured errors. No paths, resources or engines are opened.
This is syntax acceptance only; complete schema validation remains in Python.

Local acceptance: 95 native assertions, 209 differential/adversarial cases,
94 existing schema tests and all 20 Windows CTest entries passed. Physical
resource suites retain their documented local privilege/platform skips. All 55
selected historical hashes matched; new audit evidence preserves old records.
The native Unicode gate rejects lone surrogates more strictly than Python.
Repository checker: 488 text files passed; `git diff --check` passed.

SN-021 remains **in_progress**. Next implement schema semantics over captured
syntax before publishing a source graph; preserve IDs, exact quantities, source
positions and independent metadata/physical/interface/runtime gates. Atomic save,
compilation, UI and simulation readiness remain pending. Earlier accepted profiles,
SN-044 and SN-017 Python/GDB ownership are unchanged.

Branch: `codex/sn-021-declaration-ingress`, based on clean synchronized main
`5fa212e970e0f363e846d82f81cdb48ec1e9a10c`. Validated-source PR/checks/squash/push
remain authorized. Confirm final PR and main identity before claiming integration.
Earlier entries below are historical checkpoints.
Source `0dfd974` was pushed on `codex/sn-021-declaration-ingress`.
[PR #22](https://github.com/RicardoKers/SimNodus/pull/22) records the final required
Foundation checks and authorized protected-main squash. Resolve the final commit
identity from that record; source publication alone does not update main.

## Latest implementation: SN-021 native resource snapshots

The [native resource API](../architecture/NATIVE_RESOURCE_VERIFICATION.md) is
implemented under [ADR 0053](../decisions/0053-native-resource-verification.md).
C++20 validates typed request safety, captures immutable verified bytes and
returns structured errors. Windows handle/hash operations stay in platform code;
full project/lock metadata parsing is still a separate caller-side Python gate.
No project graph, resource interpreter or executable session is loaded.

The [native report](../experiments/SN-021-native-resources.md) records 41 typed
cases and 32 native filesystem cases (30 local passes, two symlink privilege
skips), including actual 8.3 long-name/alias checks and case-sensitive-directory
rejection. All 94 schema tests, preserved Python resource checks and 18 Windows
CTest entries passed. Final audit uses ordinary host access because the sandbox
denied native traversal of the owned C: temporary fixture. Initial failed build,
nonempty-directory fixture setup and restricted-token results remain recorded.
All 44 selected historical inputs matched their hashes. Repository checker:
479 text files passed; `git diff --check` passed. Binaries remain local/ignored.

SN-021 remains **in_progress**. Next specify bounded native declaration ingress
and graph construction. Full JSON semantics, interfaces/SVG/SPICE/ELF/boot,
atomic saving, source mapping/compilation and runtime negotiation remain pending.
No UI/SN-044, MCU/toolchain, instrumentation, engine profile, tolerance, PDF/PID
or SN-017 Python/GDB/fixture ownership changes.

Work branch: `codex/sn-021-native-resources`, from verified clean main/origin
`0c5aa6df3a7d179ed71a41c3492b385de5155007` (PR #20 squash). Validated source
commit, PR/checks, protected-main squash and push are authorized; publication of
this new block must be confirmed from its PR before claiming main is updated.
No issues, releases or binaries. Earlier task-state entries below are historical.

Source commit `bf3e05b` was pushed on 2026-09-14.
[PR #21](https://github.com/RicardoKers/SimNodus/pull/21) records the final hosted
checks and authorized protected-main squash integration. Consult its final state
and merge commit before claiming main is updated; local acceptance remains dated
2026-09-13 and its evidence is unchanged.
The first final-head hosted run passed Linux but failed the Windows temporary
long-name fixture (17/18 CTest entries passed). The fixture now expands its owned
root spelling and separately requires rejection of an aliased root. Local
filesystem rerun: 30 passes/two privilege skips. Preserve the failed run linked
in the native report and require final PR checks before integration.
Follow-up source `5f23db4` passed both Foundation jobs in
[run 34802067514](https://github.com/RicardoKers/SimNodus/actions/runs/34802067514):
32/32 native filesystem cases without Windows skips, 18 Windows CTests and
13 Linux CTests. The native snapshot slice is accepted at that scope; final
documentation and squash status remain traceable through PR #21.

## Latest implementation: SN-021 bounded local snapshots

SN-021 is **in_progress**. Its first coherent slice implements an explicit
[Windows/NTFS resource verifier](../architecture/LOCAL_RESOURCE_VERIFICATION.md)
under [ADR 0052](../decisions/0052-bounded-local-resource-snapshots.md), separate
from the unchanged SN-020 declaration validators. It traverses by parent handles,
rejects reparse points/aliases, checks bounded same-handle sizes/hashes and returns
immutable bytes by dependency/resource ID. No resource is rendered or executed.

The [report](../experiments/SN-021-local-resources.md) and
[evidence](../experiments/evidence/SN-021-local-resources-case-summary.json) record
32 resource cases: 28 local passes and four explicit skips (non-Windows rejection,
two unavailable symlink privileges, unavailable 8.3 name generation). All 94
declaration regressions remain unchanged. Hosted Windows must execute the symlink
cases before integration; short-name generation coverage remains host-dependent.

The fresh native Debug build and all 16 CTest entries passed; 40 historical
schema/fixture/SN-044 hashes matched. Repository checker: 468 text files passed;
`git diff --check` passed. These are filesystem/foundation checks, not engine
integration or native project-loader acceptance.

Next review/extract this snapshot boundary into native application code before
connecting project loading. Atomic saving, source/SPICE/SVG interfaces, ELF/boot
compatibility, compilation and runtime negotiation remain pending. No simulation
profile, UI/SN-044, instrumentation, MCU/toolchain decision or SN-017 fixture
ownership changes. Prior evidence/hashes, tolerances, PDF suppression and PID
retry are preserved. SN-021 is not complete.

Work branch: `codex/sn-021-local-resources`, from verified clean synchronized main
`7307bd88e2d468d65363fa6a6f0776772cbb124a`. Source commit
`cb6fc8d977b725d2d2c60d7aa59c2fb8efa23d2c` was pushed and both hosted Foundation
checks passed in [run 34785300902](https://github.com/RicardoKers/SimNodus/actions/runs/34785300902).
[PR #20](https://github.com/RicardoKers/SimNodus/pull/20) records the authorized
squash integration and final checks. Confirm main identity from that record;
branch publication alone does not update main. Foundation now prints verbose
CTest summaries so hosted skips are visible. No issues, releases or binaries.
The older SN-020 and other task-state entries below are historical snapshots.

## Snapshot

- Stage: M2 bounded backend proof complete through E-05 under ADR 0014; SN-016/SN-017 are done for their bounded profiles, including native/Python composition under ADR 0043. SN-018 is done for the local Windows Debug fixture baseline under ADR 0044. SN-020 is done for the declaration schema and reference validation baseline under ADR 0051. Production kernel/application remain unimplemented; general unpaced debugging is unapproved.
- Implementation: real experiment hosts run replay, approximate sampled coupling, and a direct-voltage ADC path; persistent RC adapter/CLI extracted; complete joint kernel and application pending.
- Direction: C++20 baseline, Qt 6 presentation, ngspice/XSPICE and Renode behind adapters.
- Platform: Windows first; Linux later.
- Repository language: English only.
- License: MIT for original project material, selected with the owner's authorization.
- Intended classroom use: February 2027; January is stabilization/rehearsal time.
- Author and maintainer: Ricardo Kerschbaumer.
- Git: public [RicardoKers/SimNodus](https://github.com/RicardoKers/SimNodus), default branch `main`.
- Publication: first commit `b3163a1` published on 2026-08-31; SN-041 complete.
- Checks: Windows/Ubuntu foundation and repeated E-01/SN-019/E-02 suites passed on GitHub. The complete E-03 matrix passed locally and in a [clean hosted Windows run](https://github.com/RicardoKers/SimNodus/actions/runs/33462555508), each with 54 isolated real-backend cases. The complete E-04 profile passed locally and in a [clean hosted Windows run](https://github.com/RicardoKers/SimNodus/actions/runs/33484129645), each with three fresh processes and 363 conversions.
- Collaboration: nine initial issues, four milestones, protected `main`, and private vulnerability reporting enabled.

## Latest acceptance: SN-020 declaration baseline

SN-020 is **done for declaration schema and reference validation**, under
[ADR 0051](../decisions/0051-project-declaration-baseline.md), with
[acceptance audit](../experiments/SN-020-acceptance.md) and
[evidence](../experiments/evidence/SN-020-acceptance-summary.json).
[Project declarations 0.1](../architecture/PROJECT_SCHEMA.md) compose topology,
parameters, descriptor/resource maps, locking, board/MCU/firmware metadata and
requested temporal/fidelity policy. All 94 schema tests and 15 CTest entries
passed; the native Debug build succeeded. Twenty-five prior schema hashes match.
A fresh default build also passed all 15 CTest entries. The repository checker
passed (459 text files), and `git diff --check` passed.
CI now runs the schema suite and default native foundation tests on both platforms.

Every resource/interface/firmware/runtime readiness flag remains false. No new
engine campaign, executable project, renderer, GUI or platform support is claimed.
Physical containment, source/ELF compatibility, runtime negotiation, atomic saving
and compilation remain SN-021 work. That task stays planned and was not started.
ADRs 0027/0028/0049 and the completed SN-044 documentation are preserved, as are
prior engine evidence, tolerances, PDF suppression and PID retry.

The owner's current policy authorizes validated commits, protected-main integration
and push for coherent accepted work, superseding the checkpoint-only publication
limit. Integration branch: `codex/sn-020-schema`, based on the existing checkpoint.
Source commit `1500898a7ad08b37d7e6fba15c53183fdb584875` was pushed and
[PR #19](https://github.com/RicardoKers/SimNodus/pull/19) records protected-main
integration/check status. Main requires an up-to-date pull request and both
Foundation checks through the existing squash flow. No issue synchronization,
binary release or ignored build/raw-data backup is included. Earlier publication
and task-state notes below are historical.

## Prior bounded slices and SN-044 record

Documentation update, 2026-09-13: SN-044 is **done** and records the owner-confirmed
[desktop UX direction](../architecture/DESKTOP_UX.md) and
[ADR 0049](../decisions/0049-desktop-ux-and-measurements.md): independent editor
and analyzer windows, component preview, adjustable panels, contextual probes
and shared measurement definitions. DEBUGGING remains the instrumentation
authority. No conflicts with existing decisions were found. Qt presentation
technology, Console/Diagnostics placement, layout persistence and concrete
measurement/storage interfaces remain open. Advanced analysis stays future
scope. No code, tests, CMake, simulator behavior or SN-020 progress changed.

Validation: `python tools/check_repository.py` passed (444 text files), and
`git diff --check` passed. These are documentation checks, not simulator tests
or UI usability evidence. No commit, push or remote synchronization was performed.

Next UI design gate is SN-022's Qt/worker evaluation, followed by SN-023/SN-024
implementation when their dependencies are met; none was started in this cycle.
The existing implementation checkpoint and next step below remain unchanged.

SN-020 remains **in_progress** with [resource lock metadata](../experiments/SN-020-resource-lock.md)
under [ADR 0048](../decisions/0048-declarative-resource-lock.md). All 64 schema
tests passed (18 new), both CLI outcomes matched expectations, and 19 prior
schema evidence hashes remain intact. The lock validates provenance/license
declarations, file inventory hashes and portable lexical paths without resource
I/O. Physical containment, byte verification and permission remain explicit
unverified gates. No engines or supported capabilities changed.
The repository checker passed (442 text files), and `git diff --check` passed.

Next define typed descriptor/resource references. Board/MCU/firmware and temporal
contracts remain pending; SN-021 physical loading/persistence stays planned.
Preserve existing numerical tolerances, evidence, PDF suppression, PID retry,
Python fixture/GDB ownership and ADRs 0027/0028.

The prior checkpoint was pushed and its remote hash verified as
`b3ce6748fcf18ca996dac655d54a9858e39c64a1` on `codex/checkpoint-2026-09-13`.
This new slice is local and outside that completed checkpoint. No further commit,
push, issue synchronization or release was performed; ignored build data stays
local. Historical preparation notes below describe the earlier state.

## Completed work

### 2026-09-13: SN-020 separate symbol/model interfaces

The [0.3 draft](../architecture/BINDINGS_DRAFT.md) under
[ADR 0047](../decisions/0047-declarative-bindings-draft.md) separates graphical
anchors, logical pins/ports and model-interface terminals. Explicit complete
pin/parameter maps preserve dimensions and full ranges. Null is unbound, not
fallback; every accepted document still reports `simulation_ready: false`.

All 46 tests passed (29 prior plus 17 binding tests), including symbol replacement
with unchanged topology/model/value data, invalid aliases, unused descriptors,
budgets and no file opening during in-memory validation. The CLI fixture reports
three symbol bindings and two model-interface bindings; the invalid alias fails
with `mapping`. Prior 0.1/0.2 hashes remain intact. See
[report](../experiments/SN-020-bindings.md) and
[evidence](../experiments/evidence/SN-020-bindings-summary.json).
The repository checker passed (434 text files), and `git diff --check` passed.

SN-020 remains **in_progress**. Next specify locked resource/dependency metadata
and path containment with origin/license records and no automatic execution or
download. Board/firmware and temporal contracts remain pending; SN-021 stays
planned. No graphics, model code, engines or new capabilities were exercised.

The owner authorized a source/evidence checkpoint commit and GitHub push on
2026-09-13, superseding earlier no-commit/no-push instructions for that checkpoint.
It is prepared on `codex/checkpoint-2026-09-13`; verify its remote tip before
treating the transfer as complete. No direct main update, issue synchronization
or release is included. Ignored build/runtime data remains local and needs a
separate backup; the source checkpoint does not claim to include it.

### 2026-09-13: SN-020 exact parameter/override draft

SN-020 remains **in_progress** under [ADR 0046](../decisions/0046-exact-parameter-draft.md).
The [0.2 contract](../architecture/PARAMETERS_DRAFT.md) adds bounded decimal-string
declarations with units, defaults and inclusive ranges, literal overrides, and
direct containing-circuit forwarding. Dimensions and full forwarded intervals
must match target constraints; no expressions or implicit name-based binding.
Resolution produces independent instance values without mutating shared definitions.

All 29 schema tests passed (15 unchanged topology + 14 parameter tests). The
valid CLI fixture resolved left RC leaf values to 1000 ohm/1e-6 F and right to
2200 ohm/220e-9 F, with six instance rows and 14 instance/value entries. A
dimension mismatch failed with `unit` as expected. All 0.1 evidence hashes still
matched. See [report](../experiments/SN-020-parameters.md) and
[evidence](../experiments/evidence/SN-020-parameters-summary.json).
The repository checker passed (426 text files), and `git diff --check` passed.

These are exact document values, not electrical/backend results. Next define
separate declarative symbol/model descriptors and explicit pin/parameter mappings,
without opening resources or loading code. Resource/dependency, board/firmware
and temporal contracts remain pending; SN-021 stays planned. No 0.1 migration,
native loader, new engine capability, execution, commit or remote update occurred.
Prior evidence, PDF suppression, PID retry and ADRs 0027/0028 remain unchanged.

### 2026-09-12: SN-020 topology draft and structural validation

SN-020 is **in_progress** under [ADR 0045](../decisions/0045-experimental-topology-draft.md).
The [topology-only draft](../architecture/TOPOLOGY_DRAFT.md) defines stable IDs,
component pins, circuits, instances, ports and explicit nets. Layout/models,
parameters and project resources are not part of 0.1. No source netlist,
implicit global/GND connection, code execution or backend invocation is allowed.

The [Python reference validator and fixtures](../../tests/schema/README.md) passed
15 tests: decoded-tree round trip, distinct left/right RC instance paths,
name/order independence, structured reference failures, malformed input,
recursion/depth and expansion budgets. CLI validation accepted the owned fixture
(26 entities, depth 2, six expanded instances) and rejected the two invalid JSON
fixtures with the expected `hierarchy`/`reference` codes. See the
[report](../experiments/SN-020-topology.md) and
[evidence](../experiments/evidence/SN-020-topology-summary.json).
Final documentation verification passed across the 2026-09-12/13 date boundary:
the repository checker passed (418 text files), and `git diff --check` passed.

This is structural validation, not a full component package/project loader,
electrical-solvability check, atomic saver or simulator. The two RC topologies
have no electrical values and do not expand real-engine support. Next specify
unit-bearing parameter declarations and per-instance overrides, with numeric
representation, dimensional/range checks and round-trip/invalid fixtures.
Remaining model/symbol/resource/firmware/temporal contracts still need SN-020
work; SN-021 stays planned. Prior engine evidence, PDF suppression, PID retry,
ADRs 0027/0028 and Python fixture/GDB boundaries remain unchanged. No engines,
commit, issue synchronization or remote publication were part of this cycle.

### 2026-09-12: SN-018 bounded baseline accepted

SN-018 is **done for the local Windows Debug baseline** under
[ADR 0044](../decisions/0044-bounded-baseline-acceptance.md). The
[acceptance audit](../experiments/SN-018-acceptance.md) maps its criterion to
fixed stepping, paced pause/resume and recreated-session scenarios of **one**
circuit/firmware fixture, plus recorded latency/error/memory. It does not count
these as distinct supported circuits or classroom lessons.

Read-only [audit evidence](../experiments/evidence/SN-018-acceptance-summary.json)
revalidated 3793 file hashes, 36 raw-session metric records, 324 fixed checkpoints,
360 analog checks and 14122 ancestry rows. No new engine, CTest or CubeIDE run
was performed. Historical failures and both old collectors remain preserved.
The corrected observer campaign is the accepted memory reference; its mixed
timing signs do not prove stable slowdown or zero overhead. Sampled memory,
one-host Debug scope, incomplete portable setup and other limits stay explicit.

Next select **SN-020**, starting with a small circuit/component schema and
validation specification: stable IDs, separate connectivity/symbol/model data,
hierarchy and untrusted project inputs. SN-020 remains planned until selected.
No loader or GUI implementation is claimed. Product budgets, Release/scaling,
clean-machine reproduction and classroom rehearsal remain future work. Python
preparation/GDB/fixture orchestration, tolerances, PDF suppression, PID retry and
ADRs 0014/0027/0028/0043 are unchanged. No commit, issue synchronization or remote
publication occurred. Older cycle statuses below are historical.
The repository checker passed (408 text files), and `git diff --check` passed.

### 2026-09-12: SN-018 identity correction and new control passed

The separate [corrected sampler and campaign](../experiments/SN-018-identity-control.md)
passed nine adversarial/Windows-child tests before a new predeclared U,S / S,U /
U,S campaign. Retained handles and creation/exit chronology replace PID-only
ancestry; names do not establish membership. Old sampler and failed campaign
remain unchanged. All 36 real sessions passed 324 fixed checkpoints and 36 pauses,
with final 4027000 ns/ADC 3541 and unchanged numerical limits.

[Evidence](../experiments/evidence/SN-018-identity-summary.json) preserves versions,
commands, hashes, identity records and 1492 raw-file hashes. Independent ancestry
audit passed all 14122 process-memory rows. There were zero query errors and
281 rejected candidate observations (259 not newer than parent, 22 after snapshot
start). Rejections are conservative exclusions, not zero-memory claims. Prior
evidence and frozen inputs verified unchanged; raw output is `build/sn018/identity-01`.

Paired S-minus-U wall differences were -0.695/-0.863/+0.558 s
(-1.439%/-1.624%/+1.039%). Mixed signs do not establish stable causal slowdown or
zero overhead. Pause medians were 69.610 ms without memory sampling and 66.007 ms
with it. Corrected sampled working-set sums peaked at 944.797-953.242 MiB and
private commit at 821.438-826.875 MiB. These remain sampled lower bounds.

SN-018 stays **in_progress**. Next assess its bounded reference-example and
measurement coverage for closure, rather than adding capabilities or automatically
repeating this same fixture. Python preparation/GDB/fixture ownership, PDF
suppression, PID retry and ADRs 0014/0027/0028/0043 are unchanged. No commit,
issue synchronization, new IDE certification or publication was performed.
The repository checker passed (404 text files), and `git diff --check` passed.

### 2026-09-12: SN-018 observer control exposed an attribution defect

SN-018 remains **in_progress**. The [paired control](../experiments/SN-018-observer-control.md)
executed six batches in predeclared U,S / S,U / U,S order, with unchanged real
engines and fixture inputs. All 36 sessions passed: 324 fixed checkpoints,
36 pauses, final 4027000 ns and ADC 3541. Maximum RC/endpoint errors remained
1.005721e-9 V / 8.673618e-19 s. Prior evidence and frozen inputs are unchanged.

The measurement result is **inconclusive**, despite functional success. Sampled
batch 3 included unrelated desktop processes in 80 samples and recorded 12562
query errors. Its memory totals and pair 2 overhead attribution are invalid.
Raw S-minus-U wall differences were -0.440, +2.408 and +1.452 s; no batch was
discarded or retried, and no reliable three-pair overhead percentage is claimed.
See [annotated evidence](../experiments/evidence/SN-018-observer-summary.json)
and raw `build/sn018/observer-01` (1495 raw-file hashes).

The unchanged sampler follows numeric parent PIDs without creation identity.
PID reuse/stale ancestry is plausible, but raw parent/creation data is absent,
so the exact chain is unproven. No unexpected names were found in the earlier
baseline samples; that diagnostic is not proof of ownership. Treat those memory
estimates as provisional. Next implement/test process identity and ancestry
validation, including PID reuse/stale parent and process-exit cases, before a
new predeclared campaign. Preserve this failed measurement and original inputs;
do not use names as an ownership filter. Coverage review follows the correction.

No native/harness change, new capability, IDE run, commit, issue synchronization
or publication occurred. Python GDB/setup/fixture ownership, tolerances, PDF
suppression, PID retry and ADRs 0014/0027/0028/0043 remain unchanged.
The repository checker passed (397 text files); `git diff --check` passed.

### 2026-09-12: SN-018 first local measurement cycle

SN-018 is **in_progress**. The [predeclared methodology](../../tests/headless/BASELINE.md)
and [report](../experiments/SN-018-baseline.md) establish one Windows Debug fixture
baseline with unchanged real engines and cycle 27 inputs. Three sequential
batches passed 18 fresh sessions, 162 fixed checkpoints and 18 asynchronous
pauses; every final virtual boundary was 4027000 ns with ADC 3541. Frozen inputs
and earlier compact evidence remained byte-identical.

Batch wall times were 57.472, 62.118 and 63.004 s. Pause acknowledgement wall
latency was 45.089-93.517 ms, median 68.609 ms, sample standard deviation 11.718 ms.
Maximum RC error was 1.005721e-9 V (limit 1e-5 V); endpoint error was
8.673618e-19 s (limit 1e-12 s). Peak sampled summed working set was
949.121-953.250 MiB and private committed memory 826.293-827.738 MiB.
Memory samples are lower bounds, include one failed query and actual gaps up to
292.078 ms; shared pages can be counted twice. Wall durations include host
scheduling, transport, deliberate waits and observer overhead.

[Evidence](../experiments/evidence/SN-018-baseline-summary.json) retains input
hashes, versions, exact commands, all session metrics and 606 raw-file hashes.
Raw measurements live in `build/sn018/baseline-01`. The processor CIM query was
denied; registry/environment supplied model/count. No engine run was retried.
No native/harness behavior changed, including PDF suppression and PID retry.
The repository checker passed (392 text files), and `git diff --check` passed.

Next predeclare a bounded unsampled control of the same fixture to assess
observer overhead, then assess SN-018 reference-example/measurement coverage.
This cycle does not validate Release, clean-machine, scaling, leaks, new IDE
behavior or classroom readiness. Python retains preparation, GDB transport and
fixture sequencing; no autonomous all-C++ simulator is claimed. ADRs
0014/0027/0028/0043 remain unchanged. No commit, issue synchronization or remote
publication was performed. Historical entries below retain their original status.

SN-043 also records [ADR 0028](../decisions/0028-mcu-platform-independence.md):
STM32F103C8/Blue Pill is the initial reference and MVP focus, not a core limitation.
Generic MCU concepts and instrumentation remain family-neutral; IDEs/toolchains
stay external, with ELF and GDB-compatible interfaces preferred where supported.
Additional platforms depend on mature backend CPU/peripheral models and validated
device profiles. No additional MCU support is implemented; select and validate
such profiles only in future expansion work. The documentation checker passed.

SN-043 records the accepted shared instrumentation direction in
[ADR 0027](../decisions/0027-shared-instrumentation.md) and the existing
[instruments section](../architecture/DEBUGGING.md#instruments): common acquisition,
signal/event stores, kernel-owned time, GUI-independent decoding, and future
circuit/firmware correlation. This is documentation only; instruments, stores,
decoder runtimes and sigrok integration remain unimplemented. Sampling, buffering,
interfaces, execution policy and optional dependency licensing remain open. The
existing repository checker passed. SN-024/SN-033 retain planned status; their
future acceptance direction is clarified. Next address concrete instrumentation
contracts when those tasks are selected. SN-017 scope, status, evidence and next
step are unchanged; this documentation cycle does not advance its implementation.

Requirements from the two architecture conversations have been consolidated. Architecture, temporal risks, component/subcircuit design, debugging requirements, ADRs, milestone planning, and experiment specifications have been prepared. Repository conventions, MIT licensing, contribution/security guidance, issue/PR templates, and a local/CI documentation check are included.

SN-010 selected Renode 1.16.1 and ngspice 47, verified archive/file hashes, inventoried the existing C++/ARM tools, and added a real C++ DLL startup probe. Renode headless startup and ngspice load/init/version/quit passed locally. A Windows Renode client build failure and an ngspice pre-init crash were recorded, with a tested ngspice setup workaround.

SN-011 ran eight real ngspice cases three times: analytical RC reference, external pulse, integration breakpoint, foreground/background pause and resume, circuit/full reset, invalid-netlist recovery, and solver retry. Copied callback samples matched final vectors. The external pulse's maximum error outside the declared edge windows was about 1.103 mV, below the 16.5 mV limit.

SN-019 adapted the pinned C client for native Windows and generated a separate loopback-only server extension from the matching official sources. Twenty cases passed in two local runs: real handshake/time control/reconnection with normal and one-byte transfers, plus separate fault/input tests. Actual listener ownership/address and cleanup were verified. Each real run advanced an empty machine from zero to 6,018,000 us with exact requested differences. The original all-interface server was not opened.

SN-012 built an owned freestanding STM32F103C8 ELF twice identically, booted it in an offline exact-memory profile, and exercised real SysTick GPIO, input sampling, EXTI on both edges, a 20 us pulse, and same-time edges. Two fresh runs passed the 100 us and 1000 us profiles. GPIO electrical modes, RCC propagation, ADC, GDB, and coupled causality remain unsupported or unvalidated.

SN-013 combined E-01 and E-02 into an [evidence-bounded temporal capability profile](../architecture/TEMPORAL_CAPABILITY_PROFILE.md). It selects checked integer-nanosecond orchestration with forward conversion to Renode's microsecond grid, distinguishes trial/observed/effective/committed time, approves known-schedule replay, labels bounded sampled exchange approximate, and keeps general live feedback, exact joint pause, cancellation, prediction, and rollback unsupported. E-03 thresholds, tolerances, quanta, repetitions, boundary cases, pulse classification, and failure behavior are fixed before execution in [ADR 0010](../decisions/0010-temporal-capability-profile.md).

SN-014 executed [E-03](../experiments/E-03-results.md) through real Renode and ngspice processes. Three replay runs met 1.103 mV / 0.353 us maxima and ended at an exact 10 ms common boundary. The 1000 us, 100 us, and 20 us sampled profiles met their declared `Q + 2 us` bounds with maximum delays of 846 us, 95 us, and 15 us, but every standard threshold was late to Renode and every intermediate ngspice stop was 100 ns beyond the request. Twenty-seven boundary cases, direct pulses, same-time collapse, past-input rejection, and three forced-failure/fresh-recovery pairs passed. [ADR 0011](../decisions/0011-e03-restricted-feedback.md) retains causal replay, permits only labelled sampled approximation, and keeps general live causal feedback unsupported.

SN-015 executed [E-04](../experiments/E-04-results.md) through three fresh real Renode processes after rejecting the incompatible pinned generic ADC model. The focused owned F103 extension accepted integer microvolts and performed the only 12-bit quantization. Across 363 conversions, static and boundary points, saturation, two channels, a 101-point firmware ramp, all eight timing selections, and start-time sample retention repeated exactly with zero maximum code error. Invalid channels and disabled start were rejected without hidden advancement. [ADR 0012](../decisions/0012-focused-stm32f103-adc.md) keeps the extension limited to the experiment. ngspice and electrical acquisition were not part of this evidence.

The [backlog](BACKLOG.md) owns task status. See [SN-010](../experiments/SN-010-results.md), [E-01](../experiments/E-01-results.md), [SN-019](../experiments/SN-019-results.md), [E-02](../experiments/E-02-results.md), [E-03](../experiments/E-03-results.md), [E-04](../experiments/E-04-results.md), and [QUALITY](../development/QUALITY.md). The bounded E-05 GDB/CubeIDE profile is complete under ADR 0014; SN-017 bounded headless extraction is complete under [ADR 0043](../decisions/0043-bounded-headless-extraction-acceptance.md). E-06 has not run. No production simulator has been extracted.

## SN-016 coordinated debugging: completed experiment, historical progress

Follow [SN-016 / #7](https://github.com/RicardoKers/SimNodus/issues/7). Run plain GDB against the pinned Renode and owned firmware before attempting the selected STM32CubeIDE version. Break on a GPIO change and the E-04 ADC read, then test continue, instruction-step, step-over, pause, reset, disconnect, backend failure, and timeout while recording effective stop state in every active domain.

The [E-05 experiment contract](../../tests/experiments/debugging/README.md) predeclares debugger ownership, permitted scheduler/debugger actions, consistency criteria, overshoot detection, loopback-only transport, and failure recovery. The current implementation uses an audited loopback transport bootstrap with Renode's official GDB stub. The standard all-interface GDB listener is not used.

The local plain-GDB profile passed three complete repetitions; raw results are retained at `build/sn016/plain-debug7/summary.json`. GPIO breakpoints, stepping, ADC readback, and reconstructed ngspice boundaries repeated. Interrupt does not establish a joint pause: global virtual time can continue to the active host-grant boundary. Disconnect during an active grant must not be treated as a safe joint stop. A partial report now includes actual CubeIDE evidence; ADR 0013 records the restricted transport policy; full IDE lifecycle validation remains open.

The actual STM32CubeIDE 2.2.0 launch now works through an owned Eclipse startup executor. Three fresh normal runs produced identical nine-stop, mailbox and real-ngspice replay evidence, ending at 4,027,000 ns with ADC result 2048. Fresh-session reset, stopped detach/reconnect through a new IDE launch, and interrupt measurement also passed their bounded subprofiles. The interrupt confirms that Renode global time can continue to the host-grant boundary after CPU halt; it is not a joint pause.

See the [E-05 partial report](../experiments/E-05-results.md) and [compact evidence](../experiments/evidence/E-05-partial-summary.json). The portable probe is `tests/experiments/debugging/cubeide.py`; its Java sources are preserved under `tests/experiments/debugging/cubeide/`. Current evidence is under `build/sn016/resume-normal-04` through `resume-normal-06`, `resume-reset-03`, and `resume-pause-02`. Keep prior workspaces, firmware and raw logs. SN-016 remains in progress. [ADR 0013](../decisions/0013-guarded-debugging-profile.md) now selects transport-level rejection and session abort for unsupported commands. All 15 real rejection/recovery matrix cases and a guarded actual CubeIDE normal path passed with the same guard source hash; see [guard evidence](../experiments/evidence/E-05-guard-summary.json). Three actual guarded IDE rejection/recovery pairs now passed reset, detach and interrupt diagnostics in the Eclipse console document, followed by identical normal stops and session recreation/reconnect at zero; the 15-case transport matrix passed again with the same guard hash. See [lifecycle evidence](../experiments/evidence/E-05-ide-lifecycle-summary.json). Actual guarded IDE backend-loss and missing-stop timeout probes now passed, each followed by identical normal stops and reset/reconnect recovery; see [fault evidence](../experiments/evidence/E-05-ide-fault-summary.json). The bounded lifecycle/fault subprofiles are complete. The remaining SN-016 blocker is the original joint pause/cancel and persistent coupled-state contract: resolving it needs new backend capability or an explicit requirements decision, not more repetitions of these passing subprofiles. A [public pause API investigation](../../tests/experiments/debugging/PAUSE_CANDIDATES.md) now confirms that neither tested monitor call cancels the held RunFor; both fresh recoveries passed. A [separate managed-build prototype](../../tests/experiments/debugging/COOPERATIVE_CANCELLATION.md) now passed three repetitions of cooperative cancellation, same-instance resume, running cancellation and endpoint completion precedence; its normal GDB/ngspice results exactly match the unmodified source build. An [explicit verified GDB notification](../../tests/experiments/debugging/DEBUG_NOTIFICATION.md) also passed three fresh repetitions, including stable stopped-state inspection, invalid/duplicate rejection and same-instance continue; its normal regression remains identical. The [actual guarded CubeIDE cooperative profile](../../tests/experiments/debugging/COOPERATIVE_CUBEIDE.md) now passed three fresh pause/inspect/resume repetitions, with exactly +5 ms on new authorization and unchanged default lifecycle regression. The [isolated persistent analog candidate](../../tests/experiments/debugging/PERSISTENT_ANALOG.md) also passed three identical repetitions at four known boundaries, with stable paused samples and analytical/reference voltage agreement. The [persistent joint checkpoint probe](../../tests/experiments/debugging/JOINT_PERSISTENT.md) now passed three plain-GDB repetitions with real GPIO-to-RC-to-ADC exchange, stable inspection, and the expected firmware ADC code 3541. Real analog-process loss prevented the next joint checkpoint and ADC exchange; three fresh normal repetitions recovered. The [paced joint IDE candidate](../../tests/experiments/debugging/JOINT_IDE.md) passed three complete sessions with nonzero capacitor state, DSF SIGNAL inspection, and ADC code 3541 after resume; the default lifecycle regression stayed exact. Unpaced candidates and an intermittent Eclipse shutdown failure are retained. Three [joint IDE analog-loss/recovery pairs](../../tests/experiments/debugging/JOINT_IDE_FAILURES.md) now passed with no failed-pause acknowledgement, automatic closure, recovered ADC code 3541 and unchanged default lifecycle results. Three live-worker response-timeout/recovery pairs also passed, with no false joint acknowledgement, recovered ADC code 3541, and unchanged process-loss/default lifecycle regressions; see [timeout evidence](../experiments/evidence/E-05-joint-ide-timeout-summary.json). Three [joint session-recreation/reconnect repetitions](../../tests/experiments/debugging/JOINT_IDE_LIFECYCLE.md) now passed in the same IDE, including six full joint sequences, persistent analog and firmware state at zero across disconnect, and the unchanged default lifecycle regression. This closes recreation/reconnect at zero without a grant. Three [active debugger-loss/recovery pairs](../../tests/experiments/debugging/JOINT_IDE_DISCONNECT.md) also passed after correcting cooperative socket-reset retention; no false joint acknowledgement was issued, all fresh recoveries reached ADC code 3541, and the default regression plus 13 relay tests passed. Active disconnect supports abort and a fresh session, not resumption of its partial grant. Three [active CPU backend-loss/recovery pairs](../../tests/experiments/debugging/JOINT_IDE_BACKEND_LOSS.md) also passed: the lost grant remained unacknowledged, the analog state stayed at 2011375 ns, all new sessions recovered ADC code 3541, and the default lifecycle regression remained exact. The [unpaced characterization](../../tests/experiments/debugging/JOINT_IDE_UNPACED.md) yielded one pass and two ADC-breakpoint race failures; rejected candidates published no third checkpoint, while paced/default controls passed. Three [breakpoint-before-request runs](../../tests/experiments/debugging/JOINT_IDE_BREAKPOINT_FIRST.md) now passed without pacing: the actual BREAKPOINT reason was preserved, ADC transferred once and resume recovered code 3541; paced/default controls passed. Three [actual interrupt/breakpoint arbitration runs](../../tests/experiments/debugging/JOINT_IDE_RACE.md) passed with 50 ms injected host latency, actual retained MI interruption, one ADC transfer and preserved BREAKPOINT; paced/default controls passed. Natural run 06 exposed an analog boundary failure at 3140000 ns, reproduced three times without IDE. The [joint-worker boundary correction](../../tests/experiments/debugging/ANALOG_BOUNDARY_FIX.md) now resolves this defect using ngspice equality while retaining the independent 1 ps check; 72 worker cases and six IDE sessions passed. Three [analog-loss/recovery pairs during arbitration](../../tests/experiments/debugging/JOINT_IDE_RACE_LOSS.md) now passed without joint acknowledgement or ADC transfer on failure, with fresh ADC recovery and an exact default lifecycle regression. The delayed injector was refined to wait for the observed breakpoint after its minimum delay; the failed first recovery injection is preserved. Three [live analog response-timeout/recovery pairs during arbitration](../../tests/experiments/debugging/JOINT_IDE_RACE_TIMEOUT.md) now passed, with no failed joint confirmation, recovered ADC code 3541, and process-loss/default regressions. This covers missing worker response, not solver nonconvergence. Three [CPU backend-loss/recovery pairs during arbitration](../../tests/experiments/debugging/JOINT_IDE_RACE_CPU_LOSS.md) now passed: the pending grant remained unacknowledged, analog state did not advance, and fresh sessions recovered ADC code 3541. Baseline CPU-loss and exact default lifecycle regressions passed. Three [debugger-loss/recovery pairs during arbitration](../../tests/experiments/debugging/JOINT_IDE_RACE_DISCONNECT.md) now passed with real retained interruption, verified CPU cancellation, no failed joint confirmation, recovered ADC code 3541 and both regressions. Three [combined persistent step/lifecycle runs](../../tests/experiments/debugging/JOINT_IDE_STEPS.md) now passed: six ten-checkpoint IDE sequences with source stepping, stable registers and nonzero RC state, plus zero-state recreation/reconnect and both regressions. The [bounded gate review](../experiments/E-05-gate-review.md) now calls for plain-GDB reconciliation and arbitration/fault integration with that extended step profile before a capability/extraction decision. General unpaced support remains unapproved. Remaining gates include unpaced joint interruption and other fault boundaries, and the remaining Eclipse PerspectiveManager exception. [Disposable startup configuration](../../tests/experiments/debugging/IDE_SHUTDOWN.md) now passed three fresh joint sessions plus a default lifecycle regression with automatic close acceptance and zero IDE exits; the prior manually confirmed dialog session is excluded from unattended evidence. The pinned default backend and guarded interrupt policy remain unchanged. Mouse-driven UI behavior remains unvalidated. No production adapter extraction is approved by these partial results.

## Latest maintenance: SN-042

At the owner's request, [targeted cache cleanup](../development/BUILD_STORAGE.md) removed 243 extracted Renode runtime copies (62.973 GiB) on 2026-09-03. The project now occupies approximately 1 GB. Sources, dependencies, firmware, workspaces, and experimental evidence were retained; 10,690 protected files were verified byte-identical immediately after cleanup, and all 14 pinned dependency checks passed. No permission changes or remote publication were performed.

Use `tools/clean_experiment_cache.ps1` for a preview, or add `-Apply` after stopping all experiment/debugger processes. Do not delete all of `build/`: unfinished E-05 source and evidence still live there. The useful CubeIDE prototype sources are now preserved under tests; continue with the open E-05 capability/command-ownership gate described above.

## Known uncertainties

- The adapted Renode client now covers the reported transport/time, bounded GPIO/EXTI profile, and focused ADC path. Broader system-bus behavior, callback unregistration/concurrency, and long sessions remain unvalidated.
- Client timeout/disconnect does not establish that an accepted Renode run stopped. The 1000 ms deadline is an experiment setting, not a product timing guarantee.
- The explicit loopback extension, paths without whitespace, and per-run temporary directories are experiment constraints, not a finished packaging design.
- E-02's offline profile has exact C8 memory bounds but only a fixed SysTick clock and RCC storage. It is not a complete Blue Pill or MCU platform.
- ngspice still requires the owned initialization file; the known pre-init call crash is not fixed. E-01 covers bounded RC lifecycle behavior, not arbitrary reentrancy, nonlinear models, leak endurance, or all crash/timeout paths.
- ngspice integration breakpoints do not pause execution. Trial-source time can reverse; accepted analog samples do not establish a joint MCU/circuit commit.
- Bundled runtimes and transitive licenses require review before distributing binaries.
- Known-schedule replay is the only selected causality-preserving profile. Bounded sampled feedback passed only as an approximation; general live feedback and debugger arbitration remain unsupported.
- Same-time input edges can collapse into one EXTI interrupt. Renode GPIO pulls, analog mode, open-drain electrical release, and RCC propagation are not hardware-faithful in the tested profile.
- The E-04 ADC is an owned experiment extension with fixed VREF/clock and a narrow register subset. Electrical acquisition, ngspice coupling, ADC2, interrupts/DMA, and complete peripheral fidelity remain unsupported.
- Laboratory Windows versions, hardware limits, and exact first lesson are not yet known.

## Resume checklist

1. Read this file and [AGENTS](../../AGENTS.md).
2. Check local changes before editing; do not overwrite unrelated work.
3. Select the next ready task and review its acceptance criteria.
4. Record execution evidence and update only genuinely completed states.
5. For E-05, preserve the fixed contract while implementing and executing plain GDB first, then the actual CubeIDE launch.


### 2026-09-09: plain GDB persistent fixed-step reconciliation

Three fresh direct-GDB runs passed nine persistent checkpoints, including one
instruction step, three source step-over commands, stable r0/sp/lr inspection and
ADC code 3541. All fixed times and GPIO states match the six extended IDE
sequences; 162 analog comparisons differ by at most 3.5084e-11 V. Relevant engine,
worker, GDB and firmware binaries match. Three original four-stop regression
runs also passed. Evidence: `E-05-plain-joint-steps-summary.json`; protocol:
`tests/experiments/debugging/PLAIN_JOINT_STEPS.md`. SN-016 remains in progress:
direct-GDB asynchronous pause/reset integration and extended fault/arbitration
coverage remain pending. This does not change milestone dates or open SN-017.

### 2026-09-09: direct-GDB persistent lifecycle cycle closed

Three lifecycle repetitions passed, each with two identical nine-checkpoint
persistent step sequences (54 checkpoints total). The first session exits
normally before both engines are recreated. Actual MI detach and reconnect use
the same GDB process in the recreated session; CPU/mailbox/time and untouched
analog state remain zero and stable. All GDB/backend exits were zero, old
listeners were removed and final ADC remained 3541. Three original four-stop
regressions also passed. Evidence: `E-05-plain-joint-lifecycle-summary.json`.
The completed cycle is the direct-GDB fixed-step recreation/reconnect extension;
SN-016/E-05 remains open for asynchronous direct-GDB pause integration, extended
fault/arbitration integration and the bounded capability decision. The user
requested a conversation handoff at this boundary; see `NEXT_CONVERSATION.md`.

### 2026-09-09: direct-GDB asynchronous host pause cycle closed

Three persistent lifecycle repetitions passed six extended sequences: 54 fixed
checkpoints and six asynchronous host-requested pauses, with CPU cancellation
verified before analog advancement and joint SIGINT notification. Pauses occurred
at 2013000 or 2014000 ns; acknowledgement took 42.7100 to 54.3480 ms. Stable
inspection, recreated zero state, same-GDB detach/reattach, ADC code 3541, zero
process exits and listener cleanup passed. Across 324 comparisons with the
previous IDE step sequences, fixed GPIO/time agreed and maximum analog difference
was 1.7969e-11 V. Three original four-stop regressions matched the prior control
exactly. See [evidence](../experiments/evidence/E-05-plain-joint-pause-summary.json)
and [protocol](../../tests/experiments/debugging/PLAIN_JOINT_PAUSE.md).

This is paced host cancellation with direct GDB notification, not an MI interrupt
through the relay. The bounded state reconciliation is measured; command-path
equivalence is not claimed. SN-016 remains in progress and SN-017 remains planned.
Next: integrate the extended IDE step sequence with fault/arbitration checks,
then record the bounded capability decision and assess the complete gate.

### 2026-09-10: extended IDE fault/arbitration cycle closed

The [35-session matrix](../experiments/evidence/E-05-steps-fault-matrix-summary.json)
passed on matching sources/binaries: three initially paced arbitration
fault/recovery pairs per fault (analog loss, live response timeout, CPU loss,
debugger loss), four continuously paced extended pairs, extended lifecycle,
shorter arbitration and exact default lifecycle controls. Failures retained
seven joint checkpoints at or before 2012875 ns with no ADC transfer or joint
pause acknowledgement. Every recovery reached ADC 3541. All IDEs accepted
closure, exited zero and removed owned listeners.

Initial arbitration pacing is released only after retained MI interruption and
pre-ADC observation. Three unsuccessful unpaced injection candidates remain
failed. A fourth matrix attempt failed on transient Windows CPU-result access;
the reader now retries only within the original phase deadline. All 14 tests
passed, including denied-read/no-acknowledgement and deadline enforcement. The
final matrix needed no natural read retry. See the
[protocol](../../tests/experiments/debugging/JOINT_STEPS_FAULTS.md) for scope,
timings, reproduction and retained predecessors.

SN-016 remains in progress. The next bounded work is the capability decision
and full E-05 gate assessment, with initial pacing, host cancellation versus MI
interruption, CPU acknowledgement versus joint commit, and full-recreation
reset explicitly distinguished. SN-017 has not started; milestone dates remain
unchanged. General unpaced behavior and arbitrary solver recovery are unapproved.

The final shared-bridge direct-GDB regression also passed three recreated
lifecycle repetitions (six sequences, 54 fixed checkpoints and six host pauses),
with matching shared hashes, ADC 3541 and complete cleanup. See
[regression evidence](../experiments/evidence/E-05-steps-fault-plain-regression-summary.json).

### 2026-09-10: bounded E-05 gate complete; SN-017 ready

[ADR 0014](../decisions/0014-bounded-cooperative-debugging.md) accepts the
measured single-CPU cooperative profile. The final direct-GDB relay check passed
three recreated lifecycle repetitions (six sequences), with actual retained MI
interrupts, joint SIGINT after CPU/analog agreement, ADC 3541 and full cleanup.
This closes the earlier command-path reconciliation gap. The
[case-by-case review](../experiments/E-05-gate-review.md) maps the original
acceptance cases to actual evidence, including the 35-session IDE fault matrix.

The owner-reported PDF annoyance is fixed for disposable IDE launches: seed the
active documentation version as already seen in ConfigurationScope. Installed
ReleaseNotesOpener inspection identifies that guard; runtime startup confirms
it. Three actual IDE controls passed, including extended lifecycle and exact
default lifecycle. The installed IDE, PDFs, existing browser documents and
Defender scanning remain unchanged. See [final evidence](../experiments/evidence/E-05-final-gate-summary.json).

SN-016 is **done for the declared bounded profile**. SN-017 is **ready**, not
started. Next extract a small headless fixture runner and explicit backend
contracts, preserving time ownership, CPU acknowledgement versus joint commit,
fresh-session recovery and the measured pacing restrictions. Keep Qt out of the
kernel and third-party types behind adapters. No production readiness or general
unpaced support is implied; milestone dates remain unchanged. Repository status
is updated locally; no commit or remote issue/publication action was performed.


### 2026-09-10: SN-017 persistent RC extraction cycle closed

The root CMake project now builds an opt-in Windows C++20 RC fixture runner and
ngspice adapter, with a dependency-free analog boundary contract. The original
E-05 worker remains the differential reference. This is the first extraction
slice, not the complete joint headless runner; Renode/GDB orchestration, grant
accounting and joint commits remain in the validated experimental harness.

Real ngspice passed 72 differential cases with identical output, 1 ps endpoints,
10 uV RC tolerance and nine invalid-request rejections. Real Renode/ngspice/GDB
passed six recreated sessions, 54 checkpoints, six guarded MI pauses and ADC
3541. Analog loss retained two checkpoints; three fresh sessions recovered.
CTest and all 14 relay/accounting tests passed. See the
[protocol](../../tests/headless/README.md) and
[evidence](../experiments/evidence/SN-017-rc-extraction-summary.json).

SN-017 remains **in_progress**. Next extract CPU grant/result accounting and
joint-state transitions before replacing the Python orchestration. ADR 0014,
backend capability restrictions, failed-run history and IDE PDF suppression are
preserved. No commit, remote publication or milestone change was performed.


### 2026-09-10: SN-017 CPU/joint-state contract cycle closed

The C++20 `JointSession` now gates grants, observations, CPU acknowledgements,
analog agreement and commits in the opt-in real-engine harness. Failure is
terminal and unused time is discarded. [ADR 0015](../decisions/0015-incremental-session-contract.md)
records the temporary helper boundary; scheduling/transport remain experimental.

Final recovery passed six sessions, 54 fixed checkpoints and six guarded MI
pauses, with ADC 3541 and complete process/listener cleanup. CPU loss before
acknowledgement retained two acknowledgements/two commits; analog loss after
acknowledgement retained three acknowledgements/two commits. Fixed state matches
the previous extraction, with maximum analog difference 2.17604e-14 V.
Two CTest targets, 15 CLI replay/adversarial cases, all 14 relay/accounting tests
and three default-path controls passed. See the
[protocol](../../tests/headless/SESSION_CONTRACT.md) and
[evidence](../experiments/evidence/SN-017-session-contract-summary.json).

SN-017 remains **in_progress**. Next extract Renode cancellation-result parsing
and bounded transport/deadline ownership before migrating joint orchestration.
The IDE/PDF suppression code and historical evidence remain unchanged. No commit,
remote publication, capability expansion or milestone change was performed.


### 2026-09-10: SN-017 CPU/joint-state contract cycle closed

The C++20 `JointSession` now gates grants, observations, CPU acknowledgements,
analog agreement and commits in the opt-in real-engine harness. Failure is
terminal and unused time is discarded. [ADR 0015](../decisions/0015-incremental-session-contract.md)
records the temporary helper boundary; scheduling/transport remain experimental.

Final recovery passed six sessions, 54 fixed checkpoints and six guarded MI
pauses, with ADC 3541 and complete process/listener cleanup. CPU loss before
acknowledgement retained two acknowledgements/two commits; analog loss after
acknowledgement retained three acknowledgements/two commits. Fixed state matches
the previous extraction, with maximum analog difference 2.17604e-14 V.
Two CTest targets, 15 CLI replay/adversarial cases, all 14 relay/accounting tests
and three default-path controls passed. See the
[protocol](../../tests/headless/SESSION_CONTRACT.md) and
[evidence](../experiments/evidence/SN-017-session-contract-summary.json).

SN-017 remains **in_progress**. Next extract Renode cancellation-result parsing
and bounded transport/deadline ownership before migrating joint orchestration.
The IDE/PDF suppression code and historical evidence remain unchanged. No commit,
remote publication, capability expansion or milestone change was performed.


### 2026-09-10: SN-017 native cancellation-result ingress cycle closed

The opt-in `--native-results` path now parses the real Renode cancellation file
and owns bounded result polling in C++. It rejects malformed/stale results,
retains a single deadline across Windows sharing retries, and acknowledges the
CPU only after parsing and session accounting pass. [ADR 0016](../decisions/0016-bounded-cancellation-ingress.md)
records the 1900 ms native budget inside the unchanged 2000 ms host watchdog.

Final recovery passed six sessions, 54 fixed checkpoints and six guarded MI
pauses, with ADC 3541 and complete cleanup. All 60 C++ cancellation results match
the raw Renode files; fixed CPU and analog checkpoints match the previous slice.
Withheld response failed in 1.9018704 s with only two acknowledgements/two commits.
CPU loss retained two/two and analog loss retained three acknowledgements/two
commits. The actual withheld backend result remains preserved.

Fifteen native file cases passed, including real Windows sharing locks and late
publication. Three CTest targets, 15 prior CLI cases, 14 relay/accounting tests
and three legacy-parser controls passed. See the
[protocol](../../tests/headless/RESULT_INGRESS.md) and
[evidence](../experiments/evidence/SN-017-result-ingress-summary.json).

SN-017 remains **in_progress**. Next extract cancellation commands and the
ready/error handshake under a single phase deadline, then native process
ownership and orchestration. GDB relay, pacing and lifecycle remain in the
experimental harness. IDE/PDF suppression and historical evidence are unchanged.
No commit, publication, capability expansion or milestone change was performed.


### 2026-09-10: SN-017 native start/cancel and ready cycle closed

The opt-in `--native-commands` path now sends the measured grant/cancel commands
through an explicitly inherited Renode stdin pipe. Native ready/error handling
and result ingress retain one 1900 ms budget per phase under the 2000 ms host
watchdog. [ADR 0017](../decisions/0017-native-cancellation-channel.md) records
handle ownership, serialized writes and the restricted command/path contract.

Final recovery passed six sessions, 54 fixed checkpoints and six retained MI
pauses, with ADC 3541 and complete cleanup. The helper performed 60 native starts,
60 ready handshakes and 60 native cancellations; all 60 results match raw Renode
files. Fixed CPU/analog state matches the previous slice exactly. CPU loss made
the native cancellation write fail and retained two acknowledgements/two commits;
analog loss retained three acknowledgements/two commits. Fresh recovery passed.

Seventeen real OS pipe cases, 15 native-file regressions, 15 prior CLI cases,
three CTest targets, 14 relay/accounting tests and three legacy-command controls
passed. See the [protocol](../../tests/headless/COMMAND_CHANNEL.md) and
[evidence](../experiments/evidence/SN-017-command-channel-summary.json).
Ready-timeout/error cases are controlled pipe/file tests, not a claim of real
Renode timeout coverage at every boundary.

SN-017 remains **in_progress**. Next extract fixed-fixture process lifecycle,
retaining the harness as the differential reference. Progress sampling, joint
notification, GDB relay and scheduling remain experimental. IDE/PDF suppression,
backend capabilities and historical evidence remain unchanged. No commit,
publication or milestone change was performed.


### 2026-09-10: SN-017 native backend lifecycle cycle closed

Renode and the persistent analog worker can now be created/reaped by an opt-in
C++ Windows supervisor. Suspended creation, explicit handle inheritance and
kill-on-close Job Objects preserve owned process-tree cleanup. Child and supervisor
PIDs and normal/forced outcomes remain distinct. [ADR 0018](../decisions/0018-native-fixture-process-lifecycle.md)
records the scope; Python retains fixture preparation and orchestration.

Final recovery passed six sessions with 12 supervised backend launches, 54 fixed
checkpoints, six retained MI pauses and ADC 3541. All 60 cancellation results
match the raw Renode files; fixed CPU state matches the prior slice and maximum
analog difference is 9.76997e-15 V. Two real engine-loss and two real supervisor-loss
cases retained prior commits. Supervisor-loss tests independently waited on the
actual child process handle; no successful native exit record was fabricated.

Nine lifecycle cases, 17 pipe cases, 15 ingress cases, 15 prior CLI cases, three
CTest targets, 14 relay/accounting tests and three legacy-lifecycle controls
passed. See the [protocol](../../tests/headless/PROCESS_LIFECYCLE.md) and
[evidence](../experiments/evidence/SN-017-process-lifecycle-summary.json).

SN-017 remains **in_progress**. Next extract the remaining bounded progress and
joint-notification commands before consolidating native orchestration. Fixture
preparation, GDB relay and scheduling remain experimental; host-harness death
without supervisor death is not newly validated. IDE/PDF suppression, engine
capabilities and historical evidence remain unchanged. No commit, publication
or milestone change was performed.

### 2026-09-10: SN-017 native debug command cycle closed

Progress sampling and joint-notification command emission now use the native
inherited Renode channel. The timestamp is derived from acknowledged CPU state;
analog agreement precedes notification. Pending notification blocks commit, and
emission/host attestation share a non-renewing 1900 ms deadline after cancellation.
The complete host MI acknowledgement limit remains 2000 ms. See
[ADR 0019](../decisions/0019-native-debug-command-emission.md).

Final real-engine recovery passed six sessions, 54 fixed checkpoints, six MI
pauses and ADC 3541. All 60 native cancellation results match raw files; fixed
CPU/firmware checkpoints match the fifth cycle within the unchanged 10 uV analog
tolerance. All 12 supervised backend launches exited normally and listeners were
removed. Four real engine/supervisor loss controls retained prior commits; three
legacy repetitions matched their prior checkpoints. Twenty-one new pipe cases,
17 prior pipe cases, 15 ingress cases, 15 session cases, three CTest targets and
14 Python tests passed. Detailed values, hashes and raw reports are in the
[evidence](../experiments/evidence/SN-017-debug-commands-summary.json).

SN-017 remains **in_progress**. Progress and notification response files remain
host-read; their non-atomic publication handling is the next coherent ingress
slice before orchestration. The host still validates actual GDB notification and
stable inspection. This cycle does not claim native end-to-end debug event
ownership. [Reproduction and scope](../../tests/headless/DEBUG_COMMANDS.md).
PDF suppression hash matches the prior evidence; no IDE launch changes, commit,
publication or capability/milestone expansion occurred.

### 2026-09-10: SN-017 native debug reply ingress cycle closed

Opt-in `--native-debug-results` now reads complete progress and joint-notification
files in C++. Exclusive Windows opening fences the owned bridge writer's close;
partial numeric data remains pending while that handle is open. Strict parsing,
stale/error checks and non-renewing phase deadlines precede native observation
or notification consumption. Neither transition commits. See
[ADR 0020](../decisions/0020-native-debug-reply-ingress.md).

Final Renode/ngspice/GDB recovery passed six sessions, 54 checkpoints, six MI
pauses and ADC 3541. Native debug and cancellation results match the raw files;
fixed CPU state matches the preceding cycle and six host-read control sessions,
with analog differences below the unchanged 10 uV tolerance. Twelve backend
launches exited normally and listeners were removed. Four real engine/supervisor
loss controls retained prior commits. All 33 new file/pipe cases, 21 previous
debug-command cases, 17 command cases, 15 cancellation cases, 15 session cases,
three CTest targets and 14 Python tests passed. Exact measurements and historical
intermediate runs are retained in the
[evidence](../experiments/evidence/SN-017-debug-ingress-summary.json).

SN-017 remains **in_progress**. Next consolidate the smallest coherent bounded
orchestration step. GDB signal/time verification, stable inspection, preparation
and scheduling remain in Python. The closure guarantee requires the owned
single-open writer on Windows; arbitrary writers and Linux remain unsupported.
The real loss controls do not claim fault injection during notification ingress.
[Reproduction](../../tests/headless/DEBUG_INGRESS.md). PDF suppression is unchanged;
no commit, publication, milestone change or capability expansion occurred.

### 2026-09-10: SN-017 composite grant transition cycle closed

Opt-in `--native-grants` consolidates grant begin/backend start and observed-stop/
backend cancellation into two native commands. Existing ready, cancellation and
debug readers retain their deadlines. Input validation precedes grant begin;
pending progress or invalid stop observations prevent cancellation emission.
These are not atomic backend transactions: partial failure is terminal and
preserves earlier commits. [ADR 0021](../decisions/0021-composite-fixture-grant-transitions.md).

Final real-engine recovery passed six sessions, 54 fixed checkpoints, six MI
pauses and ADC 3541. All 60 composite grants/stops match raw cancellation results;
12 native debug replies match their files. Fixed digital state matches the prior
cycle and six current reference sessions; analog differences remain below 10 uV.
All 12 backend launches exited normally and listeners were removed. Four real
engine/supervisor loss controls retained prior commits. Twenty-one new composite
pipe cases, 33 debug ingress cases, 21 debug command cases, 17 command cases,
15 cancellation cases, 15 session cases, three CTest targets and 14 Python tests
passed. Exact measurements and raw reports are in the
[evidence](../experiments/evidence/SN-017-grant-transitions-summary.json).

SN-017 remains **in_progress**. Next extract bounded analog catch-up after CPU
acknowledgement, retaining separate exchange, inspection and commit. Polling,
GDB verification and complete scheduling remain in the harness. See the
[protocol](../../tests/headless/GRANT_TRANSITIONS.md). Capabilities, historical
evidence and PDF suppression are preserved; no IDE startup change, commit,
publication or milestone change occurred.

### 2026-09-10: SN-017 native analog command cycle closed

`--native-analog` gives the runner an inherited analog-worker stdin. The native
advance command derives its target from acknowledged CPU state and rejects
pending/unsolicited operations. Host-read analog agreement must match the endpoint
and arrive within 1900 ms; command emission does not acknowledge or commit.
[ADR 0022](../decisions/0022-native-analog-command-channel.md) records the boundary.

The first recovery failed during transient access denial reading the published
Renode supervisor PID. Its report is retained as failed. Startup reads now retry
missing/access-denied files within the original five-second deadline; malformed
PID data remains terminal. Four actual file/lock cases and nine lifecycle cases
passed. The exact source of the transient denial was not identified.

Final real-engine recovery passed six sessions, 54 fixed checkpoints, six MI
pauses and ADC 3541. All 60 native advances match host-submitted observations and
raw worker logs; all 60 CPU results match raw cancellation files. Fixed digital
and analog state matches both the previous cycle and six current reference
sessions exactly. All 12 backend launches exited normally and listeners were
removed. Four real engine/supervisor loss cases retained two prior commits and
only two successful advance writes. Thirteen new analog pipe cases, four startup
cases, nine lifecycle cases and all prior grant/debug/result/core regressions
passed. See [evidence](../experiments/evidence/SN-017-analog-command-summary.json)
and [reproduction](../../tests/headless/ANALOG_COMMANDS.md).

SN-017 remains **in_progress**. Next extract bounded analog reply ingress; the
host still reads worker JSON and owns exchange, inspection and scheduling. The
original failed recovery and earlier fault reports remain unchanged. Capabilities,
PDF suppression and milestone targets are preserved. No commit or publication.

### 2026-09-10: SN-017 exclusive analog reply ingress cycle closed

`--native-analog-results` assigns worker stdout exclusively to the native runner.
Python no longer starts an analog reader thread on this path. Native framing and
strict ordered-field parsing validate initial, advance and inspection/exchange
responses under 1900 ms deadlines. Advance replies establish analog-ready only;
host attestations cannot bypass native ingress and pending reads block other
transitions except polling/abort. [ADR 0023](../decisions/0023-native-analog-reply-ingress.md).

Final real-engine recovery passed six sessions, 54 checkpoints, six MI pauses
and ADC 3541. All 141 native worker payloads are preserved in the analog logs;
60 native analog agreements and 60 raw CPU results were checked. Fixed digital
state matches the preceding cycle and six current host-reader controls. Maximum
analog difference is 2.17604e-14 V, below the unchanged 10 uV tolerance. All 12
backend launches exited normally and listeners were removed. Four real
engine/supervisor loss controls retained prior commits.

Twenty-five new pipe cases and all prior analog-command, grant, debug, result,
core and Python regressions passed. Two failed closed-pipe diagnostic checks and
one interrupted oversized test producer are retained; the producer now streams
bounded chunks. Pending-read abort cleanup was tightened before the final run.
See [evidence](../experiments/evidence/SN-017-analog-ingress-summary.json) and
[reproduction](../../tests/headless/ANALOG_INGRESS.md).

SN-017 remains **in_progress**. Next select the smallest bounded exchange/inspection
coordination step. Analytical checks, actual GDB verification and complete
scheduling remain in the host. The wire format has no transaction IDs, so identical
late inspection replies cannot independently prove request identity. General
workers, Linux ingress and complete native orchestration remain pending. Prior
PID retry and PDF suppression hashes are unchanged. No commit, publication,
capability expansion or milestone change occurred.

### 2026-09-11: SN-017 native high/inspection coordination cycle closed

`--native-exchange` routes high and inspect through the native worker channel.
High is limited to the single measured 2011375 ns GPIO boundary after analog
agreement; the host still verifies the actual GPIO register. Inspection requires
initialized stopped or analog-ready state. Both arm the existing reader before
emission and require an unchanged reply snapshot before further transitions.
[ADR 0024](../decisions/0024-native-fixture-exchange-inspection.md).

Real recovery passed six sessions, 54 checkpoints, six MI pauses and ADC 3541.
Six native high commands and 69 native inspections received 75 unchanged replies;
all 141 worker payloads match preserved logs and all 60 CPU results match raw
files. Fixed digital and analog state matches both the preceding cycle and six
current reference sessions exactly. All 12 backend launches exited normally and
listeners were removed. Four real engine/supervisor loss controls retained prior
commits. Nineteen new actual pipe cases and all prior analog, grant, debug, result,
core and Python regressions passed. The real runs completed before a tool approval
usage-limit interruption; test creation and remaining checks were completed after
resumption. No unexecuted check was counted as passing.

See [evidence](../experiments/evidence/SN-017-exchange-inspection-summary.json) and
[reproduction](../../tests/headless/EXCHANGE_INSPECTION.md). SN-017 remains
**in_progress**. Next extract bounded ADC boundary-input coordination. Host GPIO,
analytical and actual GDB checks plus final scheduling remain separate. Prior
reader/transaction-ID restrictions, evidence, PID retry and PDF suppression are
preserved. No commit, publication, capability expansion or milestone change.

### 2026-09-11: SN-017 bounded ADC coordination cycle closed

`--native-adc` derives channel 0 microvolts from native analog state only at the
4010750 ns ADC boundary after measured GPIO high and CPU/analog agreement.
Preparation and matching confirmation share a native 1900 ms deadline; the host
retains a 2000 ms preparation/invocation/confirmation budget. Pending coordination
blocks commit. The existing ADC helper is still invoked by Python and its response
has microsecond resolution without channel/value readback. Native confirmation
therefore remains a checked host attestation, not independent transport ingress.
[ADR 0025](../decisions/0025-bounded-adc-input-coordination.md).

Real recovery passed six sessions, 54 checkpoints, six MI pauses and six ADC
preparations/confirmations with firmware code 3541. Fixed states and exchange
values match both the previous cycle and six current reference sessions exactly.
All 141 worker payloads match logs and all 60 CPU results match raw files. All
12 backend launches exited normally and listeners were removed. Four real
engine/supervisor loss controls retained prior commits before ADC preparation.
Eighteen new pipe cases, six numerical reference cases and four invalid numeric
cases passed alongside all prior regressions, four CTest targets and 14 Python
tests. The initial numerical test include error was corrected before validation.
Documentation completion resumed after a tool approval usage-limit interruption.

See [evidence](../experiments/evidence/SN-017-adc-coordination-summary.json) and
[reproduction](../../tests/headless/ADC_COORDINATION.md). SN-017 remains
**in_progress**. Next extract bounded ADC helper invocation/result ingress;
analytical/GDB checks and final scheduling remain host-owned. Expected ADC code
is validation metadata, not an extra input quantization stage. Capabilities,
historical evidence, PID retry and PDF suppression are preserved. No commit,
publication or milestone change occurred.

## 2026-09-11: SN-017 thirteenth cycle closed

Native ADC helper invocation and result ingress are validated under
[ADR 0026](../decisions/0026-native-adc-helper-process.md), with the
[protocol and reproduction commands](../../tests/headless/ADC_PROCESS.md) and
[recorded evidence](../experiments/evidence/SN-017-adc-process-summary.json).
`--native-adc-process` launches the explicitly configured helper using prepared
channel/voltage arguments, captures bounded streams, and requires the expected
reply, zero exit and confirmed job cleanup before ADC confirmation. Preparation
and execution share the original 1900 ms acceptance deadline. Failure cleanup
uses a separate bounded wait; it cannot permit late confirmation.

The final real-engine matrix passed six recreated sessions, 54 fixed checkpoints,
six MI pauses, 60 cancellation agreements and six native ADC helper invocations.
ADC remained 3541; fixed analog values matched the preceding cycle and current
host-invocation reference exactly. All 141 worker payloads matched their logs.
Four real backend/owner-loss controls and fresh recovery passed; these controls
fail before ADC invocation. Controlled real Windows helper processes cover ADC
failure behavior separately: all 21 cases passed, including shared deadline,
fragmentation, malformed/oversized replies, stderr, nonzero exit, duplicate
operations, descendant cleanup and runner loss. Earlier passing reports from
before capture-limit/resume checks were tightened remain preserved.

Regression suites passed: 18 ADC coordination, 19 exchange, 25 analog ingress,
13 analog command, 21 grant, 33 debug ingress, 21 debug command, 17 command-channel,
15 cancellation-result and 15 session cases; four CTest targets and 14 Python
unit tests also passed. PDF suppression and the bounded PID startup retry remain
unchanged, with hashes checked against preceding evidence.

This is a Windows fixture slice, not a complete simulation kernel. The helper
reply has microsecond resolution and does not establish exact nanosecond ADC
readback or electrical acquisition. ADR 0014 restrictions remain in force.
SN-017 remains **in_progress**; next extract bounded analytical RC validation into
native coordination before further GDB verification and joint commit scheduling.
No commit or remote publication was performed.

## 2026-09-11: SN-017 fourteenth cycle closed

Native bounded RC analytical validation is implemented and tested under
[ADR 0029](../decisions/0029-native-rc-trajectory-validation.md). The opt-in
`--native-rc` path checks each advance reply before accepting analog state, using
the native high-command state and the existing 3.3 V, 1 ms, 10 microvolt/1 ps
fixture contract. Activation is allowed only before initial worker consumption;
invalid trajectories abort without a new joint commit. The Python analytical
assertions remain an independent verification oracle.

The [evidence](../experiments/evidence/SN-017-rc-trajectory-summary.json) records
six initial guarded sessions and six fresh recovery sessions, plus four real
backend/owner-loss controls. The final recovery passed 54 fixed checkpoints,
60 native RC checks, six MI pauses and six native ADC helper invocations with
ADC 3541. Fixed CPU, analog, mailbox, register and exchange records matched the
preceding cycle exactly. Every final joint commit had its native analytical
check; loss controls stopped at two commits. The PDF suppression and bounded
startup PID retry hashes remain unchanged.

Five CTest targets passed, including 15 analytical cases. Nine new Windows pipe
cases passed, along with 25 analog ingress, 19 exchange, 18 ADC coordination and
21 ADC process regressions. The first pipe-test attempt failed because its
producer emitted spaces outside the existing exact frame syntax; that report
is preserved and the corrected producer passed. See the
[protocol and reproduction](../../tests/headless/RC_TRAJECTORY.md).

SN-017 remains **in_progress**. This fixture oracle adds no solver, prediction,
rollback, general MCU support or debugging capability. The next smallest step is
bounded native readback verification, retaining the GDB allowlist and separate
joint commit scheduling. ADRs 0014, 0027 and 0028 remain unchanged. No commit or
remote publication was performed.

## 2026-09-11: SN-017 fifteenth cycle closed

Bounded native final mailbox verification is implemented and validated under
[ADR 0030](../decisions/0030-bounded-native-mailbox-readback.md). The opt-in
`--native-readback` path compares the host-normalized eight-word mailbox at
4027000 ns with native prepared ADC state. It requires completed ADC confirmation
and analog agreement before verification, and verification before the final
commit. GDB collection and before/after stability checks remain host-owned.
Verification itself neither commits nor grants time.

The [evidence](../experiments/evidence/SN-017-readback-summary.json) records six
initial guarded sessions, four real backend/owner-loss controls and six recovery
sessions. Final recovery passed 54 fixed checkpoints, six native readback
confirmations, 60 RC checks and six MI pauses. The ADC remained 3541; fixed CPU,
analog, mailbox, register and exchange records matched the preceding cycle
exactly. All final commits followed native readback verification. Loss controls
stopped at two commits without readback confirmation. PDF suppression and startup
PID retry hashes remain unchanged.

Six CTest targets passed, including 16 readback validator cases. All 16 readback
pipe cases passed, along with nine RC, 18 ADC coordination, 21 ADC process,
19 exchange, 25 analog ingress and 15 session regressions, plus 14 Python unit
tests. Historical session replay initially rejected the added diagnostic fields;
the corrected comparison requires all four optional fields to remain zero and
preserves exact comparison of every historical field. Its failed report is kept.
An earlier missing-reference invocation exited during argument parsing without
running cases. See [reproduction and limits](../../tests/headless/READBACK.md).

SN-017 remains **in_progress**. This slice verifies host-normalized observations;
it does not independently authenticate GDB origin or freshness. Next extract
bounded raw readback ingress while retaining host GDB ownership, the existing
allowlist and separate joint commit scheduling. ADR 0014 capabilities and
ADRs 0027/0028 remain unchanged. No commit or remote publication was performed.

## 2026-09-11: SN-017 sixteenth cycle closed

Native raw MI mailbox parsing is implemented and validated under
[ADR 0031](../decisions/0031-native-raw-mi-mailbox-ingress.md). The opt-in
`--native-readback-mi` path forwards the unchanged final memory result to a
fixture-specific GDB adapter. It verifies the measured token/address/range and
32-byte hex payload, decodes little endian, then applies the existing native
mailbox and commit gates. Normalized attestations are rejected in this mode.
GDB collection, observed time and inspection stability remain host-owned.

The [evidence](../experiments/evidence/SN-017-readback-mi-summary.json) records
six initial guarded sessions, four real backend/owner-loss controls and six
final recovery sessions. Recovery passed 54 checkpoints, six native raw
readbacks, 60 RC checks and six MI pauses with ADC 3541. All six raw lines matched
the final memory results in their GDB logs byte-for-byte. Fixed CPU, analog,
mailbox, register and exchange records matched the previous cycle exactly.
Loss controls retained two commits without readback confirmation. PDF suppression
and startup PID retry hashes remain unchanged.

Seven CTest targets passed, including 218 parser checks. The final process suite
passed 23 raw MI cases, including signed/overflow time rejection, and regressions
passed 16 normalized readback, 15 session, nine RC and 18 ADC coordination cases.
All 17 Python unit tests passed, including raw capture identity and ambiguity.
Initial passing reports before strict timestamp conversion are preserved with
their original source/binary hashes. Final recovery uses the hardened parser.
See [protocol and reproduction](../../tests/headless/READBACK_MI.md).

SN-017 remains **in_progress**. This exact-format parser is not generic MI support
or bounded streaming transport; the reused token and host time do not prove
freshness or origin independently. Next extract bounded mailbox request/result
correlation while retaining host GDB ownership, the allowlist and separate joint
commit scheduling. ADRs 0014, 0027 and 0028 remain unchanged. No commit or remote
publication was performed.

## 2026-09-11: SN-017 seventeenth cycle closed

Bounded mailbox request/result correlation is implemented and validated under
[ADR 0032](../decisions/0032-bounded-mailbox-request-correlation.md). The opt-in
`--native-readback-request` path arms one native request at the final analog-ready
boundary after ADC confirmation. The runner supplies the fixed memory command
and a reserved token, then requires a matching raw reply within its original
1900 ms deadline. The host executes the command and post-read time query within
one remaining-time budget. Pending, unsolicited, duplicate, wrong-token and
late results cannot create a new joint commit.

The [evidence](../experiments/evidence/SN-017-readback-request-summary.json)
records six initial guarded sessions, four real backend/owner-loss controls and
six final recovery sessions. Recovery passed 54 checkpoints, six native requests,
60 RC checks and six MI pauses with ADC 3541. Every native request had exactly
one matching-token result in the GDB log, identical to the submitted raw line.
Fixed CPU, analog, mailbox, register and exchange records matched the preceding
cycle exactly. Real losses occurred before arming and retained two commits.
PDF suppression and startup PID retry hashes remain unchanged.

Seven CTest targets passed, including 224 parser checks. Fourteen request cases
passed, including timeout, delayed success, wrong/legacy token, duplicate arm,
pending commit and normalized bypass. Regression suites passed 23 raw MI,
16 normalized readback, 15 session, nine RC and 18 ADC coordination cases.
All 18 Python tests passed, including remaining-budget propagation. The initial
request report is preserved; the final test removes unrelated inherited branches.
See [protocol and reproduction](../../tests/headless/READBACK_REQUEST.md).

SN-017 remains **in_progress**. Tokens are unique only within the owned session
and may repeat after recreation; they do not authenticate the host. Deadline
checks occur on submission, while the host enforces the wait budget. Raw observed
time decoding and GDB ownership remain in the host. Next extract bounded raw
stopped-time validation for the mailbox request, retaining the allowlist and
separate commit scheduling. ADRs 0014, 0027 and 0028 remain unchanged. No commit
or remote publication was performed.

## 2026-09-11: SN-017 eighteenth cycle closed

Native raw elapsed-time validation is implemented and tested under
[ADR 0033](../decisions/0033-native-raw-stopped-time-validation.md). The opt-in
`--native-readback-time` path adds a time command/token to the mailbox request.
The host forwards raw memory, elapsed-time and completion records without a
normalized timestamp. The runner parses time with integer arithmetic, verifies
the completion token and final native boundary, and retains the original 1900 ms
acceptance deadline and separate commit. Python checks remain independent.

The [evidence](../experiments/evidence/SN-017-readback-time-summary.json) records
six initial guarded sessions, four real backend/owner-loss controls and six final
recovery sessions. Recovery passed 54 checkpoints, six timed readbacks, 60 RC
checks and six MI pauses with ADC 3541. All 18 raw memory/time/completion records
matched their GDB logs. Fixed CPU, analog, mailbox, register and exchange records
matched the preceding cycle exactly. Loss controls preceded arming and retained
two commits. PDF suppression and startup PID retry hashes remain unchanged.

Eight CTest targets passed, including 72 time-parser and 224 memory-parser checks.
The process suites passed 22 timed-readback, 14 request, 23 raw-memory,
16 normalized-readback, 15 session, nine RC and 18 ADC coordination cases.
All 18 Python unit tests passed. See
[protocol and reproduction](../../tests/headless/READBACK_TIME.md).

SN-017 remains **in_progress**. Output stream records have no MI token, so their
request association remains host-owned; validating the completion does not
establish authenticity of the stream or host. Only selected time/completion
records are parsed, not every monitor diagnostic. Next extract bounded final
inspection stability checks while retaining GDB transport in the host, the
allowlist and separate commit scheduling. ADRs 0014, 0027 and 0028 remain
unchanged. No commit or remote publication was performed.

## 2026-09-12: SN-017 nineteenth cycle closed

Native final register stability is implemented and validated under
[ADR 0034](../decisions/0034-bounded-final-register-stability.md). The opt-in
`--native-inspection` path forwards the existing before/after final r0/sp/lr MI
records. The runner validates their shape and 32-bit values, requires equality
after timed readback within the original mailbox deadline, and gates the final
commit on inspection confirmation. No extra GDB read is introduced.

The [evidence](../experiments/evidence/SN-017-inspection-summary.json) records six
initial guarded sessions, four real backend/owner-loss controls and six final
recovery sessions. Recovery passed 54 checkpoints, six native final inspections,
60 RC checks and six MI pauses with ADC 3541. All 12 submitted register records
matched the final pair in their GDB logs. Fixed CPU, analog, mailbox, register
and exchange records matched the preceding cycle exactly. Each final commit
followed native inspection verification. Loss controls retained two commits
without inspection confirmation. PDF suppression and startup PID retry hashes
remain unchanged.

Nine CTest targets passed, including 127 register-parser checks. Process suites
passed 11 inspection, 22 timed-readback, 14 request, 23 raw-memory,
16 normalized-readback, 15 session, nine RC and 18 ADC coordination cases.
All 18 Python unit tests passed. See
[protocol and reproduction](../../tests/headless/INSPECTION.md).

SN-017 remains **in_progress**. Native equality does not prove that the two
supplied frames came from distinct reads or were separated by 100 ms. Collection,
association and interval timing remain host-owned, as do the other PC/mailbox/
time/analog stability assertions. Next extract bounded ownership/correlation of
the final register pair while keeping GDB transport in the host, the allowlist
and separate commit scheduling. ADRs 0014, 0027 and 0028 remain unchanged.
No commit or remote publication was performed.

## 2026-09-12: SN-017 twentieth cycle closed

Native register-pair request correlation is implemented and tested under
[ADR 0035](../decisions/0035-native-register-pair-correlation.md). The opt-in
`--native-inspection-request` path emits a first fixed r0/sp/lr command after
timed readback, accepts its matching token, then emits the second command with
a distinct token. Only matching, unchanged second values verify inspection.
Pending operations, replayed/wrong tokens, legacy bypass, duplicates and late
results cannot create a new joint commit. Arming does not renew the original
mailbox deadline. The host executes commands and retains the 100 ms gap.

The [evidence](../experiments/evidence/SN-017-inspection-request-summary.json)
records six initial guarded sessions, four real backend/owner-loss controls and
six final recovery sessions. Recovery passed 54 checkpoints, six native register
pairs, 60 RC checks and six MI pauses with ADC 3541. All 12 correlated register
results matched their GDB logs in order and matched the values of the independent
host reference pair. Fixed CPU, analog, mailbox, register and exchange records
matched the preceding cycle exactly. Each final commit followed second-result
verification. Real loss controls retained two commits before arming. PDF
suppression and startup PID retry hashes remain unchanged.

Nine CTest targets passed, including 132 register-parser checks. The final
request suite passed 15 cases, and regressions passed 11 legacy inspection,
22 timed readback, 14 mailbox request, 23 raw memory, 16 normalized readback,
15 session, nine RC and 18 ADC coordination cases. All 18 Python tests passed.
Two initial request-test attempts failed because inherited case names altered
mailbox setup; both reports are preserved. The final suite isolates valid setup
from inspection failures. See [reproduction](../../tests/headless/INSPECTION_REQUEST.md).

SN-017 remains **in_progress**. The interval between reads remains host-enforced;
native correlation does not impose a minimum gap or authenticate the host.
Tokens may repeat after recreation. Next extract bounded native observation-
interval gating for the register pair without renewing the shared deadline.
Retain host GDB transport, the allowlist, separate commit scheduling and
ADRs 0014/0027/0028. No commit or remote publication was performed.

## 2026-09-12: SN-017 twenty-first cycle closed

Native observation-interval gating is implemented and validated under
[ADR 0036](../decisions/0036-native-observation-interval.md). The opt-in
`--native-inspection-interval` path withholds the second register request after
first-result acceptance. `release-inspection` exposes it only after at least
100 ms of native steady-clock time. Polling cannot renew the original 1900 ms
mailbox deadline; early results and invalid pending operations fail without a
new joint commit. The host sleeps according to native remaining-wait diagnostics.

The [evidence](../experiments/evidence/SN-017-inspection-interval-summary.json)
records six initial guarded sessions, four real backend/owner-loss controls and
six final recovery sessions. Recovery passed 54 checkpoints, six native releases,
60 RC checks and six MI pauses with ADC 3541. Native release intervals ranged
from 100765700 to 101295900 ns, all above the 100 ms minimum. No waiting snapshot
exposed the second request. All 12 correlated register responses matched GDB logs.
Fixed CPU, analog, mailbox, register and exchange records matched the previous
cycle exactly. Loss controls retained two commits before arming. PDF suppression
and startup PID retry hashes remain unchanged.

Nine CTest targets passed. Process suites passed 15 interval, 15 request,
11 legacy inspection, 22 timed-readback, 14 mailbox-request, 23 raw-memory,
16 normalized-readback, 15 session, nine RC and 18 ADC coordination cases.
All 18 Python unit tests passed. See
[protocol and reproduction](../../tests/headless/INSPECTION_INTERVAL.md).

SN-017 remains **in_progress**. The minimum is a monotonic wall-time interval
between native acceptance and command release, not virtual simulation time or
proof of host authenticity. Host GDB transport, release polling and the post-pair
virtual-time assertion remain necessary. Next extract bounded raw post-pair time
confirmation before final commit, retaining the shared deadline and allowlist.
ADRs 0014/0027/0028 remain unchanged. No commit or remote publication was performed.

## 2026-09-12: SN-017 twenty-second cycle closed

Native raw post-pair time confirmation is implemented and validated under
[ADR 0037](../decisions/0037-native-post-inspection-time.md). The opt-in
`--native-inspection-time` path leaves inspection pending after the second
register result and issues a fixed elapsed-time query with its own token.
Only its matching raw reply at 4027000 ns verifies inspection. The original
1900 ms mailbox deadline remains shared; final commit is separate.

The [evidence](../experiments/evidence/SN-017-inspection-time-summary.json)
records six initial guarded sessions, four real backend/owner-loss controls and
six recovery sessions. Recovery passed 54 fixed checkpoints, 60 RC checks and
six MI pauses with ADC 3541. All six post-pair time replies and 12 register
records matched GDB logs in order; final commits followed native time acceptance.
Fixed CPU, analog, mailbox, register and exchange records matched the previous
cycle. Loss controls retained two commits. PDF suppression and startup PID retry
hashes remain unchanged; all earlier evidence is preserved.

Nine CTest targets and 18 Python unit tests passed. Pipe suites passed 15 new
time-confirmation, 15 interval, 15 request, 11 legacy inspection, 22 timed
readback, 14 mailbox request, 23 raw memory, 16 normalized readback, 15 session,
nine RC and 18 ADC coordination cases. An initial batch referenced a nonexistent
RC script; the corrected RC and ADC commands passed. See
[protocol and reproduction](../../tests/headless/INSPECTION_TIME.md).

SN-017 remains **in_progress**. Host GDB transport and association of the untagged
time stream remain; tokens provide no authentication and may repeat after
recreation. Next extract the bounded final readback/inspection state from the
CLI into a testable native coordinator, preserving the wire protocol, shared
deadline and separate commit. ADRs 0014/0027/0028 and all capability restrictions
remain unchanged. No commit or remote publication was performed.

## 2026-09-12: SN-017 twenty-third cycle closed

Final readback/inspection state is extracted from the CLI into the private-state
`FixtureInspection` application coordinator under
[ADR 0038](../decisions/0038-fixture-inspection-coordinator.md). It owns activation,
tokens, deadline, validation, pending-operation policy and diagnostics. The CLI
retains transports, global pending checks, abort and actual joint commit.
The CLI shrank from 626 to 475 lines. This fixture-specific composition still
handles the existing text protocol; no general backend API is claimed.

The [evidence](../experiments/evidence/SN-017-inspection-coordinator-summary.json)
records six initial sessions, four real backend/owner-loss controls and six
recovery sessions with Renode/ngspice/GDB. Recovery passed 54 fixed checkpoints,
60 RC checks and six MI pauses with ADC 3541. Fixed circuit/CPU/mailbox/register
records and final-validation command/diagnostic values match cycle 22, excluding
wall interval measurements. All six raw post-pair confirmations and 12 register
records match GDB logs in order. Loss controls retained two joint commits.
PDF suppression and startup PID retry hashes are unchanged. The new coordinator
header is now included in each real execution's source hashes.

Ten CTest targets passed, including direct coordinator checks for independent
instances, delegation without argument consumption, legacy-mode isolation and
terminal failure. All 173 existing pipe cases and 18 Python unit tests passed.
See [reproduction](../../tests/headless/INSPECTION_COORDINATOR.md).

SN-017 remains **in_progress**. JSON field order changed; field names, values,
commands, deadlines and commit behavior are preserved. Host GDB transport and
untagged-stream association remain required. Next extract bounded ADC
preparation/helper coordination from the CLI into an application coordinator,
preserving single transfer, deadlines and process cleanup. ADRs 0014/0027/0028,
all capability restrictions and prior evidence remain intact. No commit or
remote publication was performed.

## 2026-09-12: SN-017 twenty-fourth cycle closed

ADC preparation and helper coordination are extracted into `FixtureAdc` under
[ADR 0039](../decisions/0039-fixture-adc-coordinator.md). It owns the prepared
input, confirmation count, pending state, original deadline and helper lifetime.
Read-only facts feed final inspection. The CLI retains worker/session ownership
and startup argument validation, and shrank from 475 to 413 lines. The helper
adapter and final inspection coordinator sources remain unchanged.

The [evidence](../experiments/evidence/SN-017-adc-coordinator-summary.json)
records six initial guarded sessions, four real backend/owner-loss controls and
six recovery sessions. Recovery passed 54 fixed checkpoints, 60 RC checks and
six MI pauses with ADC 3541. Each session prepared and confirmed one transfer,
without an implicit joint commit, and cleaned its helper with exit zero. Final
ADC diagnostics match cycle 23 except process IDs. CPU, analog, mailbox, register
and exchange records match exactly; raw final GDB replies retain their order.
Real loss controls retained two joint commits. PDF suppression and startup PID
retry hashes remain unchanged. All earlier evidence is preserved.

Eleven CTest targets, 194 protocol/process cases and 18 Python unit tests passed.
The process suite includes 21 actual helper cases covering cleanup, descendants,
runner loss and the original deadline. Direct C++ checks cover independent
coordinators, delegation, denied preparation and duplicate confirmation. See
[reproduction](../../tests/headless/ADC_COORDINATOR.md).

Count correction: cycle 23's aggregate of 183 was an arithmetic error; its
individual raw suites total 173. This cycle adds 21 process cases for 194.
Historical evidence files remain unchanged; this correction concerns only the
aggregate, not any test outcome.

SN-017 remains **in_progress**. Next extract bounded analog worker coordination
(initialization, catch-up and high/inspection sequencing) from the CLI while
preserving raw ingress, RC acceptance, deadlines and backend ownership. Host GDB
transport and untagged-stream association remain necessary. ADRs 0014/0027/0028
and all capability restrictions remain unchanged. No commit or remote
publication was performed.

## 2026-09-12: SN-017 twenty-fifth cycle closed

Analog worker sequencing is extracted into `FixtureAnalog` under
[ADR 0040](../decisions/0040-fixture-analog-coordinator.md). It owns initialization,
advance/high/inspection acceptance, pending state, RC checks and diagnostics.
The CLI still owns worker endpoints, session and joint commit scheduling; the
coordinator borrows endpoints and exposes read-only observations/eligibility.
The CLI shrank from 413 to 326 lines. Worker adapters, ADC and inspection
coordinators are unchanged. JSON RC field order changed; protocol values did not.

The [evidence](../experiments/evidence/SN-017-analog-coordinator-summary.json)
records six initial sessions, four real backend/owner-loss controls and six
recovery sessions. Recovery passed 54 fixed checkpoints, 60 RC checks and six
MI pauses with ADC 3541. Analog final counters and fixed CPU/analog/mailbox/
register/exchange records match cycle 24. Each session confirmed one ADC transfer
and cleaned its helper. Raw final GDB replies match logs in order. Loss controls
retained two joint commits. PDF suppression and startup PID retry hashes remain
unchanged; all earlier evidence is preserved.

Twelve CTest targets, 251 protocol/process cases and 18 Python unit tests passed.
The new direct pipe test proves borrowed endpoint lifetime and rejection without
writing or committing. Additional suites passed 13 analog command, 25 ingress
and 19 exchange cases, alongside the preceding 194 cases. See
[reproduction](../../tests/headless/ANALOG_COORDINATOR.md).

SN-017 remains **in_progress**. Next extract bounded Renode execution coordination
(start/readiness, cancellation and result acceptance) from the CLI, preserving
grant accounting, debug interaction and transport deadlines. This remains a
fixture-specific protocol composition. ADRs 0014/0027/0028 and all capability
restrictions remain intact. No commit or remote publication was performed.

## 2026-09-12: SN-017 twenty-sixth cycle closed

Renode execution and shared debugger coordination are extracted into
`FixtureExecution` under [ADR 0041](../decisions/0041-fixture-execution-coordinator.md).
It owns readiness/cancellation/debug readers, flags and deadlines, borrowing the
CLI-owned control channel. Shared debug state remains in the same coordinator.
The CLI retains startup, global pending gates, session and actual commit, and
shrank from 326 to 139 lines. Adapters and other coordinators are unchanged.

The [evidence](../experiments/evidence/SN-017-execution-coordinator-summary.json)
records six initial sessions, four real backend/owner-loss controls and six
recovery sessions. Recovery passed 54 fixed checkpoints, 60 RC checks and six
joint pauses with ADC 3541. Final start/cancel/notify counters and fixed
CPU/analog/mailbox/register/exchange records match cycle 25. Asynchronous progress
sampling issued one or two commands (previously one); that timing-dependent
count is preserved separately from fixed virtual boundaries. Each session
confirmed one ADC transfer and cleaned its helper. Loss controls retained two
commits. PDF suppression and startup PID retry hashes remain unchanged.

Thirteen CTest targets, 358 protocol/process cases and 18 Python tests passed.
Direct pipe checks verify borrowed channel lifetime, independent instances and
premature cancellation without writes or commits. One ingress invocation lacked
a reference; the next used summary JSON instead of raw grant text. Its failed
report remains preserved; the corrected raw-grant run passed all 15 cases.
See [reproduction](../../tests/headless/EXECUTION_COORDINATOR.md).

SN-017 remains **in_progress**. Next extract remaining command dispatch and global
pending/commit gates into a bounded application session, retaining protocol,
endpoint ownership, deadlines and separate commit. This remains fixture-specific;
ADRs 0014/0027/0028 and all capability restrictions remain intact. No commit or
remote publication was performed.

## 2026-09-12: SN-017 twenty-seventh cycle closed

Dispatch and global pending/commit gates now belong to `FixtureSession` under
[ADR 0042](../decisions/0042-fixture-application-session.md). The session owns
joint state and the four unchanged coordinators. It exposes read-only state and
explicit reply/exit dispositions. The CLI retains validated startup, endpoint
ownership and text I/O, and shrank from 139 to 60 lines. Global pending rejection
still precedes every handler, including quit; actual commit remains separate.

The [evidence](../experiments/evidence/SN-017-application-session-summary.json)
records six initial guarded sessions, four real backend/owner-loss controls and
six recovery sessions. Recovery passed 54 fixed checkpoints, 60 RC checks and
six joint pauses with ADC 3541. Times, CPU/mailbox/register/exchange state, analog
sample counts and final start/cancel/notify counters match cycle 26. Two analog
voltages in one recreated session differ by less than 2e-14 V, within the existing
10 microvolt tolerance; both measured differences are retained rather than
claiming byte-identical voltage results. Progress polling counts remain variable.
Each session confirmed one ADC transfer and cleaned its helper; raw final GDB
replies match logs in order. Loss controls retained two commits. PDF suppression
and startup PID retry hashes remain unchanged, as do all coordinator/adapter
sources. All prior evidence is preserved.

Fourteen CTest targets, 358 protocol/process cases and 18 Python tests passed.
The direct composed-session test covers isolated state, separate commit,
terminal failure, exit disposition and pending-worker rejection before commit,
quit, begin and analog input. See
[reproduction](../../tests/headless/APPLICATION_SESSION.md).

SN-017 remains **in_progress**, pending acceptance audit. Next map its acceptance
criteria to the extracted runner/contracts and evidence, explicitly identify
remaining Python orchestration, and decide whether bounded extraction can close
or requires one concrete gap addressed. Do not equate this fixture-specific
session with a production application, autonomous general runner or new backend
capabilities. ADRs 0014/0027/0028 remain intact. No commit or remote publication
was performed.

## 2026-09-12: SN-017 bounded acceptance audit closed

SN-017 is **done for the declared headless composition** under
[ADR 0043](../decisions/0043-bounded-headless-extraction-acceptance.md).
The [acceptance audit](../experiments/SN-017-acceptance.md) maps the backlog
criterion to native core/contracts, adapters, process supervision, application
session and the real-engine fixture driver. Python still owns preparation,
GDB/MI transport, fixture scheduling and evidence collection. This closure does
not claim native-only autonomous orchestration or a production application.

The [audit evidence](../experiments/evidence/SN-017-acceptance-summary.json)
revalidates all recorded code/binary, raw-report and GDB-log hashes from cycle 27.
Its 14 CTest targets, 358 protocol/process cases, 18 Python tests, six initial
and six recovery sessions, 54 recovery checkpoints, four real fault controls,
60 RC checks and ADC 3541 remain the functional acceptance evidence. This was a
read-only implementation/evidence audit followed by documentation changes; no
new engine run or CubeIDE matrix is claimed. Voltage-roundoff differences,
variable progress polling, historical failed reports, PDF suppression and PID
retry remain explicitly recorded and unchanged.

SN-018 is **ready**. Next establish a bounded reproducibility/performance baseline
for this exact composition, with fixed inputs and declared latency/error/memory
measurement methodology. Do not add capabilities or infer classroom readiness.
Complete M2/product orchestration, project loading, instruments, GUI, packaging
and additional validated profiles remain separate pending work. ADRs
0014/0027/0028 and classroom targets are unchanged. Remote issue synchronization
is deferred; no commit or publication was performed. Older cycle entries above
are historical and do not override this acceptance decision.
