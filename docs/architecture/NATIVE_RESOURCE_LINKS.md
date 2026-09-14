# Native typed resource links

Status: bounded SN-021 contract under [ADR 0060](../decisions/0060-native-resource-links.md).
Implement preserved [resource-links 0.1](RESOURCE_LINKS_DRAFT.md) over a single
owned syntax capture. Internal validators accept trusted token subtree roots;
public topology/parameter/binding/lock APIs keep strict standalone version gates.
No projection, reserialization, field dropping or repeated ingress is used.

## Acceptance

- Fully validate embedded topology 0.3 and lock 0.1 before links. Keep all earlier
  quantities, paths, hashes, hierarchy, source IDs and input/output budgets.
- Each asset has an explicit existing dependency/resource pair and one closed
  role: symbol-svg or model-spice. Reject duplicate IDs, role aliases, wrong kinds,
  unknown targets and invalid unused assets. Count at most 256 assets.
- Symbol/model maps cover their entire descriptor catalog, including explicit
  nulls. Non-null model bindings require an asset, entrypoint and complete terminal/
  parameter maps. Source tokens use the preserved ASCII grammar and are unique
  case-insensitively within each map; the two namespaces remain independent.
- Retain the combined 4096 topology/parameter/descriptor/resource-link budget.
  Count assets, all descriptor slots including nulls, entrypoints and source maps.
- Publish an immutable composition with topology parameter occurrences, lock
  requests, link statistics and false interface-verification flag. Both nested
  results own the same original capture, including roles, maps and nulls. No
  source file is opened or interpreted, and no graph/runtime state is published.
- Compare the unchanged Python suite and full statistics; add exact combined/asset
  limits, token boundaries, separate namespaces, all-null states and unknown fields
  inside nested documents. Symbol rebinding preserves model/topology declarations.
- Verify shared capture ownership after caller mutation/result release, nested
  error offsets and each occurrence's values/origins/offsets against the standalone
  binding result. Regress prior validators and physical-resource tests.

## Remaining boundaries

Declared entrypoints/maps do not verify actual source interfaces, renderer safety,
origin, redistribution or execution permission. Physical containment and bytes
remain a separate explicit Windows/NTFS operation with handle-based traversal,
alias/reparse rejection and immutable snapshots. No textual prefix or later path
reopen is introduced as proof; symlinks, junctions, missing files, replacement,
limits and size/hash mismatches retain the existing physical tests and policies.

No downloads, DLL/model/firmware loading, preview rendering, compilation or UI
follow from this API. Project semantics, native graph loading, atomic saving and
source-preserving compilation remain pending. Keep SN-044, local SN-045, shared
instrumentation, MCU/toolchain independence, numerical profiles, PDF/PID behavior
and SN-017 Python/GDB ownership. Native Unicode remains stricter than historical
Python. Allocation failures are structured but not fault-injected; no hard OS
memory or wall quota is claimed. The probe is a developer test, not a public CLI.
