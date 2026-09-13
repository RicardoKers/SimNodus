# Experimental parameter schema 0.2

SN-020 second slice, 2026-09-13. [ADR 0046](../decisions/0046-exact-parameter-draft.md)
extends the [0.1 topology contract](TOPOLOGY_DRAFT.md) with quantities and scoped
instance overrides. The original validator, fixtures and contract remain intact.
This is an experimental document/inspection contract, not electrical model
selection, project loading or simulation support.

## Version and fields

Keep `format: "simnodus-topology"`; require `version: "0.2"`. Every component
and circuit definition adds the required `parameters` array. Every instance adds
the required `overrides` object. Empty arrays/objects are allowed. All other 0.1
fields, identity scopes, graph constraints and rejection policies remain.
The new validator projects only these known additions away to reuse unchanged
0.1 topology validation. It never strips unknown user fields or migrates files.
The 0.1 and 0.2 validators each reject the other's version.

A parameter declaration has exactly `id`, `name`, `unit`, `default`, `minimum`
and `maximum`. Parameter IDs are unique in a separate parameter namespace of
their definition. IDs/names use the existing lexical rules. Pins/ports and
parameters can share spelling without sharing identity or automatically binding.
The three numeric strings are in the declaration's base unit; bounds are finite
and inclusive, and `minimum <= default <= maximum` is required. Negative and zero
values depend on the explicit bounds, not on an inferred component model.

## Exact values and closed unit vocabulary

Quantities use decimal **strings**, never JSON numbers or binary floating point.
Accepted grammar is `-?(0|[1-9][0-9]*)(\.[0-9]+)?(e[+-]?(0|[1-9][0-9]*))?`.
Limit each string to 64 characters, 32 mantissa digits including zeros, and an
explicit exponent from -24 through +24. No leading plus, leading zero integer,
whitespace, comma, uppercase E, NaN, infinity, expressions or unit suffix in a
value. Negative zero is allowed; source spelling is preserved on round trip.

| Dimension/base unit | Accepted quantity units | Exact factor to base |
|---|---|---|
| Resistance / `ohm` | `ohm`, `kohm` | 1, 1000 |
| Capacitance / `F` | `F`, `uF`, `nF` | 1, 1e-6, 1e-9 |
| Voltage / `V` | `V`, `mV` | 1, 1e-3 |
| Time / `s` | `s`, `ms` | 1, 1e-3 |
| Dimensionless / `1` | `1` | 1 |

Declarations use base units only. Literal overrides can use any listed spelling
of the same dimension. Units are case-sensitive tokens, not a generic SI/SPICE
parser; `m`, `kOhm`, Unicode micro signs and guessed aliases are rejected. This
vocabulary does not approve corresponding components or models. Convert by exact
decimal powers of ten before checking bounds; no clipping, rounding or tolerance
is applied. Reference code uses a local decimal precision of 100 for scaling,
which exceeds the bounded input precision, independently of ambient precision.

## Overrides and hierarchy

`overrides` maps a target definition's parameter ID to exactly one of:

- `{ "value": "2.2", "unit": "kohm" }`: a literal quantity, dimension-compatible
  and within the target declaration's inclusive range after exact conversion.
- `{ "parameter": "resistance" }`: direct forwarding of a parameter declared
  by the **containing circuit**. No sibling, child, global or dotted-path lookup.

Unmentioned target parameters use their own defaults. Root circuit parameters
use their defaults; root-level runtime overrides are not part of this draft.
Forwarding requires matching dimensions and the entire declared source interval
to fit inside the target interval. This conservative rule checks even values not
used by current instances. A valid default alone cannot justify a binding whose
legal future override could exceed a descendant's range. No narrowing cast,
expression evaluator, arithmetic binding or implicit same-name propagation exists.

Resolve from root toward children. Each occurrence gets fresh effective values;
forwarded values come from that containing occurrence, after its own overrides.
Definitions are never mutated. Topology recursion/depth checks still apply to
all definitions, reachable or not. All declarations/bindings are checked even
when unused by the selected root.

The [0.2 fixture](../../tests/schema/fixtures/two-rc-parameters.json) resolves
left resistor/capacitor to 1000 ohm and 1e-6 F, and right to 2200 ohm and
220e-9 F, through explicit RC parameter forwarding. The instances retain
`main/left/r` versus `main/right/r` paths. This is an inspection snapshot of
document values, not a SPICE netlist or tested electrical state.

## Validation, budgets and output

Keep 1 MiB input, 32 JSON container levels, eight circuit-definition levels and
the 0.1 expanded-instance limit. Parameter declarations and override entries
also consume the shared 4096 declared-entity budget. Before resolution, require
at most 16384 total expanded instance rows **plus their parameter values**, for
every definition. This additional bound prevents a compact shared definition's
parameter list from multiplying into an excessive output snapshot.

New error codes are `quantity` (invalid decimal syntax/budget), `unit` (unknown
unit or incompatible dimension), and `range` (invalid declaration, out-of-bounds
literal or unsafe forwarding interval). Existing `shape`, `reference`, `id`,
`budget`, `hierarchy`, `version` and `input` codes retain their meaning.

The [developer CLI](../../tests/schema/parameters.py) returns exit 0 with
`valid-parameters-only` plus an instance-path snapshot, or exit 1 with a
structured validation error. Snapshot values use exact base-unit decimal strings
and origins `default`, `literal`, or `containing-circuit:<parameter-id>`; trailing
zeros may remain. The snapshot is not a round-trip source document. Source JSON
round trips preserve original units, strings, overrides, IDs and decoded tree.
Validation/resolution do not save or change the input, access resources, load
models, invoke compilers/engines or evaluate host code.

Symbol/layout separation, model pin/parameter mappings, resources/dependency
locks, MCU/board/firmware metadata, temporal policies and full persistence remain
pending. SN-020 is not complete. All previous engine/fixture capability limits,
PDF suppression, PID retry and ADRs 0027/0028 remain unchanged.
