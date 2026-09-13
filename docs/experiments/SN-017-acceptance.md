# SN-017 acceptance audit

Date: 2026-09-12. Result: **done for the bounded headless extraction** under
[ADR 0043](../decisions/0043-bounded-headless-extraction-acceptance.md).
The [backlog](../planning/BACKLOG.md) criterion is: "Headless runner and contract
tests using real engines". The implemented runner is a composition of native
C++ components and the declared Python/C# fixture driver. This audit does not
claim an autonomous all-C++ executable or a production simulation application.

## Criteria and evidence

| Requirement | Implemented result | Acceptance evidence |
|---|---|---|
| Headless execution | Root-CMake RC worker, session CLI and Windows process supervisor, driven by the explicit fixture harness without Qt | [Complete reproduction](../../tests/headless/APPLICATION_SESSION.md); six initial and six recovery sessions |
| Native contracts and time ownership | `JointSession` distinguishes granted, observed, CPU-acknowledged and jointly committed state; endpoint and RC/ADC/readback checks remain separate | [Core](../../src/core/joint_session.hpp), 14 CTest targets, 358 protocol/process cases |
| Extracted application coordination | `FixtureSession` composes execution, analog, ADC and inspection, applying global pending/commit gates | [Session](../../src/application/fixture_session.hpp), direct composition tests and real native transition records |
| Real integration | Patched Renode 1.16.1, ngspice 47, GDB 15.2.90.20241229, owned firmware and helper | [Cycle 27 evidence](evidence/SN-017-application-session-summary.json): 54 recovery checkpoints, 60 RC checks, six pauses, ADC 3541 |
| Failure and recovery | No new commit on backend/owner loss; fresh sessions and owned helper cleanup | Four real loss controls retain two commits; six recreated/recovery sessions pass; process/adversarial suites retained |
| Fidelity and capability limits | Same paced ADR 0014 single-CPU GPIO/RC and boundary-sampled ADC profile | Exact fixed time/digital records; two voltage differences below 2e-14 V, within 10 microvolt tolerance; no broadened peripheral or unpaced claim |
| Reproducibility and preservation | Explicit commands, configuration, source/binary hashes, raw logs and failed predecessors | [Audit record](evidence/SN-017-acceptance-summary.json); current source, binary, report and GDB-log hashes revalidated |
| Dependency and licensing boundary | Original extraction remains MIT; dependencies externally prepared and pinned | [Inventory](../research/DEPENDENCIES.md), [licensing record](../development/LICENSING.md), ADR 0014 exact runtime overrides; no new third-party copy or distribution |

## Remaining host responsibilities

The harness selects trusted dependencies and fixed firmware/platform assets,
configures the measured profile, invokes native supervisors, chooses the fixture
sequence and drives GDB actions. Python owns GDB/MI transport, command relay
coordination, untagged time-stream association, independent reference assertions,
fault injection and evidence collection. The native session accepts/rejects
transitions and owns its extracted coordination state; it does not autonomously
choose the next fixture action. The supervisor and helper adapters own their
measured native process lifetimes.

These are disclosed runtime boundaries of the accepted composition. Eliminating
Python is not the backlog's acceptance criterion. ADR 0015's temporary helper
boundary remains an experimental interface, not a selected production IPC API.
The audit closes the extraction task without relabelling that helper as a general
orchestration engine. Further orchestration replacement needs its own bounded
acceptance work; it must not silently change this reference profile.

## Validation provenance

No code or engine input changed during this audit. Revalidated the hashes of
the immediately preceding cycle's sources, binaries, raw reports and GDB logs;
all match the workspace. The audit reuses those actual real-engine runs and
358 protocol/process cases, 14 CTest targets and 18 Python tests. It does not
claim a new run or a fresh CubeIDE matrix. E-05's original broader gate remains
[separately recorded](E-05-gate-review.md); raw extraction reports correctly keep
`complete_e05_profile: false`.

Historical failed reports, the PDF startup suppression and bounded startup PID
retry are unchanged. The cycle 23 aggregate-count correction remains recorded;
individual test results are not rewritten. Host-timed progress polling may vary
without changing fixed virtual boundaries. No result is promoted outside its
measured profile.

## Closure and next task

SN-017 is done locally for this declared extraction. SN-018 is ready to establish
a reproducibility/performance baseline for the same composition: fixed input
manifest, repeated measurements and explicit latency/error/memory methodology.
It must distinguish variable host timing from simulation time and retain the
same tolerances and capability limits. No performance target or classroom
readiness is inferred from these functional checks.

The full M2 application/runner, arbitrary circuits, E-03 feedback generalization,
physical ADC, new MCU models, project loading, instrumentation, GUI, packaging,
binary redistribution and clean-machine classroom rehearsal remain unvalidated
or pending in their existing tasks. January stabilization and February 2027
teaching targets are unchanged. Remote issue synchronization waits for an
authorized publication cycle; this audit makes no commit or remote update.
