# Structural connectivity compilation

Status: bounded SN-021 stage under [ADR 0067](../decisions/0067-connectivity-compilation.md).

`compile_connectivity(original_bytes)` validates/loads the complete project, expands
reachable instance occurrences and partitions explicitly connected terminals.
The output is immutable logical connectivity with a retained complete source graph.
It is not a SPICE netlist, executable project, solver allocation or readiness token.

## Input and output contract

- Borrowed input must remain stable during the call. Validate the complete project
  0.1, including unused definitions/resources/targets/temporal declarations, before
  expansion. Do not accept a caller-constructed graph as validation proof.
- Start at the selected root and retain tuple paths `(root ID, ordered instance
  IDs)`; never concatenate names. Include the root circuit occurrence and every
  reachable circuit/component occurrence. Repeated definitions remain distinct.
- Create one terminal for every port/pin of every reachable occurrence, including
  disconnected terminals. A child circuit port referenced from its parent and
  declared inside the child is the same occurrence/terminal identity.
- Join only the terminals of each explicit source net. Compute transitive
  equivalence across hierarchical port aliases. Retain singleton groups, every
  constituent source-net occurrence and its source span. Do not join by `GND`,
  display name, direction, parameter, model, root label or assumed global reference.
- Sort occurrences and members by stable tuple identity; sort groups by their
  first terminal. Array reorderings do not change the canonical connectivity
  partition. These are structural identities, not assigned backend node numbers.
- Retain exact source bytes, original hierarchical definitions and all metadata.
  Map each occurrence, terminal and net occurrence to its original source object
  span. Root occurrence maps to the root-selection token. Repeated occurrences
  legitimately share definition spans but have different tuple paths. Original
  net objects retain their explicit endpoint references. No source is flattened
  or overwritten; only the derived result expands.
- Return declaration errors separately from expansion errors. No partial result
  escapes. Error offsets point into the original owned source. Copied shared
  results remain valid after input and result-wrapper release.

## Bounded expansion

Keep all existing input/entity/depth/instance/parameter limits. Add an operation
budget of **65536 records**, counted as root plus descendant occurrences, expanded
ports/pins and source-net occurrences. Charge before inserting each record; fail
with `expansion-budget` at the responsible source span if exceeded. A declaration
can remain valid while this operation rejects its expansion. This is not a schema
or electrical-profile expansion, nor a promised supported simulation size.

Use bounded maps plus union-by-size and path compression. Explicit net membership
is already constrained by declaration validation. Hard OS memory/time quotas,
cancellation and allocation-failure injection are not implemented. Source metadata
and the owned definition graph remain in memory beside the bounded derived data.

## Readiness and next stage

Compilation here means structural lowering only. Effective numerical parameters,
model source interfaces, source bytes/containment, units/ranges at backend input,
reference/ground choice, stimulus/analysis selection, firmware/platform interfaces,
temporal negotiation, trust/redistribution and execution remain separate gates.
No circuit ground or model interface can be inferred from the current schema or
fixture labels. The fixture model text is not an accepted new electrical profile.

The operation opens no file/resource, loads no project-selected DLL/model/firmware,
renders nothing, downloads nothing and invokes no engine. Pure structural tests
do not establish real-engine integration. Next bind the smallest explicitly
supported model/interface and parameter lowering contract to verified captured
resources, with reference/stimulus/analysis authority outside untrusted labels,
then collect real-engine evidence before claiming executable compilation.
Preserve the existing backend profiles and SN-017 Python preparation/GDB/fixture
boundary. Safe overwrite remains pending under ADR 0064; no UI work is selected.
