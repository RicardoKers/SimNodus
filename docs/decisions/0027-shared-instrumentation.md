# ADR 0027: Shared signal instrumentation and decoder boundary

Date: 2026-09-11. Status: accepted architectural direction; implementation and optional integrations pending.

## Context

RF-07 and RF-15 require signal observation and future circuit/firmware inspection. The existing [instruments section](../architecture/DEBUGGING.md#instruments) calls for exports before graphical instruments, provenance, and preserved events. The owner requests one infrastructure for probes, plots, scope, logic analysis, and decoding. This is documentation-only SN-043, independent of SN-017 extraction.

## Decision

Extend the existing instruments section as the authoritative detailed design. Share acquisition, analog sample storage, digital transition storage, and distribution across instruments and headless consumers. Reuse the kernel timeline and committed-state rules from [ADR 0002](0002-time-and-debugging.md), including current measured temporal restrictions.

Separate protocol decoding from the GUI through a conceptual decoder manager and common annotations. Leave native C++, optional Python, and optional sigrok adapter paths open without tying internal contracts to an external runtime or library. Preserve future correlation through stable circuit identities and timestamped MCU/peripheral provenance. Exact names, types, interfaces, and storage mechanisms are not selected.

## Alternatives

- Separate acquisition per instrument duplicates timing, retention, and provenance rules and weakens correlation.
- Direct ngspice-to-scope or Renode-to-logic-view paths violate existing adapter and presentation boundaries.
- A sigrok-dependent internal model would let an optional integration dictate core contracts before capability and licensing review.

## Consequences

Future instruments share captures and decoder results without Qt in acquisition or analysis. Future work includes bounded retention, distribution, decoder lifecycle, source mappings, and capability-aware inspection. The detailed design's suggested native decoder order is not an implementation schedule.

Python execution/isolation, decoder APIs, sampling, digital state encoding, annotation schema, persistence, and plotting remain open. Review exact licenses and distribution obligations before direct `libsigrokdecode` integration; MIT does not relicense dependencies. Opening a circuit must not execute decoders or automatically load code or dependencies.

This extends [ADR 0001](0001-core-and-backends.md) and ADR 0002 without superseding them. Accepted-versus-trial analog semantics, approximate-mode labels, same-time event limitations, and unsupported feedback/debugging restrictions remain in force. No conflict with existing decisions was identified. No engine capability, SN-017 scope/status, or classroom milestone changes; this ADR validates no instrumentation implementation.

## Revisit criteria

Revisit concrete design when SN-024 instrumentation or SN-033 inspection requires it, using measured capture size, event completeness, timing/provenance, and headless/GUI consistency evidence. Select optional decoder integrations only after capability, execution-policy, licensing, and distribution review. Changing virtual-time invariants requires a separate evidence-backed decision.
