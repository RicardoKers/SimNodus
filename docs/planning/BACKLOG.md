# Backlog

Latest slice (2026-09-15): SN-021 [source-preserving name revision](../experiments/SN-021-revision.md)
changes one display-name token with full revalidation and rebuilt provenance.
Acceptance: 11 cases/33 requests and 39 Windows CTests. SN-021 stays **in_progress**;
compiler ingress/readiness, lowering with real-engine evidence and safe overwrite
ownership remain pending. Further editor features are not selected by ADR 0066.

Latest slice (2026-09-15): SN-021 [native project acquisition](../experiments/SN-021-acquisition.md)
loads one physically captured document into an owned validated graph without
resource I/O or save authority. Acceptance: 17 cases (15 local passes/two explicit
skips), 38 Windows CTests. SN-021 stays **in_progress**; source-preserving revision/
editing, safe overwrite ownership and compilation remain pending under ADR 0065.

Latest slice (2026-09-15): SN-021 [overwrite ownership boundary](../experiments/SN-021-overwrite-boundary.md)
accepts seven physical counterexamples and 37 Windows CTests. Unsafe replacement
candidates are rejected; create-only saving remains supported. SN-021 stays
**in_progress**. Next: bounded native project acquisition; safe overwrite ownership,
editing and compilation remain pending under ADR 0064.

Latest slice (2026-09-15): SN-021 [atomic project creation](../experiments/SN-021-save-create.md)
persists exact validated bytes without overwriting an existing/concurrent entry.
Acceptance: 25 boundary checks, 14 physical cases with one local privilege skip,
36 Windows CTests. SN-021 stays **in_progress**; safe overwrite ownership, path
acquisition/editing and source-preserving compilation remain pending.

Latest slice (2026-09-15): SN-021 [native source connectivity](../experiments/SN-021-graph.md)
loads the validated definition graph with owned domain values and exhaustive
source mapping, retaining all metadata. Acceptance: 20 cases/56 comparisons and
34 Windows CTests. SN-021 remains **in_progress**; safe atomic persistence and
source-preserving compilation remain pending, with path acquisition/editing separate.

Latest slice (2026-09-15): SN-021 [native project declarations](../experiments/SN-021-project.md)
composes the declarative baseline with target, firmware and temporal metadata.
Acceptance: 20 cases/106 native-reference comparisons and 32 Windows CTests.
SN-021 remains **in_progress**; native source graph loading, atomic saving and
source-preserving compilation still require implementation and evidence.

Latest slice (2026-09-14): SN-021 [native resource links](../experiments/SN-021-links.md)
implements typed asset/source associations over one captured topology/lock document.
Focused acceptance: 19 cases/57 reference comparisons. Full regressions and publication
are in the report. SN-021 remains **in_progress**; project composition, graph loading,
atomic saving and source-preserving compilation remain pending.

Latest slice (2026-09-14): SN-021 [native resource lock metadata](../experiments/SN-021-lock.md)
implements lock 0.1 declaration rules and canonical inventory fingerprints without
resource I/O. Focused acceptance: 23 cases/135 metadata comparisons/137 digest cases.
Full regressions and publication are in the report. SN-021 stays **in_progress**;
next port typed resource links and project semantics before native graph loading.

Latest slice (2026-09-14): SN-021 [native descriptor bindings](../experiments/SN-021-bindings.md)
implements topology 0.3 catalog, map and exact interval semantics over owned syntax.
Focused acceptance: 22 cases/157 native-reference comparisons; full regressions,
failures and publication are recorded in the report. SN-021 stays **in_progress**;
next port resource lock/link and project semantics before native graph loading.

Latest slice (2026-09-14): SN-021 [native exact parameters](../experiments/SN-021-parameters.md)
implements topology 0.2 quantities, scoped overrides and immutable per-occurrence
inspection. Focused acceptance: 22 cases/315 reference comparisons. The report
records full regressions and publication. SN-021 stays **in_progress**; next port
descriptor/model declaration semantics, with full project loading still pending.

Latest slice (2026-09-14): SN-021 [native topology semantics](../experiments/SN-021-topology.md)
implements the complete preserved topology 0.1 rules over immutable captured
syntax. The report records differential/adversarial acceptance and publication.
Full project/topology 0.3 semantics remain Python-owned; next port exact parameters
and scoped overrides. SN-021 stays **in_progress**; no resources are opened.

Latest slice (2026-09-14): SN-021 native
[declaration syntax ingress](../experiments/SN-021-ingress.md) is locally accepted:
95 native assertions, 209 differential/adversarial cases and 20 Windows CTests.
This preserves bytes/tokens without opening resources or granting schema validity.
SN-021 stays **in_progress**; next validate schema semantics before graph creation.
Repository checker passed for 488 text files; historical evidence is preserved.
[PR #22](https://github.com/RicardoKers/SimNodus/pull/22), starting at source
`0dfd974`, records final hosted acceptance and protected-main integration.

Latest native slice (2026-09-13): SN-021 remains **in_progress** with the
[C++20 snapshot API](../experiments/SN-021-native-resources.md) under ADR 0053.
Typed safety validation and Windows physical verification are native; complete
JSON/lock parsing remains caller-owned. Native project graph loading, saving,
source compilation and runtime acceptance remain pending. Actual long-name/8.3
and case-policy rejection evidence supplements the preserved Python baseline.
Source `bf3e05b` was pushed on 2026-09-14; follow
[PR #21](https://github.com/RicardoKers/SimNodus/pull/21) for final required checks
and protected-main squash integration. This native slice does not close SN-021.
The hosted temporary-root alias fixture needed a follow-up; its failed run and
local corrected result are recorded in the native report. Production is unchanged.
Follow-up `5f23db4` passed both hosted Foundation jobs: 32/32 native filesystem
cases on Windows, 18 Windows and 13 Linux CTest entries. The snapshot slice is
accepted; native declaration ingress/graph construction is the next bounded work.

Latest implementation (2026-09-13): SN-021 is **in_progress** with explicit
[bounded local resource snapshots](../experiments/SN-021-local-resources.md)
under ADR 0052. Windows/NTFS reference verification precedes native loading;
atomic saving, source interfaces, compilation and runtime acceptance remain
pending. Local passes/skips and required hosted evidence are recorded separately.
All earlier accepted profiles and SN-044 remain unchanged. Both Foundation jobs
passed for source `cb6fc8d`; [PR #20](https://github.com/RicardoKers/SimNodus/pull/20)
records final checks and protected-main integration. No issue sync/release.

Final local hardening preserves original directory spellings in handle-cache
keys: 32 resource cases, 28 passes/four skips, 94 schema passes and 16 CTest passes.

Latest local acceptance (2026-09-12): SN-016 is done for ADR 0014, and SN-017
is **done for the bounded headless composition** under
[ADR 0043](../decisions/0043-bounded-headless-extraction-acceptance.md).
The [audit](../experiments/SN-017-acceptance.md) records native implementation,
real-engine evidence and remaining Python orchestration. SN-018 is **done for the
bounded local baseline** under [ADR 0044](../decisions/0044-bounded-baseline-acceptance.md).
The [acceptance audit](../experiments/SN-018-acceptance.md) maps fixed scenarios
of one fixture to latency/error/memory evidence, preserving the inconclusive
predecessor and all limitations. SN-020 is now **done for the declaration baseline**
under [ADR 0051](../decisions/0051-project-declaration-baseline.md):
[acceptance](../experiments/SN-020-acceptance.md), 94 schema tests, 15 CTest entries,
native Debug build and 25 preserved schema hashes. Earlier slices include the
[topology draft](../experiments/SN-020-topology.md) and
[exact parameters/overrides](../experiments/SN-020-parameters.md): 29 tests and
valid/invalid CLI fixtures passed. The [0.3 symbol/model bindings](../experiments/SN-020-bindings.md)
passed all 46 tests. The [resource lock metadata](../experiments/SN-020-resource-lock.md)
passed 64 tests, with lexical paths and a specified future physical containment
gate. Typed descriptor references and project/board/firmware/temporal declarations
are now included in the accepted baseline. The owner authorizes coherent validated
commits and protected-main integration/push; issues and releases are excluded.
General unpaced debugging and production readiness remain unapproved. Remote
issue synchronization awaits an authorized publication cycle.

States: `done`, `in_progress`, `ready`, `planned`, `blocked`. Priority: P0 critical path, P1 near-term, P2 later. Dependencies are task IDs. Production kernel/application implementation has not started.

`SN-001` is the repository-foundation task. SN-010 completed dependency selection and startup checks; SN-011 passed the standalone RC/lifecycle experiment. Coupled engine and application features remain unimplemented. Keep the Markdown status and GitHub issue status consistent; do not maintain independent competing queues.

## Foundation and technical proof

| ID | Priority | State | Task | Depends on | Acceptance evidence |
|---|---|---|---|---|---|
| SN-001 | P0 | done | Establish architecture, planning, license, scaffolding, checks | â€” | English files and local structural verification |
| SN-010 | P0 | done | Inventory environment and pin backend revisions | SN-001 | [Version, provenance, hashes, setup and startup report](../experiments/SN-010-results.md); integration limitations explicit |
| SN-011 | P0 | done | Run E-01 standalone ngspice | SN-010 | [Eight real cases, three local runs, numeric/lifecycle evidence](../experiments/E-01-results.md); pre-init crash remains an unsupported route |
| SN-019 | P0 | done | Adapt the pinned Renode external client for native Windows | SN-010 | [Native build, verified loopback server, real time/reconnect and fault evidence](../experiments/SN-019-results.md); two complete 20-case local runs |
| SN-012 | P0 | done | Run E-02 standalone Renode | SN-010, SN-019 | [Rebuildable ELF, offline C8 profile, real timed GPIO/input/EXTI evidence and coverage audit](../experiments/E-02-results.md) in two local runs |
| SN-013 | P0 | done | Define supported temporal capability profile | SN-011, SN-012 | [Evidence-bounded units, timing, operating modes, failure semantics, and predeclared E-03 gates](../architecture/TEMPORAL_CAPABILITY_PROFILE.md) |
| SN-014 | P0 | done | Run E-03 coupled GPIO and digital feedback | SN-013 | [54 fresh real-backend cases: replay passed; sampled delays measured and explicitly approximate](../experiments/E-03-results.md) |
| SN-015 | P0 | done | Run E-04 ADC path | SN-012, SN-014 | [Focused microvolt-to-firmware path, timing, sampling, boundaries, mapping, ramp, and limitations](../experiments/E-04-results.md) |
| SN-016 | P0 | done | Run E-05 GDB and CubeIDE coordination | SN-014 | [Original contract](../../tests/experiments/debugging/README.md), [final evidence](../experiments/evidence/E-05-final-gate-summary.json) and [case-by-case gate review](../experiments/E-05-gate-review.md); [ADR 0014](../decisions/0014-bounded-cooperative-debugging.md) accepts the bounded cooperative profile, with explicit pacing, command restrictions, no false joint commit and fresh recovery. General unpaced behavior remains unapproved. |
| SN-017 | P0 | done | Extract tested kernel/adapters from experiments | SN-014, SN-015, SN-016 | Headless runner and contract tests using real engines; [bounded acceptance audit](../experiments/SN-017-acceptance.md) |
| SN-018 | P1 | done | Establish reproducibility/performance baseline | SN-017 | [Bounded local acceptance](../experiments/SN-018-acceptance.md): fixed scenarios of one fixture, latency/error/memory measurements and corrected observer control. No product performance or portable-setup acceptance |

## Teaching MVP

SN-022 through SN-024 should use the [desktop UX criteria](../architecture/DESKTOP_UX.md)
and [ADR 0049](../decisions/0049-desktop-ux-and-measurements.md) for toolkit evaluation,
preview/panel behavior, independent analyzer presentation and shared contextual
measurements. Their implementation states are unchanged; advanced analysis is
future direction, not a new M3 delivery commitment.

| ID | Priority | State | Task | Depends on | Acceptance evidence |
|---|---|---|---|---|---|
| SN-020 | P1 | done | Specify circuit/component schema and validation | SN-013 | [Accepted declaration baseline](../experiments/SN-020-acceptance.md) under ADR 0051: 94 schema tests, 15 CTest entries, explicit board/firmware/temporal/resource declarations; physical loading and runtime verification remain SN-021 |
| SN-021 | P1 | in_progress | Implement project loading/saving and circuit compilation | SN-020 | Portable paths, atomic save, source mapping, independent graph. [Local reference](../experiments/SN-021-local-resources.md) and [native byte snapshots](../experiments/SN-021-native-resources.md) implemented; full project loading/saving/compilation remain pending |
| SN-022 | P1 | planned | Select Qt modules and worker boundary | SN-014 | Small UI experiment, licensing inventory, crash-handling decision |
| SN-023 | P1 | planned | Build minimal Windows editor | SN-017, SN-021, SN-022 | R/C/LED/switch/source/GND/MCU, wiring, properties, undo, save/open |
| SN-024 | P1 | planned | Add scope, logic view, and UART terminal using shared instrumentation | SN-018, SN-023 | Responsive views over common committed signal/event records; units/provenance; exportable full traces; [architectural direction](../architecture/DEBUGGING.md#instruments) |
| SN-025 | P1 | planned | Add core teaching diagnostics | SN-023 | Invalid wiring, unsupported mode, floating/undefined input explained |
| SN-026 | P0 | planned | Prepare three reproducible lesson projects | SN-024, SN-025 | Firmware sources, expected traces, guides, fidelity notes |
| SN-027 | P0 | planned | Package and test Windows classroom candidate | SN-026 | Clean/offline install rehearsal; dependency notices; known limitations |
| SN-028 | P0 | planned | Conduct January classroom readiness review | SN-027 | Owner go/no-go with exact release and fallback materials |

## Reuse, extensions, and public project

| ID | Priority | State | Task | Depends on | Acceptance evidence |
|---|---|---|---|---|---|
| SN-030 | P1 | planned | Implement project-local subcircuits | SN-021, SN-023 | Two RC instances, independent state, explicit ports, recursion rejection |
| SN-031 | P2 | planned | Implement user library and dependency locking | SN-030 | Conflict/update behavior; portable exported package |
| SN-032 | P2 | planned | Expand validated peripheral and component coverage | SN-018 | Feature-specific firmware reports and licensed models |
| SN-033 | P2 | planned | Add node/peripheral/software inspection | SN-024, SN-032 | Timestamped circuit/firmware correlation through shared instrumentation; configured/observed/derived fields distinguished; explicit unavailable values |
| SN-034 | P2 | planned | Design and implement WASM SDK | SN-031 | Versioned capabilities, execution limits, malicious-package tests |
| SN-035 | P2 | planned | Evaluate HDL integration | SN-017 | ADR chooses one integration path; reproducible HDL example |
| SN-036 | P2 | planned | Ship a Linux application build | SN-027 | Linux integration tests, packaging, documentation, clean-machine run |
| SN-040 | P1 | done | Resolve GitHub owner/name and publication review | SN-001 | RicardoKers/SimNodus and Ricardo Kerschbaumer confirmed; privacy review completed |
| SN-041 | P1 | done | Publish initial public source repository | SN-040 | Public first commit; Windows/Ubuntu CI passed; issue links, branch protection, and private security reporting configured |
| SN-042 | P1 | done | Reclaim extracted experiment runtimes without losing ongoing work | SN-010 | [243 caches / 62.973 GiB removed; 10,690 protected files verified unchanged; dependencies verified; bounded preview-first cleanup command](../development/BUILD_STORAGE.md) |

## Architecture maintenance

| ID | Priority | State | Task | Depends on | Acceptance evidence |
|---|---|---|---|---|---|
| SN-044 | P1 | done | Record desktop UX and shared visual instrumentation direction (documentation only) | SN-043 | [Desktop UX](../architecture/DESKTOP_UX.md) and [ADR 0049](../decisions/0049-desktop-ux-and-measurements.md) reconcile responsibilities, contextual measurements, MVP/future scope and open choices with ADR 0027; repository checker passed (444 text files) and whitespace check passed; no implementation or SN-020 advancement |
| SN-043 | P1 | done | Record shared instrumentation, decoders, and MCU independence (documentation only) | SN-001 | [ADR 0027](../decisions/0027-shared-instrumentation.md), [ADR 0028](../decisions/0028-mcu-platform-independence.md), and reconciled [instrument design](../architecture/DEBUGGING.md#instruments); existing repository checker passes; no implementation or SN-017 changes |

## GitHub issue mapping

| Task | Issue |
|---|---|
| SN-010 | [#1 â€” Toolchain and backend revisions](https://github.com/RicardoKers/SimNodus/issues/1) |
| SN-011 | [#2 â€” Standalone ngspice](https://github.com/RicardoKers/SimNodus/issues/2) |
| SN-012 | [#3 â€” Standalone Renode](https://github.com/RicardoKers/SimNodus/issues/3) |
| SN-013 | [#4 â€” Temporal capability profile](https://github.com/RicardoKers/SimNodus/issues/4) |
| SN-014 | [#5 â€” GPIO and feedback causality](https://github.com/RicardoKers/SimNodus/issues/5) |
| SN-015 | [#6 â€” ADC path](https://github.com/RicardoKers/SimNodus/issues/6) |
| SN-016 | [#7 â€” Coordinated debugging](https://github.com/RicardoKers/SimNodus/issues/7) |
| SN-017 | [#8 â€” Verified headless kernel](https://github.com/RicardoKers/SimNodus/issues/8) |
| SN-018 | [#9 â€” Reproducibility and performance](https://github.com/RicardoKers/SimNodus/issues/9) |
| SN-019 | [#11 â€” Native Windows Renode client](https://github.com/RicardoKers/SimNodus/issues/11) |

## Definition of ready

A task has bounded scope, dependencies, acceptance evidence, and required inputs. Missing laboratory details do not block standalone backend experiments.

## Definition of done

The result exists, relevant checks passed, limitations and evidence are recorded, licensing is accounted for, and status/ADRs are updated. A design document, mock, or passing repository check never counts as a working engine feature.

SN-017 sixth cycle: native progress/joint-notification emission validated; see
[ADR 0019](../decisions/0019-native-debug-command-emission.md). Status remains
**in_progress**. Next extract complete debug-response ingress, then orchestration.

SN-017 seventh cycle: opt-in complete debug reply ingress validated under
[ADR 0020](../decisions/0020-native-debug-reply-ingress.md). Status remains
**in_progress**; next consolidate bounded fixture orchestration.

SN-017 eighth cycle: composite grant/start and observed-stop/cancel transitions
validated under [ADR 0021](../decisions/0021-composite-fixture-grant-transitions.md).
Status remains **in_progress**; next extract bounded analog catch-up after CPU
acknowledgement before further orchestration.

SN-017 ninth cycle: native analog catch-up command emission validated under
[ADR 0022](../decisions/0022-native-analog-command-channel.md); status remains
**in_progress**. Next extract bounded analog reply ingress. Preserve the failed
startup-read report and the bounded retry correction.

SN-017 tenth cycle: exclusive native analog reply ingress validated under
[ADR 0023](../decisions/0023-native-analog-reply-ingress.md). Status remains
**in_progress**; next select the smallest bounded exchange/inspection coordination
step, preserving the separate commit and the harness reference.

SN-017 eleventh cycle: bounded native high/inspection coordination validated under
[ADR 0024](../decisions/0024-native-fixture-exchange-inspection.md). Status remains
**in_progress**; next extract bounded ADC boundary-input coordination.

SN-017 twelfth cycle: bounded ADC preparation/confirmation validated under
[ADR 0025](../decisions/0025-bounded-adc-input-coordination.md). Status remains
**in_progress**; next extract bounded ADC helper invocation/result ingress.

SN-017 thirteenth cycle: native ADC helper invocation/result ingress validated
under [ADR 0026](../decisions/0026-native-adc-helper-process.md). Status remains
**in_progress**; next extract bounded analytical RC validation into native
coordination before further GDB/commit scheduling.

SN-017 fourteenth cycle: native bounded RC analytical acceptance validated under
[ADR 0029](../decisions/0029-native-rc-trajectory-validation.md). Status remains
**in_progress**; next select the smallest bounded native readback-verification
step, preserving the GDB allowlist and separate joint commit scheduling.

SN-017 fifteenth cycle: native final mailbox verification and final commit
prerequisite validated under
[ADR 0030](../decisions/0030-bounded-native-mailbox-readback.md). Status remains
**in_progress**; next extract bounded raw readback ingress, retaining host GDB
ownership, the allowlist and separate joint commit scheduling.

SN-017 sixteenth cycle: exact raw MI mailbox parsing validated under
[ADR 0031](../decisions/0031-native-raw-mi-mailbox-ingress.md). Status remains
**in_progress**; next extract bounded mailbox request/result correlation while
retaining host GDB ownership, the allowlist and separate joint commit scheduling.

SN-017 seventeenth cycle: bounded mailbox request correlation validated under
[ADR 0032](../decisions/0032-bounded-mailbox-request-correlation.md). Status remains
**in_progress**; next extract bounded raw stopped-time validation for the mailbox
request, retaining host GDB ownership and separate joint commit scheduling.

SN-017 eighteenth cycle: native raw elapsed-time validation passed under
[ADR 0033](../decisions/0033-native-raw-stopped-time-validation.md). Status remains
**in_progress**; next extract bounded final inspection stability checks, retaining
host GDB transport, the allowlist and separate joint commit scheduling.

SN-017 nineteenth cycle: native final register stability passed under
[ADR 0034](../decisions/0034-bounded-final-register-stability.md). Status remains
**in_progress**; next extract bounded ownership/correlation of the final register
pair, retaining host GDB transport, the allowlist and separate joint commit.

SN-017 twentieth cycle: native sequential register-pair correlation passed under
[ADR 0035](../decisions/0035-native-register-pair-correlation.md). Status remains
**in_progress**; next extract bounded native observation-interval gating without
renewing the shared deadline, retaining host GDB transport and separate commit.

SN-017 twenty-first cycle: native minimum observation interval passed under
[ADR 0036](../decisions/0036-native-observation-interval.md). Status remains
**in_progress**; next extract bounded raw post-pair time confirmation before final
commit, preserving the original shared deadline and host GDB transport.

SN-017 twenty-second cycle: native raw post-pair time confirmation passed under
[ADR 0037](../decisions/0037-native-post-inspection-time.md); see
[evidence](../experiments/evidence/SN-017-inspection-time-summary.json). Status
remains **in_progress**. Next extract bounded final readback/inspection state from
the CLI into a native coordinator without changing protocol or capabilities.

SN-017 twenty-third cycle: final inspection application coordinator extracted
and validated under [ADR 0038](../decisions/0038-fixture-inspection-coordinator.md);
see [evidence](../experiments/evidence/SN-017-inspection-coordinator-summary.json).
Status remains **in_progress**. Next extract bounded ADC preparation/helper
coordination while retaining single transfer, deadlines and process cleanup.

SN-017 twenty-fourth cycle: bounded ADC application coordinator extracted under
[ADR 0039](../decisions/0039-fixture-adc-coordinator.md); see
[evidence](../experiments/evidence/SN-017-adc-coordinator-summary.json). Status
remains **in_progress**. Next extract bounded analog worker coordination while
preserving raw ingress, RC acceptance, deadlines and backend ownership.

SN-017 twenty-fifth cycle: bounded analog coordinator extracted under
[ADR 0040](../decisions/0040-fixture-analog-coordinator.md); see
[evidence](../experiments/evidence/SN-017-analog-coordinator-summary.json). Status
remains **in_progress**. Next extract bounded Renode execution coordination,
retaining grant accounting, debug interaction and native deadlines.

SN-017 twenty-sixth cycle: bounded execution/debug coordinator extracted under
[ADR 0041](../decisions/0041-fixture-execution-coordinator.md); see
[evidence](../experiments/evidence/SN-017-execution-coordinator-summary.json).
Status remains **in_progress**. Next extract remaining dispatch and global
pending/commit gates into a bounded application session.

SN-017 twenty-seventh cycle: bounded application session extracted under
[ADR 0042](../decisions/0042-fixture-application-session.md); see
[evidence](../experiments/evidence/SN-017-application-session-summary.json). Status
remains **in_progress**, pending an acceptance audit against the extracted native
runner/contracts, evidence and remaining Python orchestration.

SN-017 acceptance audit: **done for the bounded composition** under
[ADR 0043](../decisions/0043-bounded-headless-extraction-acceptance.md). Native
contracts/session are validated; host GDB transport and fixture scheduling remain
explicit. SN-018 is ready. Earlier cycle notes above describe historical status.
