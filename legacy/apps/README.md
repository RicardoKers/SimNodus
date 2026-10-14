# Application entry points

`cli/` is the destination for headless backend experiments and reproducible simulation runs, broadened to advanced users, scripting, CI, external tools and AI agents under [ADR 0055](../../docs/decisions/0055-automation-first-headless.md). The [bounded `desktop/` slice](desktop/README.md) implements inert document inspection/name editing; full circuit editing and instruments remain pending.

Bounded experimental CLI runners exist under [SN-017 acceptance](../../docs/experiments/SN-017-acceptance.md); the product CLI and full desktop remain pending. The CLI comes first; both consume the same application/domain contracts. The CLI is a facade over shared application automation capabilities, not the owner of automation semantics. Neither facade owns a second scheduler or bypasses the kernel.

See [headless operation and automation](../../docs/architecture/AUTOMATION.md) for structured results, diagnostics, reproducibility, shared measurements and future API consumers. Existing fixture text protocols and backend-specific experimental commands remain bounded evidence, not a final public command design. No second CLI, MCP server or bindings are introduced.
