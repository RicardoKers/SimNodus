# ADR 0079: Rebuild the product layer and adopt a lightweight process

Date: 2026-10-14. Status: **accepted** (owner decision in conversation, 2026-10-14).

## Context

Between 2026-08-31 and 2026-10-06 the project produced careful backend evidence
(E-01..E-05) but very little usable product. The desktop application never ran
ngspice or Renode; it opened one fixed RC project and edited three values. The
product code accepted exactly one fixture (ideal RC 1 kΩ/1 µF, one 227-byte SVG,
5/100 ms grants), edited documents by splicing JSON bytes, and implemented file
I/O only through Windows NT APIs. About 80 % of the main window source consisted
of acceptance scripts compiled into the executable. Planning documents reached
roughly 31 000 lines against 13 000 lines of C++, and SN-023 advanced in very
small separately gated increments (for example eight on zoom/pan/keyboard of a
fixed two-component drawing). The owner now needs a usable simulator to prepare
lessons and asked for different interface requirements.

## Decision

1. Rebuild the product layer (document model, editor, library, netlist,
   simulation session, analyzer) according to
   [PRODUCT_ARCHITECTURE](../architecture/PRODUCT_ARCHITECTURE.md) and the
   revised [requirements](../REQUIREMENTS.md).
2. Keep the backend knowledge and assets: experiments under `tests/experiments`,
   pinned versions, owned firmware, the owned F103 ADC model, the Renode
   cancellation patch, measured coupling semantics (ADR 0010/0011/0012/0014).
3. Move the previous product code to `legacy/` (not built by default). Its ideas
   may be reused selectively; nothing there constrains the new code.
4. Replace the project format of SN-020/SN-021 with a simple versioned `.snx`
   JSON document. Save with a standard atomic write plus backup; the hardened
   managed store (ADRs 0063–0077) is not used by the product.
5. Lightweight process: requirements, architecture, roadmap, backlog, a short
   CURRENT status and ADRs only for decisions with lasting consequences.
   Evidence is automated tests and CI, not narrative reports per increment.
   Releases every two weeks with owner hands-on testing.
6. Portable code (Windows and Linux) with Windows as the classroom platform.

## Alternatives

- **Evolve the existing code.** Rejected: every layer is fixture-specific; the
  editing model cannot add components or wires; I/O is Windows-native only.
- **Rebuild but keep the SN-020 schema.** Rejected: the declaration schema has no
  geometry and its multi-file locking/linking model is heavier than needed now.

## Consequences

- Faster path to a usable editor; superseded history remains in Git and in
  `docs/archive/2026-10-legacy/`.
- ADRs 0015–0026, 0029–0042 (fixture extraction slices), 0045–0048, 0050–0054,
  0056–0077 (declaration/persistence) are historical for the product. ADR 0049 is
  superseded in its layout details by ADR 0081 while its analyzer/probe
  principles are kept. ADR 0055 (automation-first) is deferred to SN-163.
- Some previously validated guarantees (byte-exact preservation, NTFS-specific
  protections) are intentionally not carried over.

## Revisit criteria

If the rebuilt layer misses release 0.1 or 0.2 by more than a week, review scope
with the owner rather than restoring the previous process.
