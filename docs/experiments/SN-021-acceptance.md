# SN-021 acceptance matrix and remaining gates

Date: 2026-09-22. Status: **in_progress**, not full acceptance. This audit follows
PR #43 (`04f0174`) and supersedes earlier slices' next-step descriptions without
rewriting their evidence. The backlog criterion is portable paths, atomic save,
source mapping and an independent graph. ADR 0051 also requires actual resource,
interface, boot and runtime correspondence before executable acceptance.

## Subsequent configured lifecycle acceptance

The [configured lifecycle](SN-021-replay-lifecycle.md) now composes native save/
reopen and fixed replay with real ngspice consumption, policy/source preservation
and negative collision/resource controls. The initial audit below is historical.
Safe overwrite under ADR 0064 remains the implementation gate before final bounded
acceptance. Unsupported runtime modes remain explicit limits, not automatic scope
expansion. Preserve the original audit and all prior engine failures.

## Subsequent measured binding

The [fixed E-01 replay](SN-021-fixed-replay.md), accepted under ADR 0075 after this
initial audit, now supplies positive configured-project correspondence for one
analog-only 5 ms interval. Original policy/schedule provenance is retained, existing
standalone rejection is unchanged, and real ngspice passed the project bounds.
The table and remaining-work list below describe the initial PR #44 audit. Its
configured-runtime gap is narrowed by that measured binding; next compose it with
configured acquisition/edit/save-copy/reopen. Safe overwrite remains pending.
Other temporal/mixed-signal/debug modes are unsupported, not implicitly required
profile expansions. Keep the original audit evidence and all negative results.

## Implemented and measured

| Concern | Implementation and evidence | Limit |
|---|---|---|
| Declaration and independent graph | [Native validation](SN-021-project.md), [source graph](SN-021-graph.md), [connectivity compilation](SN-021-connectivity-compilation.md) | Owned graph, stable occurrence/source identities; not electrical solvability |
| Paths and resource bytes | [Native snapshots](SN-021-native-resources.md) | Bounded Windows/NTFS handle-relative capture, alias/reparse refusal, missing/size/hash failures; no pathname lease or general filesystem support |
| Acquisition and editing | [Project lifecycle](SN-021-project-lifecycle.md) | Capture, validate, name-edit, save-copy and reopen; not a general editor or migration system |
| Atomic persistence | [Create-only save](SN-021-save-create.md) and lifecycle collisions | Existing destinations preserved; overwrite unsupported under ADR 0064 |
| Passive interface and backend artifact | [Ideal RC compilation](SN-021-ideal-rc-compilation.md) | Explicit E-01 request, recognized R/C wrappers, exact values/wiring/source mapping and real ngspice; unconfigured project only |
| Firmware and platform | [Reference target](SN-021-reference-target.md), ADR 0074 | Owned SN-012 identity, inert ELF/static boot checks and explicit real boot through retained staging; one isolated unconfigured reference |
| Temporal/runtime correspondence | Declaration validation and rejection regression below | No configured project-to-session composition or readiness approval |

Declaration validity, physical containment, byte identity, interface compatibility,
static boot checks and execution permission remain separate. Origin and redistribution
permission follow from none of these results. Opening a project never downloads,
loads DLLs, runs firmware or renders unverified content.

## Why isolated engine results do not close the runtime gate

E-01 and the isolated SN-012 workflow reject configured temporal policy. Combining
them is not a coupled project compiler. E-03's P0/P1 evidence uses a 16.5 mV voltage
error gate and a 2 us crossing error gate; project 0.1 declares 10 microvolt and 1 ps
bounds. These concern distinct measured quantities and cannot be substituted or
relaxed to make a project appear accepted. Sampled late feedback remains approximate
with smaller quanta. E-03's recorded schedule is experiment output, not an accepted
project schedule-import format.

[SN-017](SN-017-acceptance.md) accepts a different bounded paced fixture composition.
Python preparation, GDB transport and fixture sequencing remain host responsibilities.
A project's requested duration is not automatically a native session segment duration;
its quantum, debug choice or matching architecture does not select a tested session.
Require explicit correspondence to exact owned firmware, platform, circuit, pacing,
electrical/ADC behavior and tolerances. Do not replace requested policy silently.

## Rejection regression and preservation

Two existing native-operation test suites now submit 48 adversarial requests:
for each of ideal RC compilation and reference-target inspection, both configured
modes, both debug choices, quanta 20/100/1000 us and two unusable roots (missing
absolute directory and invalid relative path). Duration is 10 ms. Each request
first passes the unchanged Python declaration validator with readiness false,
then must fail at the native standalone-policy gate before physical root validation.
A valid manifest must not authorize a fallback that ignores its temporal policy.

The replay reference selects an existing locked resource only to establish metadata
validity; no accepted schedule content is claimed. Neither operation consumes those
resources. Both targeted CTests passed. These are synthetic rejection checks, not
new integration evidence. Native code and engine inputs are unchanged, so prior
real-engine results are reused without claiming a new engine run.

The [audit](evidence/SN-021-acceptance-gates-summary.json) records 103 unchanged
historical evidence files, 14 schema fixtures, 24 prior runtime/source hashes,
two changed test hashes and twelve preserved local files. The first audit stopped
on the superseded lifecycle CMake hash; the corrected audit uses the latest recorded
hash per path and retains that correction. No historical hash was rewritten.

```sh
ctest --test-dir build/sn021-save -C Debug -R 'native-ideal-rc-compilation|native-reference-target' --output-on-failure
python tools/check_repository.py
git diff --check
```

## Remaining acceptance, in order

1. **Configured project correspondence.** Predeclare one existing measured fixture
   and its exact requested-policy mapping. Bind retained graph/resource/firmware
   identities and explicit analysis authority to an inert prepared plan. Preserve
   original policy fields, numerical bounds and source positions. Reject mismatched
   duration/quantum/debug/pacing, wiring, images and unsupported schedules before
   execution. Consume the plan in the existing explicit real-engine harness with
   unchanged assertions and Python/GDB ownership. Require positive real consumption
   and adversarial mismatch evidence, not another metadata-only success. Define a
   schedule parser only if the selected accepted fixture needs it; do not invent
   a general schedule format now.
2. **Safe overwrite.** Satisfy ADR 0064's expected identity/version and atomic
   publication gate under a stated concurrency model, with physical competing-writer
   and ordinary-failure evidence. Retain create-only saving meanwhile. Do not repeat
   check-then-replace or advisory-lock-only protocols, or use textual prefixes and
   resolve-then-open as proof. Document any new ownership/concurrency contract before
   implementation rather than weakening the selected-directory contract implicitly.
3. **Final bounded acceptance audit.** Compose the accepted lifecycle, compilation
   and requested runtime; record source mapping, failure behavior, limitations and
   actual engine evidence. Only then consider SN-021 complete and prepare the next
   cycle prompt. UI, new MCUs, arbitrary models, native-only orchestration, releases
   and other SN work are not implied completion requirements.

This audit changes no architecture decision or runtime API. ADRs 0027/0028/0043,
0051/0064/0071/0074, SN-044, instrumentation, PDF suppression and PID retry remain
in force. Twelve local SN-045 files remain outside publication.
