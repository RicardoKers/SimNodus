# Experimental managed record format 1

Date: 2026-09-24. Status: implemented inert codec for the
[ADR 0077 candidate](MANAGED_COMMIT_EXPERIMENT.md), not a storage/save API.
This is a private experimental format, with no migration or deployed compatibility
promise. The codec performs no filesystem operation and accepts no execution grant.

## Encoding

Every integer is unsigned little-endian; lengths count bytes. IDs are sixteen opaque
bytes, without GUID text/mixed-endian conversion. Digests are 32 raw SHA-256 bytes,
not hex text. No padding, BOM, optional fields, normalization or trailing bytes.
Non-null IDs/file IDs and predecessor digests cannot be all zero. Volume serials
may be zero. Inputs must remain stable during a call; returned data is owned.

| Offset | Bytes | Field |
|---|---|---|
| 0 | 8 | ASCII `SNMC0001`, including format version |
| 8 | 4 | Exact total length, including final digest |
| 12 | 4 | Reserved, must be zero |
| 16 | 4 | Revision, 1 through 64 |
| 20 | 4 | Binary principal SID length, 12 through 68 |
| 24 | 4 | Encoded resource context length, at most 16 KiB |
| 28 | 4 | Project length, 1 through 1 MiB |
| 32 | 16 | Store generation |
| 48 | 16 | Managed document ID, independent of project metadata IDs |
| 64 | 16 | Writer-assigned fresh commit ID |
| 80 | 16 | Stable operation ID |
| 96 | 108 | Expected predecessor token |
| 204 | 32 | Request digest |
| 236 | SID length | Exact binary principal SID |
| following | Context length | Exact encoded resource context |
| following | Project length | Exact declaratively validated project bytes |
| final | 32 | SHA-256 of all preceding record bytes |

The total is `268 + sid_length + context_length + project_length`, at most 2 MiB.
All individual bounds apply before count summation or payload allocation; the
current bounded fields cannot consume the whole envelope. No compression or archive
entries. A future extension requires a new format rather than ignored extra data.

A token is generation (16), document (16), revision (4), commit ID (16), record
digest (32), volume serial (8), file ID (16), in that order. Revision 1 requires
108 zero bytes. Later revisions require the same generation/document, revision minus
one, a non-null different commit ID, non-null digest and non-null file ID. The codec
checks these internal relations only. A future chain validator must compare the
token to the actually observed predecessor; fabricated identity bytes still parse.
The new record's own observed identity is added to its returned token after physical
publication, not guessed or embedded before creating the object.

The principal is a Windows binary SID: revision byte 1, subauthority-count byte
1 through 15, six identifier-authority bytes and exactly that many four-byte
subauthorities. No string-SID alternatives. Structural acceptance neither resolves
an account nor proves authentication; only the external authenticated operation may
provide this field. A SID appearing in a record cannot authorize itself.

## Resource context and request binding

The one-root context contains ASCII `SRC1` (4), binding ID (16), observed volume
serial (8), observed file ID (16), policy (4), locator byte length (4), then exact
UTF-8 locator bytes. Its size is `52 + locator_length`. Policy must be 1: the existing
local-resource capture policy, not a filesystem capability. Binding/file IDs must be
non-null. The codec enforces strict UTF-8 and the existing `root_syntax` contract:
drive-absolute root, 3 through 4096 bytes, existing component/alias restrictions.
UNC/device/relative paths, invalid UTF-8 and alternate data streams reject. Valid
locators retain their spelling; neither normalization nor prefix comparison proves
physical containment. Missing roots are not opened or rejected by this inert codec.
The eventual explicit capture must independently revalidate physical identity,
ancestors, reparse points, aliases and retained-handle containment.

Request SHA-256 input, without separators or implicit terminators:

1. ASCII `SNREQ001` (8).
2. Generation (16), managed document (16), operation ID (16).
3. Exact expected predecessor token (108).
4. SID length, context length, project length (three four-byte integers).
5. Exact SID, context and project bytes in that order.

The writer-assigned commit ID and resulting revision are not independent caller
request fields; the revision is fixed by the expected predecessor. Execution-run ID
is intentionally separate from stable operation identity. The future authority must
authenticate/fence the run outside this encoding. Receipt lookup, duplicate operation
rejection, globally fresh IDs, stale-token rejection, A-B-A chain checks and capacity
enforcement against an actual store are not implemented by this codec.

Both digests are recomputed on decode. Recomputing hashes can make modified bytes
internally consistent; it cannot establish origin, trust, redistribution rights,
interface compatibility, simulation readiness, physical containment or permission
to execute. Declarative project validation uses the unchanged native validator.
Opening/encoding a record neither loads DLLs/firmware nor downloads or renders resources.

## API and acceptance boundary

[The C++ codec](../../src/application/managed_record.hpp) exposes typed encoding and
owned decoding in `simnodus::experimental`. Errors distinguish size, format, internal
fields, context, project, digest and allocation failures. An error never returns a
partly valid record. Field validation can reject before digest validation; error
ordering is not a corruption-location or authenticity guarantee.

[Native checks](../../tests/resources/native_managed_record.cpp) cover owned results,
typed-input refusal and unchanged 1 MiB lock-digest limits. The existing SHA core is
shared behind a separately bounded 2 MiB experimental entry point; the original lock
entry point is unchanged. An independent Python `hashlib` vector checks the 2 MiB bound.
[Independent vectors](../../tests/resources/native_managed_record_regression.py) use
Python `struct`/`hashlib`, then require native decode/re-encode to reproduce the exact
bytes. Tests include recomputed outer hashes and fully hashed invalid field records,
not only accidental corruption. See [results](../experiments/SN-021-managed-record-codec.md).

This does not implement writing, provisioning, locking, chain scanning/recovery,
operation receipt lookup, transport integration or managed saving. Next implement
the bounded chain/write candidate and the predeclared consolidated physical VM batch.
No service installation or new simulation profile is required by this step.
