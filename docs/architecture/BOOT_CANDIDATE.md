# Static SN-012 boot candidate inspection

`firmware::inspect_boot_candidate(bytes, declared_architecture, profile)` is a
pure adapter operation. Require an explicit `sn012_stm32f103c8` profile and exact
`arm-cortex-m3` declaration. Reinspect the supplied bytes through the bounded ELF
reader; retain its immutable source and original program order. No path, loader,
memory write or engine is involved. An unspecified profile rejects before parsing.

## Profile-specific criteria

These are conservative fixture criteria derived from the owned
[linker script](../../tests/experiments/renode-stm32/firmware.ld),
[platform subset](../../tests/experiments/renode-stm32/stm32f103c8.repl) and recorded
compiler output, not universal ARM/ELF rules or a new executable project profile.

- ELF machine 40, OS ABI/version zero and exactly the observed flags `0x05000200`.
- Every program record is a nonempty `PT_LOAD`; opaque non-load records reject.
- RX (flags 5) records have equal file/memory sizes and virtual/physical addresses,
  entirely inside flash `[0x08000000, 0x08010000)`.
- RW (flags 6) records occupy RAM `[0x20000000, 0x20005000)`. Initialized bytes have
  their declared physical source inside flash; zero-file-byte records have equal
  physical/virtual RAM addresses. Other permission combinations reject.
- Reject pairwise overlap of virtual memory intervals, physical file-byte intervals
  and file-byte intervals. Use widened arithmetic; do not allocate zero-fill areas.
- Require at least eight file-backed RX bytes at flash start. Decode the initial
  stack/reset words there and retain their file offset.
- Stack is eight-byte aligned, above RAM start and no higher than RAM end. Reserve
  at least 1 KiB below it, above RAM start and every declared RW memory interval,
  matching the fixture's linker reservation.
- Reset has its low bit set, exactly equals the ELF entry, and identifies at least
  two file-backed RX bytes after clearing that bit. No instruction is decoded.

Return the owned ELF, selected profile, copied architecture token, stack, reset
word and vector file offset. Errors identify request/ELF/candidate/operation stage;
ELF/candidate offsets refer to image bytes, while request/operation offset zero
has no byte-location meaning. Keep the original `load_ordered` diagnostic unchanged.

## What this does not prove

This is a static candidate, not device compatibility or successful boot. Exact
header correspondence cannot prove instruction set use, reset code behavior,
initialization of data/BSS, interrupt vectors, peripheral access, runtime pacing,
section/relocation semantics or engine loader behavior. No section is interpreted.
Unordered load records remain unordered; passing these interval checks does not
certify ABI ordering or authorize a loader to consume the image.

Use physically captured bytes in a future explicit project composition. This pure
function establishes no containment, origin, license, target/platform-resource
correspondence or execution grant. It changes no declaration readiness flag and
does not implement project preparation. The MCU-specific policy stays in an adapter,
preserving ADR 0028 and the family-neutral domain.

Acceptance tests request/architecture/header mismatches, boundaries, overlapping
records, permissions, vector/reset/stack failures, ownership and unordered records.
Compare the real historical compiler image with its original hash and recorded
symbols without loading it. A future project-to-Renode integration must separately
prove the chosen loader semantics and real boot through the existing fixture.
See [ADR 0073](../decisions/0073-static-boot-candidate.md) and
the [acceptance report](../experiments/SN-021-boot-candidate.md).
