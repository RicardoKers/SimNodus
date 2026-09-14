# Native descriptor and binding declarations

Status: bounded SN-021 topology 0.3 contract under
[ADR 0058](../decisions/0058-native-descriptor-bindings.md).

Validate the complete preserved [0.3 rules](BINDINGS_DRAFT.md) over one owned
native syntax capture. Reuse structural and exact parameter phases internally;
the public topology 0.1 and parameter 0.2 operations remain separate and strict.
Reject other versions, full projects, unknown fields and duplicate JSON keys.
Do not project, reserialize, migrate or drop fields before validation.

## Acceptance criteria

- Validate all symbol/model records and their local anchors/terminals/slots,
  including unused descriptors. Preserve independent namespaces, ID/name grammar,
  electrical-only terminals, closed base units and exact decimal intervals.
- Require explicit bindings or null. Resolve by typed ID, never order or label.
  Require complete bijections for logical pins/ports and component parameters;
  reject aliases, missing/extra entries, wrong categories and unknown targets.
  A component's entire allowed interval must fit the model slot, with matching
  dimensions. Null never grants a fallback model or automatic selection.
- Count records, local entries, non-null bindings and every mapping pair in the
  combined 4096 entity/parameter/descriptor budget. Preserve 1 MiB ingress,
  nesting, eight-level hierarchy and 16384 expansion/resolution limits.
- Return an immutable declaration containing parameter occurrence snapshots and
  binding statistics, always with `simulation_ready: false`. Original catalog
  records, maps, explicit nulls and source token positions remain owned through
  `parameters.topology.syntax`; no second mutable graph or catalog is published.
- Compare acceptance/rejection categories, all statistics and effective parameter
  occurrence values/origins with unchanged Python fixtures and validators.
  Check exact budget boundaries, decimal interval neighbors, unused invalid
  records, separate namespaces, Unicode names, null/empty interfaces and nested
  unexpected fields. Symbol replacement must preserve topology/model/value data.
- Verify C++ ownership after caller mutation/result release, original binding
  token positions, structured reference offsets and public version isolation.
  Regress existing topology, parameters, syntax, schema and physical resources.

## Independent gates

This operation accepts caller-provided bytes and performs no filesystem or engine
I/O. Metadata association is separate from physical containment and verified bytes;
those remain the existing explicit Windows/NTFS same-handle snapshot operation.
No textual prefix or resolve-then-open proof is introduced. Its symlink, junction,
reparse-point, alias, missing-file, byte-limit, size/hash and replacement-race
policies remain unchanged. A checked path is not permission to reopen it later.

Declared interface agreement does not verify actual SVG anchors, SPICE source
formals, source provenance, redistribution rights, ELF/boot compatibility or
runtime capability/authorization. No download, DLL/model/firmware loading,
rendering, compilation, saving, public CLI or GUI follows from validation.
Full resource/link/project semantics and graph loading remain pending natively.
Preserve SN-044, local SN-045 documentation, shared instrumentation, MCU/toolchain
independence, numerical tolerances, PDF/PID fixes and SN-017 Python/GDB ownership.

Allocations remain bounded by the declared input/output limits, not a hard OS
memory or wall-time quota. Allocation failures are structured; no fault-injection
coverage is claimed. Native lone-surrogate rejection remains intentionally stricter
than the preserved Python reference. No engine integration claim is made here.
