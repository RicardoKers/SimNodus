# Experimental resource lock 0.1

Status: SN-020 metadata contract and developer reference validator, under
[ADR 0048](../decisions/0048-declarative-resource-lock.md). This is a separate
`simnodus-resource-lock` document, version `0.1`, not topology 0.4. The topology
0.1/0.2/0.3 documents remain unchanged. No resource loader is implemented.

## Shape and identity

The root has exactly `format`, `version`, and a nonempty `dependencies` array.
Each dependency has exactly `id`, `version`, `origin`, `license`, `files`, and
`content_sha256`. Dependency IDs are unique in the lock. Resource IDs are unique
within their dependency, using the existing lowercase stable-ID grammar.
References to resources will use dependency ID plus resource ID; integration
with symbol/model descriptors remains pending and no name-based binding exists.

Versions are literal three-part numeric strings, each part 1-6 digits, without
ranges, tags or update instructions. They identify this declared inventory, not
engine compatibility. No version ordering or dependency solver is implemented.
This flat lock has no dependency edges, transitive acquisition or package scripts.

`origin` has `kind` (`owned` or `external`), `author`, `reference`, and `revision`.
The last three fields are nonempty printable ASCII strings up to 512 characters,
without leading/trailing whitespace. References are inert provenance text, never
resolved as URLs or paths. Revisions are declarations, not authenticated upstream
identities. Do not place credentials in provenance. `license` has `identifier`
(the same text bounds) and `notice` (a resource ID in this dependency). The
identifier is a declaration, not a parsed license expression or redistribution
approval. External material still requires the existing provenance/permission
review before incorporation; this slice adds only owned fixture references.

Every file has exactly `id`, `path`, `bytes`, and `sha256`. Paths are relative to
one explicitly selected project/package root, never the current working directory
implicitly. Byte counts are nonnegative JSON integers (not booleans or floats).
SHA-256 values are 64 lowercase hexadecimal characters and refer to exact bytes,
without newline or encoding normalization. A notice is inventoried and hashed
like any other file. All files, including unused declarations, are validated.

`content_sha256` hashes the declared inventory using this precise recipe:

1. Sort files by their original ASCII `path` string.
2. Project each file to `path`, `bytes`, and `sha256` only.
3. Encode that array as JSON with object keys sorted lexicographically, no spaces,
   ASCII escaping, decimal integers, and no trailing newline.
4. SHA-256 the ASCII bytes of that serialization.

This fingerprint is insensitive to array order and local resource IDs. It binds
paths, sizes and file digests, not dependency metadata. It is neither a signature
nor a digest of an archive or an entire directory. Unlisted files are not approved
resources. A forged file hash plus a recomputed inventory hash remains valid
metadata; actual file verification and trust are separate gates.

## Portable lexical paths

Accept 1-240 ASCII characters, at most 16 slash-separated segments, each 1-80
characters. A segment starts with an ASCII letter, digit, underscore or hyphen;
remaining characters may also include periods. Trailing periods are rejected.
Case-insensitive stems before the first period may not be CON, PRN, AUX, NUL,
COM1-COM9 or LPT1-LPT9. The restricted alphabet also excludes other device syntax.

Reject absolute/drive-relative/UNC/device paths, backslashes, empty segments,
dot/dot-dot traversal, colons/alternate streams, percent encoding, tilde aliases,
wildcards, control characters, spaces and Unicode. Do not normalize or repair an
invalid path. Reject case-insensitive collisions across the entire lock, including
across dependencies, and file/directory prefix conflicts such as `a` and `a/b`.
These conservative limits are experimental portability policy, not a claim to
support every legitimate host filename.

Limits: 1 MiB encoded JSON, nesting below 32 containers, at most 32 dependencies,
256 total files, 16 MiB declared bytes per file, 64 MiB total declared bytes.
Duplicate JSON keys, non-finite numbers, unsupported fields and versions fail
with structured diagnostics. Parsing/validation never opens resource files.
The CLI reads only the explicitly supplied manifest, bounded to 1 MiB plus one
byte for overflow detection.

## Physical containment gate: specified, not implemented

Lexical validation cannot detect symlinks, Windows junctions/reparse points,
hard-link aliases, missing files, permissions or replacement races. Successful
metadata validation always reports `containment_verified: false`,
`resources_verified: false`, `redistribution_verified: false` and
`simulation_ready: false`.

Before a future loader consumes a resource, it must:

1. Obtain an explicit root from the caller and establish its actual filesystem
   identity. Fail closed when containment cannot be established.
2. Traverse under that root while rejecting symlinks/reparse points, including
   intermediate junctions. Reject non-regular files and ambiguous aliases. A
   textual prefix or `resolve`-then-open comparison alone is insufficient.
3. Open and inspect the same resource identity through a bounded handle-based
   operation, preventing path replacement between check and consumption. Pin
   or snapshot bytes; reusing a checked filename later is insufficient.
4. Enforce actual byte/count limits and verify exact sizes and SHA-256 digests
   from those handles before exposing any bytes to consumers. Fail on missing,
   changed, mismatched, inaccessible or escaping resources.
5. Keep graph validation, provenance/license review, model/backend capability
   negotiation and explicit execution policy separate. A verified resource
   does not authorize loading DLLs, scripts, compilers, decoders or firmware.

OS-specific handle semantics and tests for links/junctions, replacement races,
hard links, missing files and hash mismatch are SN-021 prerequisites, not passed
tests in this slice. No resource is copied, downloaded, extracted or executed
when opening metadata. Archive import and nonportable external references are
unsupported. Future schema changes must preserve earlier draft evidence.

## Fixture and next step

The [owned lock](../../tests/schema/fixtures/owned-resource-lock.json) declares
the existing 0.3 binding fixture and root LICENSE, using the repository root.
`0.3.0` is a fixture inventory label, not the application version. The recorded
revision identifies the source checkpoint containing both files. Unit tests
explicitly read these two trusted repository fixtures to verify their recorded
bytes; that test action is separate from the no-resource-I/O validator.

Next specify explicit resource references for symbol/model descriptors with
missing/wrong-kind reference tests, preserving the inert parsing boundary.
Board/MCU/firmware and temporal policies remain pending. No additional platform,
model, graphical renderer or simulation capability is validated.
