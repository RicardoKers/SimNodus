# SN-021: inert ELF32 envelope inspection

Date: 2026-09-16. Status: bounded reader acceptance; full SN-021 remains in progress.
See the [contract](../architecture/ELF_INSPECTION.md) and
[ADR 0072](../decisions/0072-inert-elf-inspection.md).

## Scope and acceptance

Implement a pure C++20 adapter with no OS/engine dependency, operating on a bounded
owned byte copy. Return raw ELF32 little-endian executable header/program records
and source offsets. Before implementation, acceptance selected 16 MiB, 64 program
headers, section-table envelope bounds, truncation/overflow/unsupported-format
rejection and inspection of an existing compiler-produced owned firmware image.
No image is mapped, relocated, booted or executed. Physical resource capture and
typed project association remain separate operations.

The [regression suite](../../tests/schema/native_elf_regression.py) has 12 cases,
including every one of 260 truncated prefixes; exact file/table/address limits;
integer overflow; alignment; zero-fill size without allocation; unsupported dynamic,
interpreter and TLS types; opaque records; unknown raw machine/ABI values; and
ownership after caller input and result release. Python `struct` independently
compares all exposed fields and offsets. These synthetic parser tests make no
claim about real backend/device behavior.

All 12 final cases passed locally. All 46 Windows CTests passed. Linux hosted
results belong to the publication PR; this reader has no filesystem dependency.

## Historical compiler-output attempt and correction

The explicit [fixture reader](../../tests/schema/elf_fixture_acceptance.py) checks
the existing SN-012 `firmware.elf` digest against its original `build.json`, then
compares native fields with independent binary extraction. Its output file must
be new, and failed attempts are retained. No compiler, Renode or firmware loader
is invoked. The image remains local; no firmware binary is published.

The original 9712-byte image retains SHA-256
`7895196a7e63134e5576f9bae2f4b124e7b0f1a3487891bea5a6ad56576b3689`.
It was produced by the recorded GNU Tools for STM32 GCC 14.3.1 toolchain.
The first inspection **failed** at byte 124 with `load-order`: its three load
records have virtual addresses 0x08000000, 0x20000100 and 0x20000000. The final
zero-file-byte record represents 256 declared memory bytes. The original build
and image have not been changed to satisfy the reader.

The initial reader treated nondecreasing virtual addresses as a rejection gate.
Review separated this ABI/loader concern from inert record inspection. The final
reader preserves the original records and exposes `load_ordered: false`; it does
not sort, rewrite, load or certify them. This is a documented inspection-policy
correction, not a claim that the file satisfies the generic ABI ordering rule or
is newly approved for execution. The independent decoding comparison then passed.
Both attempts, probe hashes and the original build-record hash are retained in
the [audit](evidence/SN-021-elf-inspection-summary.json).

Reproduce with the existing local compiler output:

```sh
cmake -S . -B build/sn021-save
cmake --build build/sn021-save --config Debug
ctest --test-dir build/sn021-save -C Debug --output-on-failure
python tests/schema/elf_fixture_acceptance.py --probe build/sn021-save/Debug/native_elf_probe.exe --image build/sn012/firmware/firmware.elf --build-record build/sn012/firmware/build.json --output build/sn021-elf-new-attempt.json
python tools/check_repository.py
git diff --check
```

## Remaining gates

This result does not validate section contents, relocation/dynamic semantics,
overlapping mappings, entry/boot/vector semantics, device memory maps, ABI flags,
trusted origin or licensing. It neither modifies nor sets declaration readiness.
No declared machine value is interpreted as support for ARM, AVR, RISC-V or any
other MCU. Unsupported input here means outside this reader's bounded contract,
not necessarily invalid ELF. Preserve the unchanged electrical/runtime profiles.

Next bind selected firmware resource declarations to physically captured bytes
and explicit architecture evidence, retaining unknown/unverified fields and
diagnostics. Define device/boot rules before any real target integration. Do not
reopen verified paths or treat a matching hash/header as execution authority.
Safe overwrite remains separately pending ADR 0064. Preserve SN-017 Python/GDB/
fixture ownership, SN-044, shared instrumentation, MCU/toolchain independence,
all historical/negative evidence, tolerances and PDF/PID behavior. No UI, downloads,
issue synchronization or release. The next-cycle prompt awaits full SN-021 closure.
