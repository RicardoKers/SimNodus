# Bounded inert ELF32 inspection

SN-021's `firmware::inspect_elf32` is a pure adapter operation on borrowed bytes
that must remain stable during the call. It copies the bounded input before
parsing and returns immutable owned bytes and program-header records, or a
structured code and zero-based byte offset. It has no file, engine or OS-loader
dependency. No project-open path invokes it implicitly.

## Selected envelope

- Maximum 16 MiB, matching the existing per-resource capture budget; minimum
  52-byte header. Decode individual little-endian fields, never cast packed structs.
- ELF32, little-endian, both version fields equal to one, `ET_EXEC`, and exact
  52-byte ELF/32-byte program-header sizes. Reject ELF64, big-endian, shared objects,
  relocatable files and extended program counts in this initial envelope.
- Require 1..64 program records, with their complete table after the ELF header
  and inside the captured file. Require at least one `PT_LOAD` record.
- Check each non-null program's file interval. For load records require file size
  no greater than memory size, virtual and declared physical intervals within the
  half-open 32-bit address space and power-of-two
  alignment/congruence when alignment exceeds one. Zero and one impose no alignment.
- Reject dynamic, interpreter, reserved shared-library and TLS program types.
  Other non-load records remain opaque after file-extent checking. Null records
  retain their unspecified fields without interpreting them.
- If present, the section table has 1..4096 entries of 40 bytes, lies after the
  ELF header and within the file, and has a bounded name-table index. Extended
  section counts/indexes are unsupported. An absent table has zero offset/count/
  name index. Section entries, names, links, payloads and relocations are NOT parsed.

Sizes and products are widened before arithmetic; containment uses subtraction
after checking the starting offset. Address intervals may end exactly at 2^32.
No buffer is allocated from declared memory size and no zero-fill is materialized.
Total storage is bounded input plus at most 64 small records.

Retain machine, OS ABI, ABI version, entry and flags as raw numbers, including
unknown values. Preserve all original bytes and each program header's source
offset. Report whether load records have nondecreasing virtual addresses as
`load_ordered`; preserve the original order even when false. Numeric decoding does not certify that a machine number is assigned,
supported or compatible. No ARM/STM32 assumption enters the domain or this reader.

## Deliberate limits

Success means only this inspected envelope passed. It is not full ELF validity,
a load plan or a firmware compatibility result. Unordered loads, overlapping segment addresses,
overlapping metadata tables, unknown program types/flags, opaque section data and
an entry outside executable segments do not receive semantic approval. They remain
unresolved inputs to a future stricter consumer, which must explicitly reject
unsupported semantics before constructing a memory/boot plan. A successful result
must never be passed directly to an engine as an approved executable.

Physical containment, same-handle resource capture, declared hashes, image/target
association, device memory maps, vector tables, boot behavior, ABI compatibility,
trusted origin, redistribution permission and execution authorization remain
separate gates. A future composition must consume retained resource bytes, not
reopen a previously verified pathname. This pure reader claims no containment.
All existing declaration/readiness flags remain unchanged and false.

## Acceptance

Test all prefixes before a minimal image's payload end; malformed header fields;
table counts and offsets; 16 MiB and 64/4096 entry limits; file/address overflow;
alignment; load order; dynamic/interpreter/TLS rejection; empty and zero-fill
segments; opaque records; exact field/provenance ownership after input/result
release. Compare accepted decoding with independent Python `struct` extraction.

Inspect the existing owned SN-012 compiler-produced ELF after checking its original
build-record hash. This is read-only real compiler-output evidence, not a new engine
or device-integration run. The complete existing test suite must continue passing.
No dependencies are downloaded or firmware loaded by this acceptance.

The field layout is checked against the primary [ELF header specification](https://gabi.xinuos.com/elf/02-eheader.html)
and [program-header specification](https://gabi.xinuos.com/elf/07-pheader.html),
accessed 2026-09-16. The limits and exclusions above are SimNodus policy, not
claims that every excluded file violates ELF. See [ADR 0072](../decisions/0072-inert-elf-inspection.md).
