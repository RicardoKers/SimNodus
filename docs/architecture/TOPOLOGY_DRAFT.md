# Experimental topology schema 0.1

SN-020 first slice, 2026-09-12. This is a bounded, executable **topology draft**,
not the complete project/component format or a runnable simulation input.
[ADR 0045](../decisions/0045-experimental-topology-draft.md) scopes the decision.
The normative field/rule tables below and the reference validator must agree;
changing either requires updating fixtures and the recorded schema version.

## Document and identity

Use a UTF-8 JSON object with exactly `format`, `version`, `root`, `components`
and `circuits`. `format` is `simnodus-topology`; `version` is the string `0.1`.
Other versions, unknown fields, duplicate JSON keys and non-finite numbers are
errors. No migration, extension passthrough or silent field dropping occurs.
The draft defines no filename extension or application-version coupling.

IDs match `[a-z][a-z0-9_-]{0,63}` and are case-sensitive. Component and circuit
definition IDs share a bundle-wide namespace. Inside a component, pin IDs are
unique. Inside a circuit, instance, port and net IDs share a namespace. Display
names are separate nonempty strings, at most 80 characters, with no control
characters. Names do not join nets, select models or establish identity.

Preserve IDs through save/reopen and name changes. The validator checks one
document's uniqueness/references; it cannot infer whether an editor improperly
changed an ID between revisions. Future editing/migration code must enforce that
cross-revision contract. No IDs are generated or repaired while validating.

| Object | Exact fields | Meaning |
|---|---|---|
| Component | `id`, `name`, `pins` | Topological pin declaration only; no model capability claim |
| Pin | `id`, `name`, `domain` | `domain` is `electrical` in this draft |
| Circuit | `id`, `name`, `ports`, `instances`, `nets` | Reusable definition; `root` selects one definition |
| Port | `id`, `name`, `domain`, `direction` | Electrical domain; direction is `input`, `output` or `bidirectional`, descriptive only |
| Instance | `id`, `name`, `kind`, `definition` | Kind is `component` or `circuit`; target must exist with that kind |
| Net | `id`, `name`, `terminals` | At least two explicit terminals; no implicit global/ground connection |

All collections are JSON arrays; `components` can be empty and `circuits` must
contain the selected root. Components require at least one pin. Empty circuit
collections are valid, including an empty root circuit.

A terminal is exactly one of `{ "port": "port-id" }` (a port of this circuit)
or `{ "instance": "instance-id", "terminal": "pin-or-port-id" }` (a pin of a
component instance or port of a subcircuit instance). No bare display-name joins
or hierarchical path strings are accepted. Each terminal can appear at most once
among all nets of its containing circuit. Unconnected pins/ports are allowed;
electrical floating-node or solver readiness checks belong to later work.

## Hierarchy and source identity

References preserve the definition graph, with no flattened netlist as source.
Reject missing references, wrong target kind, direct/indirect recursion and
depth overflow in **every** definition, including unreachable definitions.
Repeated references to a definition are valid. Instance identity after future
expansion is `(root-definition-id, ordered instance-ID path)`; terminals add
their pin/port ID. Never concatenate display names to establish identity.

The [valid fixture](../../tests/schema/fixtures/two-rc.json) contains `left` and
`right` instances of one RC topology definition. Their leaf paths differ even
though they reference the same resistor/capacitor pin declarations. The fixture
does not assign electrical values or instantiate backend state. It demonstrates
independent structural identities, not tested independent simulation state.

There is no special `GND` name, pin, global net or board identity in this slice.
Hierarchical ports bind only through explicit terminals. Units, parameters,
reference-ground semantics, model mappings, symbols/layout, board/MCU profiles,
firmware, dependency locks, resources and temporal policies are **not fields of
0.1**. They require a later SN-020 slice; the architecture's separation and stable
identity rules remain mandatory. No schema field may request execution.

## Bounded validation and round trip

The standard-library [reference validator](../../tests/schema/topology.py) reads
only the explicitly supplied JSON file. It does not discover packages, resolve
paths, load a library, invoke an engine/compiler or access the network. The CLI
is a developer validation tool, not the SN-021 project loader. Future resource
handling still needs containment, symlink/junction and atomic-save checks.

Limits before simulation: 1 MiB input bytes, at most 32 JSON container levels,
4096 declared entities (definitions, pins, ports, instances and nets), hierarchy
depth at most eight circuit definitions including the starting definition, and
at most 16384 expanded instance occurrences per definition. Repeated subcircuit
instances count separately in expansion. Validate unreachable definitions too;
a shallow acyclic graph can still expand exponentially. These are conservative
draft validation budgets, not simulator circuit-size support promises.

Validation returns a structured error code and JSON-style location. Invalid
documents never produce a normalized document. First error only is guaranteed;
diagnostic order is not a public ABI. CLI exit is 0 for valid, 1 for validation
failure and 2 for argument errors. Round-trip checks parse, validate, serialize
and revalidate without changing the decoded tree, IDs, array order or hierarchy.
Whitespace/key ordering is not preserved; no save operation modifies the input.

| Error code | Meaning |
|---|---|
| `input` | Size, encoding, JSON syntax/duplicate keys, non-finite literal or nesting violation |
| `shape` | Wrong type, missing/unknown field or invalid collection/cardinality |
| `version` | Unsupported format or schema version |
| `id` | Invalid or duplicate persistent ID |
| `value` | Invalid display name, domain, kind or direction |
| `reference` | Missing/wrong-kind definition, instance or terminal |
| `connection` | Terminal appears more than once across a circuit's nets |
| `hierarchy` | Recursive definitions, excessive depth or expansion |
| `budget` | Too many declared entities |

The malformed topology fixtures are valid JSON with expected semantic failures;
additional adversarial tests cover duplicate keys, encoding/size/nesting and
graph growth. Passing this validator says only that the draft topology satisfies
its structural contract. Native loading, compilation, electrical validation and
simulation of these files remain unimplemented.
