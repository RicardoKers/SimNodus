# Native declaration ingress

Status: bounded SN-021 syntax contract under [ADR 0054](../decisions/0054-declaration-ingress.md).
Semantic project loading and graph construction remain pending.

## First slice and acceptance

Accept caller-supplied bytes, never a path, into an owned immutable syntax
document. Check the existing 1 MiB limit before copying. Parse exactly one JSON
value with at most 32 nested containers (root at depth zero), matching the
reference nesting rule. Reject invalid UTF-8, BOM, duplicate decoded object
keys, malformed escapes/numbers, comments, trailing input and incomplete values.
Reject unpaired escaped surrogates as a stricter native interoperability gate;
the historical Python validator is preserved, including its permissive behavior.

Use a small internal parser with no external dependencies, OS operations,
callbacks or implicit resource verification. This is a syntax reader for the
application boundary, not a general JSON SDK or a serializer. JSON grammar is
based on [RFC 8259](https://www.rfc-editor.org/rfc/rfc8259), with the project's
duplicate-key and Unicode restrictions. Preserve exact number tokens without
floating-point conversion; grammar acceptance does not approve numeric ranges.
For example, `1e9999` is syntax, not a finite accepted project quantity.

Return owned original bytes and preorder tokens with kind, byte begin/end,
subtree end index and decoded strings. Object children alternate string keys
and values; arrays contain values. A scalar's subtree ends at its next index.
Offsets are half-open byte ranges in the original input, not display columns.
String equality compares decoded Unicode scalars without normalization or case
folding; equivalent escape spellings collide, distinct normalization forms do not.
Nothing is silently dropped or rewritten. A complete immutable result or a
structured error (code and byte offset) is returned; no partial tree escapes.
Borrowed input must remain stable during the call. No views into caller memory
remain after return. Syntax tokens are not stable domain entity IDs.

Input size bounds token/string storage; nesting bounds recursion. No additional
entity budget replaces the existing schema budgets. Allocation failure returns
a memory error; a hard OS memory or wall-time quota is not claimed.

Tests must cover every token kind, retained integer/decimal spelling, nested
subtree navigation, ownership after input mutation, duplicate escaped keys,
Unicode boundaries, truncated prefixes, depth/byte limits and broad bounded
differential syntax cases. All saved schema JSON fixtures must pass syntax,
including semantically invalid fixtures; this explicitly tests the boundary.
Keep the original schema tests and physical-resource checks unchanged.

## Subsequent gates

1. Validate exact versions/fields, types, stable IDs, quantities, inventory
   fingerprints and all cross-references using the captured syntax document.
   Compare against the existing Python schema suite before native acceptance.
2. Build an independent immutable declaration graph, preserving definition IDs,
   occurrence paths, explicit unbound states and source locations. Reject cycles,
   expansion overflow and inconsistent mappings before publishing any graph.
3. Explicitly request physical snapshots using validated lock requests and an
   explicit root. Consume the returned bytes; never reopen checked paths.
4. Independently check interfaces, trust/redistribution, firmware/boot and actual
   runtime capabilities before an explicitly authorized executable session.

This slice implements only syntax ingress. Unknown versions/fields and invalid
entity relations can be syntactically valid; callers must not infer declaration
validity, containment, compatibility or simulation readiness from its success.
Saving, compilation, UI, resource rendering, engine invocation and SN-017 fixture
ownership remain outside this change. Opening a future project does not authorize
resource interpretation or execution.
