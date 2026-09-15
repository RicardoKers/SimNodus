# Native source connectivity graph

Status: bounded SN-021 contract under [ADR 0062](../decisions/0062-native-source-graph.md).
Load the topology of a complete validated project 0.1 into an immutable native
definition graph. This is source connectivity, not an expanded simulation netlist.

## Representation and acceptance

- Domain types in `src/domain/circuit_graph.hpp` contain owned IDs/names, pins,
  ports, instances, nets and a discriminated local-port/instance-terminal union.
  Closed domain/direction/kind enums follow the accepted topology rules. Domain
  types depend only on standard C++ containers, not JSON, Qt or engine types.
- Keep root selection, every definition including unreachable ones, original
  collection order and explicit connections. Repeated instances reference the
  same definition with distinct scoped IDs. No flattening, generated IDs, global
  ground, label joins or inferred wires are introduced. Unconnected ports/pins
  remain unconnected; names such as GND have no special electrical meaning.
- The application result owns both the graph and complete immutable project
  declaration. All original fields/bytes remain available, including exact
  parameters, overrides, symbols, models, resources, platforms and timing. These
  metadata are not duplicated into the connectivity graph or silently discarded.
  Existing per-occurrence parameter paths remain the instance provenance authority.
- Source mapping covers root selection, every graph entity and every terminal.
  Keys use collection/ID selectors, with explicit endpoint selectors for terminals:
  `circuits/main/nets/drive/terminals/instance/left/input`, represented as separate
  string components. A local port uses `.../terminals/port/source`. These are
  source-definition identities, not flattened occurrence identities or labels.
  Byte spans are half-open offsets in the owned original project input. Object
  spans retain the full declaration, including metadata not copied into the graph.
- The application loader accepts bytes only. It first
  validates the entire project with the existing 1 MiB, nesting, entity, hierarchy,
  expansion and metadata budgets. Its private builder never accepts arbitrary
  caller-built syntax. Validation/allocation failure publishes no partial graph.
  Caller bytes must remain stable during the call; successful results own storage.
  The domain structs are values, not security capabilities; constructing a graph
  manually does not establish project validity or authorize later consumption.
- Compare graph content with an independent view of the preserved Python-validated
  declaration. For every source-map entry, parse its original byte slice and compare
  against the corresponding full source object. Check exhaustive mapping coverage,
  uniqueness, Unicode/escaped labels, reordered arrays/keys, unreachable definitions,
  unconnected ports, the exact 4096-entity boundary and invalid complete projects.
- Verify immutable ownership after input/result release, independence of a copied
  domain graph, scoped repeated-instance IDs and original nested error offsets.
  Regress the full native declarations and existing physical-resource tests.

## Boundaries and next work

`load_project_graph` takes bytes; it does not open files, traverse directories,
download, render, load native code/models/firmware, compile or authorize execution.
Physical Windows containment remains the separate handle-relative verifier with
reparse/alias rejection and bounded immutable byte snapshots. Textual prefixes or
resolve-then-open are not containment proofs. Existing missing-file, substitution,
size/hash and symlink/junction tests retain their original meaning.

All inherited resource/interface/firmware/runtime readiness flags stay false.
Neither graph connectivity nor matching metadata verifies source trust, licensing,
boot/interface compatibility, electrical solvability or real-engine capability.
Synthetic graph tests make no integration claim and expand no accepted profile.

Editing transactions, model/firmware import, portable path-based project acquisition,
safe atomic persistence and source-preserving compilation remain separate work.
The next bounded SN-021 step should address persistence of the owned validated
source with explicit destination ownership, failure preservation and replacement
tests before compilation. A future compiler must retain graph/source identities
and use real engines when claiming supported execution. No schema migration,
format extension, UI, shared instrumentation change or public CLI is selected.
Preserve MCU/toolchain independence, SN-044, local SN-045, numerical bounds,
PDF/PID behavior and SN-017 Python preparation/GDB/fixture scheduling ownership.
Allocation failure is structured but not fault-injected; no hard OS quota is claimed.
