# SN-021: static reference boot candidate

Date: 2026-09-22. Status: bounded static inspection accepted; full SN-021 pending.
See the [contract](../architecture/BOOT_CANDIDATE.md) and [ADR 0073](../decisions/0073-static-boot-candidate.md).

The pure adapter requires an explicit SN-012 STM32F103C8 profile and architecture
token. It owns/reinspects ELF bytes, checks the recorded header fields, flash/RAM
regions, segment overlap, stack reserve, vector words and ELF entry. It preserves
program order and reports an inert candidate; no path or engine is accessed.

Eight adversarial cases passed, covering request gates, owned results, architecture
fields, permissions and regions, three kinds of overlap, initialized/zero-fill
data, unordered records, stack boundaries/reserve and reset/vector failures.
All 48 Windows CTests passed. Hosted cross-platform results belong to the branch PR.

The historical SN-012 image retains SHA-256
`7895196a7e63134e5576f9bae2f4b124e7b0f1a3487891bea5a6ad56576b3689`.
Read-only candidate inspection passed: 9712 bytes, vector file offset 4096, stack
`0x20005000`, reset `0x080000ad`, `load_ordered: false`. Stack/reset match the original
build record's linker symbols (including the reset low bit). The image and original
build record remain unchanged. No new engine run, boot or loader behavior is claimed.
The [audit](evidence/SN-021-boot-candidate-summary.json) records source, fixture,
historical evidence, binary and raw-log hashes. No failed attempt occurred here;
earlier ELF-order failures remain preserved.

```sh
cmake -S . -B build/sn021-save
cmake --build build/sn021-save --config Debug
ctest --test-dir build/sn021-save -C Debug --output-on-failure
python tests/schema/native_boot_regression.py --probe build/sn021-save/Debug/native_boot_probe.exe --image build/sn012/firmware/firmware.elf --build-record build/sn012/firmware/build.json --output build/sn021-boot-new-attempt.json
python tools/check_repository.py
git diff --check
```

The explicit fixture command requires an existing image/build record and a new
output file. No dependencies, compilation toolchains or binaries are downloaded.

Next connect explicit profile selection with project target/platform identity and
trusted fixture resources, then obtain real-loader/boot evidence through the
existing Renode fixture. Static acceptance must not silently approve section,
instruction, initialization, interrupt, device or runtime semantics. Keep selected
firmware snapshots owned; never reopen a previously verified path. Safe overwrite
and full executable-project acceptance remain pending. Preserve SN-017 Python/GDB/
fixture control, SN-044, instrumentation, MCU/toolchain independence, profiles,
tolerances, PDF/PID, historical negatives and the local overlay. No UI or other SN
is advanced. Prepare the next-cycle prompt only after full SN-021 completion.
