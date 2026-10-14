# ADR 0055: Automation-first headless application boundary

Date: 2026-09-14. Status: accepted architectural direction; product interface and implementation pending.

## Context

[ADR 0001](0001-core-and-backends.md) already places CLI and GUI over a headless core. The owner now explicitly requires automation for advanced users, scripting, CI, external tools and AI agents, beyond internal experiments. [ADR 0027](0027-shared-instrumentation.md) and [ADR 0049](0049-desktop-ux-and-measurements.md) require shared observations and measurements. Existing bounded runner protocols are experimental, not a public product interface.

## Decision

Adopt the [automation direction](../architecture/AUTOMATION.md): the existing CLI destination is a facade over shared application/session and domain capabilities. Future Automation API consumers reuse those capabilities, independent of Qt and backend-specific APIs. Structured results, stable diagnostic codes/entity IDs and a documented stable exit policy are first-class requirements; exact schemas, numbers and syntax remain open, with JSON the leading candidate.

All facades share measurements, diagnostics, kernel time and capability/security gates. MCP and Python/notebook clients are future possibilities only; no embedded AI, transport, new runtime dependency or public API is selected. No code, pipeline or SN-021 change is part of this decision.

## Alternatives

- An experiments-only CLI would omit the intended external automation consumers.
- Automation implemented primarily in command parsing would duplicate application behavior across GUI and future facades.
- Backend-specific public commands would couple consumers to engines and device details.
- Selecting MCP or a transport now would constrain an application contract that has not yet been validated.

## Consequences

The shared boundary supports reproducible unattended use and consistent observations across consumers. Stable public contracts will require compatibility, failure and real-engine acceptance evidence. The bounded SN-017 runner and schema validators keep their measured protocols; this direction does not retroactively change them. The minimal headless foundation belongs with M2/M3, while a full command catalog, MCP and bindings remain outside the classroom commitment. No third-party material or dependency is added.

## Revisit criteria

Revisit concrete API boundaries when implemented product flows expose a measured limitation. Record public result/exit compatibility and any future transport/trust choice in later ADRs. Preserve causality, shared instrumentation and untrusted-input boundaries. Track this documentation under SN-045 in the [backlog](../planning/BACKLOG.md); earlier ADRs are complemented, not superseded.
