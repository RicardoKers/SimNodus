# ADR 0054: Bounded native declaration syntax ingress

Date: 2026-09-14. Status: accepted bounded implementation contract.

## Decision

Begin native declaration loading with [owned syntax ingress](../architecture/DECLARATION_INGRESS.md).
Retain original bytes and lossless tokens before implementing schema semantics
or publishing a domain graph. Use existing 1 MiB/32-container bounds, strict
UTF-8, decoded duplicate-key detection and exact number spellings. Reject lone
surrogates in native input without rewriting the historical reference validator.

The repository has no selected native JSON dependency. This slice uses an
internal bounded syntax reader, with differential and adversarial tests, rather
than adding a downloaded dependency or a Python process to the application API.
It has no writer, public mutation API, resource path parameter or engine access.
Review this decision if later semantic work needs a broader JSON library; any
replacement must preserve the captured bytes, diagnostics and acceptance gates.

## Consequences

Syntax success is deliberately weaker than declaration validity. No native
graph loader or project-open flow is claimed. Exact uint64 quantities will be
validated from number tokens, not rounded through double precision. Source
offsets support later diagnostics without selecting a save/formatting strategy.
Windows and Linux can test the same pure parser; Linux physical loading remains
unsupported. Existing schema evidence, physical snapshots, engine profiles,
SN-044, numerical restrictions and SN-017 Python orchestration remain unchanged.
