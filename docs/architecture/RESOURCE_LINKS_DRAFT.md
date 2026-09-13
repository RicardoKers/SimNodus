# Descriptor/resource references 0.1

Status: bounded SN-020 reference contract, under
[ADR 0050](../decisions/0050-typed-descriptor-resources.md). Full project/component
schema acceptance remains pending. This document does not approve rendering,
electrical execution, resource loading or new backend capabilities.

## Document and explicit association

The root has exactly `format: "simnodus-resource-links"`, `version: "0.1"`,
`topology`, `lock`, `assets`, `symbols`, and `models`. Topology is an embedded,
unchanged [0.3 document](BINDINGS_DRAFT.md). Lock is an embedded, unchanged
[resource lock 0.1](RESOURCE_LOCK_DRAFT.md). Embedding creates one bounded
validation input without following file references. No migration or rewrite of
the earlier documents occurs. Paths retain the explicit root required by the lock.

Every asset has exactly `id`, `kind`, `dependency`, and `resource`. IDs follow
the existing stable-ID grammar. Asset IDs are unique; the dependency/resource
pair must exist in the lock. A resource can have only one asset declaration,
preventing conflicting or aliased roles. Many descriptors may refer to that same
asset ID. Unused assets are also checked; untyped inventory entries such as
license notices remain allowed. Roles are closed to `symbol-svg` and `model-spice`.
These describe intended content, not validated parsers or backend adapters.
Neither path suffix nor display name chooses a role.

`symbols` is a map covering every symbol descriptor ID exactly once. Each value
is an asset ID of kind `symbol-svg`, or explicit null. `models` likewise covers
every model descriptor ID. Null means absent with no fallback. Non-null model
bindings have exactly `asset`, `entrypoint`, `terminal_map`, and `parameter_map`.
The asset must have kind `model-spice`. Entrypoints and mapped source tokens use
`[A-Za-z_][A-Za-z0-9_]{0,63}`; arbitrary source fragments are not accepted.
Each map covers all corresponding interface IDs. Values are unique ignoring
case within each map. Terminal and parameter namespaces are separate. For example:

```json
{
  "asset": "passive-model",
  "entrypoint": "declared_resistor",
  "terminal_map": {"first": "first", "second": "second"},
  "parameter_map": {"value": "value"}
}
```

This records intended source names without parsing the resource or asserting
that the entrypoint exists. A future SPICE importer must verify the subcircuit,
formal ports/order, parameters, units/ranges and supported dialect against the
descriptor before compilation. No source-name inference or positional guessing
is authorized. SVG anchors/geometry also need a later validated representation
and importer. The SVG fixture deliberately establishes no renderer/anchor ABI.

## Invariants and limits

Changing a visual association leaves topology, logical pin maps, electrical
bindings and effective parameters intact. Resources have their own stable IDs;
renaming an asset requires explicit reference updates. Nothing rebinds by label.
UI preview can inspect declarations without executing models, consistent with
[ADR 0049](../decisions/0049-desktop-ux-and-measurements.md). No changes to the
SN-044 window, measurement or instrumentation design are made here.

The whole encoded document is bounded to 1 MiB and the existing nesting limit.
At most 256 assets are allowed. Asset records, descriptor binding slots, non-null
entrypoints and model map entries count against the existing 4096 topology/
parameter/descriptor budget. Lock file/dependency/byte budgets remain separate
and unchanged. All unknown fields, malformed types, unresolved references,
wrong roles, duplicate keys and unsupported versions are rejected with structured
errors. Whole-document JSON round trips preserve all fields.

`resource_links.py` is a pure reference validator. Its CLI reads only the
explicitly supplied metadata file. Results always report `simulation_ready`,
`resources_verified`, `containment_verified`, `redistribution_verified`, and
`resource_interfaces_verified` false. A valid role, hash or entrypoint declaration
is not evidence of safe contents or verified interfaces. No filesystem traversal,
download, SVG rendering, model execution, firmware loading or engine runs occur.

## Evidence and remaining acceptance

The [fixture](../../tests/schema/fixtures/two-rc-resource-links.json) embeds the
preserved two-RC topology and references owned SVG/SPICE fixture text plus LICENSE.
Original asset text is MIT project material; no external asset was incorporated.
The tiny model source is input for reference checks, not an accepted new electrical
profile. File size/hash checks in tests read these trusted repository files
explicitly and do not grant the metadata parser resource access.

See the [report](../experiments/SN-020-resource-links.md) for executed checks.
SN-020 still needs project-level identity/composition, board versus MCU and
firmware contracts, and temporal/fidelity configuration before overall acceptance.
Do not confuse syntactic links with physical loading (SN-021) or advance UI work.
Probe definitions remain a later instrumentation contract under ADR 0049.
