# SN-020 first slice: executable topology draft

Date: 2026-09-12. Status: first slice passed; **SN-020 remains in progress**.
[ADR 0045](../decisions/0045-experimental-topology-draft.md) introduces the
[topology-only 0.1 draft](../architecture/TOPOLOGY_DRAFT.md). Full component/project
schema and native loading/saving/compilation are not implemented by this cycle.

## Implemented and tested

The reference validator accepts explicit component pins, circuit definitions,
instances, ports and nets. IDs are separate from names; repeated subcircuit
instances retain distinct identity paths. The draft rejects unknown fields,
unsupported versions, duplicate IDs/keys, dangling/wrong-kind references,
multiply connected terminals and recursive/excessively deep or expanding graphs.
Validation is bounded to 1 MiB input, 32 JSON container levels, 4096 declared
entities, eight circuit levels and 16384 expanded instances per definition.
These are validation budgets, not supported simulation sizes.

Fifteen tests passed, including valid/invalid JSON round-trip fixtures, unchanged
decoded trees, two distinct RC instance paths, order independence, names that do
not join nets, malformed input and exponential acyclic expansion. The CLI valid
fixture passed with 26 declared entities, depth 2 and six expanded instances.
The two invalid fixtures returned exit 1 with `hierarchy` and `reference` as
expected. Commands live in the [fixture guide](../../tests/schema/README.md).
Source/fixture hashes and observed results are in
[compact evidence](evidence/SN-020-topology-summary.json).
Final documentation verification on 2026-09-13: the repository checker passed
(418 text files), and `git diff --check` passed. No simulator tests were run.

All new content is owned project text/code. No third-party assets or schema
dependencies were introduced. The developer tool reads one explicitly supplied
file; it performs no dependency discovery, download, code loading, compilation
or engine invocation. Directory preparation initially hit a local write denial;
the authorized fixture directory was then created and all checks ran normally.

## Deliberate exclusions and next step

The RC fixture is structural: it has no resistance/capacitance values, solver,
firmware or new engine-support claim. Component pin declarations are not full
packages. There is no implicit ground, global net or electrical-solvability
approval. Layout, symbols, models, parameters/units, resources, dependencies,
MCU/board metadata and temporal policies are not yet schema fields. Unknown
fields are rejected rather than stripped; no migration is implemented.

Next take the bounded **unit-bearing parameter declarations and per-instance
overrides** slice, specifying numeric representation, dimensional compatibility,
ranges and invalid/round-trip cases without loading models or external resources.
Keep symbols, electrical behavior and board representation separate. Complete
SN-020 acceptance must still address the remaining project/component contracts;
SN-021 owns actual persistence and compilation and remains planned.

SN-017 and SN-018 acceptance limits, Python GDB/fixture ownership, PDF suppression,
PID retry and ADRs 0027/0028 are unchanged. No engine/CubeIDE execution or simulator
readiness is claimed. No commit, issue synchronization or publication occurred.
