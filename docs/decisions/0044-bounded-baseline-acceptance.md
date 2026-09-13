# ADR 0044: Accept the bounded local reproducibility/performance baseline

Date: 2026-09-12. Status: accepted for the measured Windows Debug fixture only.

## Decision

Close SN-018 for the initial baseline of the ADR 0014/0043 headless composition.
The [acceptance audit](../experiments/SN-018-acceptance.md) maps its fixed-reference,
latency, error and memory criterion to the measured stepping, pause/resume and
recreated-session scenarios. These scenarios deliberately share one circuit and
firmware; acceptance does not imply multiple supported circuits or lesson projects.

Use the corrected identity sampler campaign for memory attribution. Thirty-six
real sessions and their raw metrics support the baseline; the prior contaminated
campaign remains inconclusive. A descriptive baseline does not require proving
zero observer overhead, a speed target or real-time execution. Mixed paired
differences remain unresolved as causal slowdown. Memory is a sampled lower
bound, not an exact peak or leak test.

## Evidence and alternatives

The [audit JSON](../experiments/evidence/SN-018-acceptance-summary.json) revalidates
3793 file hashes, 36 raw-session metric records, 324 fixed checkpoints, 360 analog
checks and 14122 ancestry rows. No new engine run is claimed for this audit.

Adding another circuit would expand the user-directed measurement profile.
Repeating unchanged batches would not establish clean-machine readiness, Release
behavior, scaling or product budgets. Those require their own scoped evidence
when needed. Accept the bounded reference scenarios now, while retaining those
limitations, rather than claiming broad completion or leaving an unbounded task.

## Consequences and revisit criteria

Mark SN-018 done locally and next select SN-020's schema/validation specification.
Do not mark the production simulator, shared instrumentation, portable setup,
classroom lessons or deployment ready. Python/C# preparation, GDB and fixture
orchestration remain explicit. ADRs 0014/0027/0028/0043, tolerances, pacing, PDF
suppression and startup PID retry are unchanged. No commit or remote change.

Revisit this baseline for changed engine/firmware/native inputs, sampler semantics,
compiler configuration, host comparison or a new supported workload. Define
product performance targets and classroom hardware using appropriate new
measurements before release; this decision supplies no universal performance
guarantee and no support expansion.
