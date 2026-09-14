# Native exact parameters and scoped overrides

Status: bounded SN-021 implementation contract under
[ADR 0057](../decisions/0057-native-exact-parameters.md).

Implement the complete preserved [parameter 0.2 contract](PARAMETERS_DRAFT.md)
over owned native syntax. Reuse topology structure validation internally with
only the required 0.2 additions (`parameters`, `overrides`). The public topology
0.1 entry point remains strict and rejects 0.2. This new entry point rejects 0.1,
0.3 and full project documents. No migration, field dropping or parser expression.

## Acceptance

- Preserve original bytes, stable IDs and source offsets. Accept bytes only;
  never open resources, start engines, compile sources or modify definitions.
- Validate decimal strings exactly: existing grammar, 64 characters, 32 mantissa
  digits and exponent -24..24. Keep the closed, case-sensitive unit vocabulary.
  Store signed decimal digits and a power of ten; compare and scale without
  binary floating point, clipping or tolerances. Preserve negative zero and
  trailing decimal zeros in inspection output consistently with Python Decimal.
- Check all declarations, including unused definitions, and inclusive ranges.
  Parameters have their own per-definition namespace. Overrides must name a
  target parameter and contain exactly a literal quantity or a containing-circuit
  reference. Forwarded units must match and the entire source interval must fit.
- Enforce topology/entity/depth bounds and the combined 4096 declaration/override
  budget. Before output allocation, check at most 16384 resolved rows plus
  parameter entries for every definition, including those unreachable from root.
- Resolve in source instance order, separately per occurrence. Missing overrides
  use target defaults. Forwarding uses effective values of the containing
  occurrence. Root values use defaults. Publish an immutable inspection snapshot
  with paths, definition IDs, base-unit values and explicit origins; no editable
  domain graph, backend state or executable project is created.
- Attach row source offsets to instance declarations; attach value source offsets
  to the responsible default, literal value or forwarding reference use site.
  Retain the same owned syntax throughout validation and resolution; no reparsing
  or rewritten projection is used for source diagnostics.
- Compare native output, values/origins and rejection categories with the unchanged
  Python parameter suite. Add decimal grammar/precision/exponent/unit boundaries,
  signed zero, exact range neighbors, unused declarations, namespace/scope tests,
  resolved-budget boundaries and immutable ownership/source-offset checks.
  Existing topology, syntax, schema and resource suites must remain passing.

Successful resolution means valid parameters only. Full topology 0.3, descriptor,
lock/link and project semantics remain pending natively. Resource containment,
byte hashes, actual source interfaces, trust/redistribution and runtime approval
remain independent gates. No public CLI/automation API is selected by the test
probe. SN-044, preexisting SN-045 documentation, engine restrictions, numerical
tolerances, PDF/PID fixes and SN-017 Python/GDB ownership remain unchanged.

The existing native Unicode policy remains stricter than historical Python for
lone surrogates. Allocations are input/output bounded; allocation failure has a
structured diagnostic, without claiming a hard OS memory or wall-time quota.
