# Native project declaration validation

Status: bounded SN-021 contract under [ADR 0061](../decisions/0061-native-project-declarations.md).
Implement the preserved [project 0.1](PROJECT_SCHEMA.md) rules over one immutable
native syntax capture. This is declaration composition, not source graph loading.

## Acceptance

- Validate the entire embedded resource-links 0.1 document using the internal
  captured subtree entry point. Preserve all topology, exact parameter, binding,
  inventory and resource-link budgets and strict standalone version gates.
- Retain project identity, independent platform/firmware catalogs, optional board
  identity, exported pins and locked resource references. Validate unused records.
  Display names count Unicode scalars, with the existing 80-character/control rules.
- Targets identify distinct concrete component occurrences. Require matching
  declared architectures, ELF declaration format, null electrical model ownership,
  and complete bijective logical-to-platform pin maps. These are metadata checks.
- Enforce the existing combined 256-entry platform/board/pin/firmware/target/map
  budget and target/path bounds. Preserve explicit empty catalogs and null states.
- Retain unconfigured, known-schedule-replay and approximate-sampled policies,
  explicit fidelity, schedule/null rules and fail-closed debugging/capabilities.
  Parse virtual times as exact positive uint64 integers on the 1000 ns grid;
  reject booleans, floats, overflow and quantum greater than duration. Preserve
  2000 ms pause deadline, 10 microvolt and 1 picosecond tolerances exactly.
- Publish only a complete immutable declaration with shared original bytes and
  absolute source positions, nested parameter occurrences and inventory requests.
  Firmware, runtime, source-interface and all inherited readiness flags stay false.
- Compare the unchanged reference suite and full statistics, with adversarial
  combined-budget, uint64/grid/token, catalog-scope, unused-record, name, path and
  capability tests. Check ownership after caller mutation/result release and
  every occurrence value/origin/offset against standalone resource-link validation.

## Boundaries

The API accepts bytes only. It does not open project/resource paths, follow links,
render SVG, parse SPICE/ELF, load DLLs/firmware, download, compile or start engines.
The existing physical Windows/NTFS verifier remains a separate explicit operation:
handle-relative containment, alias/reparse rejection, bounded same-handle hashes
and immutable snapshots. Textual prefixes and resolve-then-open are not evidence
of containment. Missing files, replacement races and byte mismatches remain covered
by that verifier; declaration success does not bypass those checks.

Matching architecture strings, locked hashes or declared source tokens do not
verify actual ELF/boot interfaces, MCU support, source origin, redistribution or
execution permission. Generic accepted temporal declarations do not expand the
real-engine profiles. No engine integration claim is made by synthetic metadata.

Native source graph loading, safe persistence and source-preserving compilation
remain pending. Keep domain, presentation, instrumentation and adapters separate;
retain SN-044, local SN-045, MCU/toolchain independence and SN-017 Python/GDB
ownership. No UI or public CLI is selected. Native Unicode remains stricter than
historical Python; allocation failures are structured but not fault-injected.
Input/output budgets do not claim hard OS memory or wall-clock quotas.
