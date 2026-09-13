# Project declaration baseline 0.1

Status: SN-020 declaration specification, validated by the Python reference tool.
[ADR 0051](../decisions/0051-project-declaration-baseline.md) defines the acceptance
boundary. Native loading/saving, rendering, resource import and simulation are
not implemented by this schema. Application version and schema version differ.

## Composition and evolution

The exact root fields are `format: "simnodus-project"`, `version: "0.1"`, `id`,
`name`, `sources`, `platforms`, `firmware`, `targets`, and `temporal`. `id` and
`name` use the existing stable-ID and display-name rules. `sources` embeds the
[resource-links 0.1](RESOURCE_LINKS_DRAFT.md) document, which embeds the unchanged
topology 0.3 and resource lock 0.1. This single input is limited to 1 MiB; a
future directory representation must preserve the same semantics and reference
identities. No `.snod` extension, archive format or automatic splitting is selected.

The [topology](TOPOLOGY_DRAFT.md), [parameters](PARAMETERS_DRAFT.md),
[descriptors](BINDINGS_DRAFT.md), [lock](RESOURCE_LOCK_DRAFT.md) and
[typed references](RESOURCE_LINKS_DRAFT.md) define their respective subdocuments.
Earlier independent drafts and their evidence remain unchanged historical
contracts. Unknown fields/versions and duplicate JSON keys are errors, never
silently dropped. There is no automatic migration: future migrations require a
new version, preserved original bytes, explicit conversion and round-trip tests.

Editable declarations need not be runnable: null model/resource associations,
no MCU targets and unconfigured temporal policy are valid explicit states. Every
result reports `simulation_ready: false`. Semantic validity is distinct from
electrical solvability, correct assets, firmware compatibility and runtime approval.

## Board, MCU and firmware declarations

`platforms` and `firmware` are arrays with unique IDs in their separate catalogs.
Each platform has `id`, `name`, `mcu`, `board`, `pins`. `mcu` has `device` and
`architecture` stable-ID tokens and a `resource` reference. This resource declares
the external MCU/platform description; its concrete backend format is deliberately
not interpreted by the domain. A platform's exported logical interface is `pins`,
an array of unique `id`, `name` records. These are interface IDs, not inferred
device package numbers, register names or backend types.

`board` is explicit null or an object with `id`, `name`, `resource`. Board identity
is scoped to the platform and remains distinct from MCU device identity. Board
wiring, boot configuration and MCU details belong to the referenced description
and future adapter verification, not generic core fields or GUI code.

Every firmware record has `id`, `name`, `architecture`, `image_format: "elf"`,
and `resource`. Every resource reference here is exactly `dependency`, `resource`,
resolved as an existing pair in the embedded lock. Its use site declares its
purpose; neither a filename nor a license string proves that purpose. Firmware
bytes, ELF header/architecture, boot settings, memory maps and actual device
support must be verified before execution. No compiler/IDE path, build command,
automatic download, native library or execution hook is accepted.

`targets` associate execution intent with concrete component occurrences. Each
has `path` (root circuit ID followed by instance IDs), `platform`, `firmware`,
and `pin_map`. Paths must resolve to a component, not a reusable circuit or label,
and may appear only once. Repeated instances may select the same platform/image
but retain distinct paths. The declared architectures must match exactly.
`pin_map` bijectively covers logical component pins and exported platform pins.
The component must have null electrical model binding, preventing simultaneous
SPICE and MCU ownership. Null models without targets remain explicitly unbound.

No additional MCU or peripheral support is inferred. The tests use synthetic
platform/image declarations in memory to test reference rules, never a fake
backend or firmware-execution acceptance. The saved two-RC project has no MCU
target; the existing real-engine evidence remains the support authority.

## Temporal and fidelity policy

The exact fields of `temporal` are:

| Field | Declaration rule |
|---|---|
| `mode` | `unconfigured`, `known-schedule-replay`, or `approximate-sampled` |
| `fidelity` | null when unconfigured; `causal-replay` for replay; `approximate` for sampled |
| `duration_ns` | null when unconfigured; otherwise positive unsigned-64-bit integer virtual nanoseconds on the 1000 ns grid |
| `exchange_quantum_ns` | Same representation; positive and no larger than duration |
| `schedule` | Locked dependency/resource pair for replay; null otherwise |
| `debug` | `disabled` when unconfigured; otherwise `disabled` or requested `bounded-cooperative` |
| `pause_wall_timeout_ms` | Exactly 2000 integer wall milliseconds |
| `voltage_tolerance_uv` | Exactly 10 integer microvolts |
| `analog_time_tolerance_ps` | Exactly 1 integer picosecond |
| `on_unsupported` | Exactly `reject`; no silent mode or ideal-model fallback |

Booleans/floats/strings are not integer time values. Quantum and duration do not
represent host performance; the 1000 ns grid is a conservative declaration rule
for the existing external-control boundary, not timing precision of an MCU.
Pause deadlines use wall time. The analytical voltage/time tolerances keep the
existing numerical bounds; the picosecond tolerance does not imply picosecond
control or MCU accuracy. No real-time requirement follows from these fields.

These are requested policies, not negotiated capabilities. `runtime_profile_verified`
always remains false. A future session must validate exact engines, schedule
contents, pacing, electrical modes, firmware and debugging capabilities against
the existing accepted profiles. Unsupported combinations must fail before
execution. A well-formed generic replay/approximate request does not make the
arbitrary project executable. Live causal feedback, prediction, rollback and
unpaced/general debugging are not selectable. No virtual time advances here.

## Budgets, ownership and deferred runtime gates

All embedded document budgets remain in force. Platform records, board records,
exported pins, firmware records, target records and pin mappings share an
additional maximum of 256 project entries. At most 256 targets and the existing
hierarchy path depth are allowed. Unused declarations are validated too.

The reference CLI opens only the explicitly selected JSON input. No resource
traversal, ELF loading, graphical preview, model import or backend startup occurs.
Outputs retain the lock/link verification flags and add `firmware_verified` and
`runtime_profile_verified`, always false. SN-021 must perform physical containment,
bounded same-handle byte verification, importer/boot/interface validation,
atomic saving and source-preserving compilation. Those are not schema tests.

The source graph, asset metadata and editable properties remain independent of
presentation layout, session captures and runtime state. ADR 0049 remains the
authority for independent windows and shared persistent measurements; geometry,
probe persistence and concrete measurement types are not added to this baseline.
Future versions must preserve stable entity/instance IDs and explicit measurement
reference/orientation. No SN-022/023/024 work is started by this contract.
