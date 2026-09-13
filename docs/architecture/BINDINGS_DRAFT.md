# Experimental symbol/model descriptor schema 0.3

SN-020 third slice, 2026-09-13. [ADR 0047](../decisions/0047-declarative-bindings-draft.md)
adds separate descriptors and explicit bindings to the
[0.2 contract](PARAMETERS_DRAFT.md). This validates declared references, not
graphics, model implementations or verified backend support.

## Fields and identity

Require `format: "simnodus-topology"`, `version: "0.3"`, all 0.2 top-level fields
and new required arrays `symbols` and `models`. Components add required `symbol`
and `model` bindings; circuits add required `symbol` only. Bindings may be `null`,
meaning explicitly unbound. Instances retain 0.2 fields/overrides; there is no
per-instance symbol/model selection. Circuit behavior remains its hierarchy.

| Descriptor | Exact fields | Meaning |
|---|---|---|
| Symbol | `id`, `name`, `pins` | Visual interface without geometry or resource paths |
| Symbol pin | `id`, `name` | Visual anchor, distinct from logical pin identity |
| Model | `id`, `name`, `terminals`, `parameters` | Model interface without implementation/backend selection |
| Model terminal | `id`, `name`, `domain` | Only `electrical` in this draft |
| Model parameter slot | `id`, `name`, `unit`, `minimum`, `maximum` | 0.2 base unit, bounded decimal strings and inclusive interval |

Symbol IDs and model IDs each have their own document-wide namespace. Anchor IDs
are unique within a symbol; terminal and parameter IDs each have separate scopes
within a model. Existing lexical ID/name rules apply. Spelling shared across
catalogs never establishes a binding. Empty catalogs/interfaces are allowed,
subject to binding cardinality. Validate unused descriptors too. Model slots
require `minimum <= maximum`; component declarations own defaults.

## Binding rules

A symbol binding is exactly `{"definition":"two-pin","pin_map":{"p":"a","n":"b"}}`.
Keys cover all component logical pins, or all circuit ports, once; values cover
every anchor of the selected symbol exactly once. Missing pins, extra anchors,
aliases and unresolved IDs are errors. Hidden pins, multi-unit symbols and
partial/many-to-one mappings require a later contract.

A component model binding is exactly:

```json
{"definition":"resistor-interface","pin_map":{"p":"first","n":"second"},"parameter_map":{"resistance":"value"}}
```

Pin maps are bijections from logical pins to model terminals, with matching
domains. Parameter maps are bijections from all component parameter declarations
to all model slots. Dimensions match, and the component's full declared range
must fit inside the target slot interval. No implicit order, omitted parameter,
conversion expression or inferred default is accepted. Circuits cannot have a
`model` field. Parameters/forwarding still follow 0.2.

Changing symbols must preserve logical connectivity, model maps and effective
parameter values. The fixture exercises alternate anchors/order without changing
those structures. Null bindings mean absence, never an ideal-model fallback.
Unresolved IDs fail instead of becoming null. A non-null model is only a declared
interface; every successful result reports `simulation_ready: false`.

## Validation and limits

The [reference validator](../../tests/schema/bindings.py) removes only these known
additions from an in-memory copy to reuse 0.2 validation. Unknown fields are not
discarded; no migration occurs. Preserve all original 0.1/0.2 artifacts.

Descriptor records, anchors, terminals, slots, non-null bindings and mapping
entries consume the existing shared 4096 declared-entity budget. Previous input,
nesting, hierarchy and resolved-value limits remain. New error `mapping` covers
incomplete/aliased terminal or parameter maps; other existing error codes retain
their meaning. CLI exit 0 reports `valid-descriptors-only`; exit 1 reports a
structured validation error. Input documents are never rewritten.

Round-trip fixtures preserve catalogs, maps, identities and parameters. The
[owned fixture](../../tests/schema/fixtures/two-rc-bindings.json) includes three
symbol interfaces and two passive-model interface declarations, with no external
artwork, vendor models or executable implementation. There are no `path`, `url`,
`dll`, `script`, `backend` or resource/license fields in this version. Such fields
are rejected, not opened. Descriptor truth, rendering and solvability remain
unvalidated. A descriptor is not a package/plugin ABI or a redistribution approval.

Next specify locked resource/dependency metadata and path containment, including
origin/license records and no automatic download/execution. Board/MCU/firmware
and temporal configuration remain pending. SN-020 stays in progress and SN-021
loading/saving/compilation stays planned. ADRs 0027/0028 and prior engine limits
are unchanged.
