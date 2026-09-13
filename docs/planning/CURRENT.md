# Current state

Updated: 2026-09-13.

## Latest implementation: SN-021 bounded local snapshots

SN-021 is **in_progress**. Its first coherent slice implements an explicit
[Windows/NTFS resource verifier](../architecture/LOCAL_RESOURCE_VERIFICATION.md)
under [ADR 0052](../decisions/0052-bounded-local-resource-snapshots.md), separate
from the unchanged SN-020 declaration validators. It traverses by parent handles,
rejects reparse points/aliases, checks bounded same-handle sizes/hashes and returns
immutable bytes by dependency/resource ID. No resource is rendered or executed.

The [report](../experiments/SN-021-local-resources.md) and
[evidence](../experiments/evidence/SN-021-local-resources-final-summary.json) record
31 resource cases: 27 local passes and four explicit skips (non-Windows rejection,
two unavailable symlink privileges, unavailable 8.3 name generation). All 94
declaration regressions remain unchanged. Hosted Windows must execute the symlink
cases before integration; short-name generation coverage remains host-dependent.

The fresh native Debug build and all 16 CTest entries passed; 40 historical
schema/fixture/SN-044 hashes matched. Repository checker: 467 text files passed;
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
