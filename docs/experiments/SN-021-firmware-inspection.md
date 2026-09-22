# SN-021: selected firmware capture and inspection

Date: 2026-09-22. Status: bounded composition accepted; full SN-021 in progress.
The [contract](../architecture/FIRMWARE_INSPECTION.md) defines acceptance separately
from architecture correspondence, boot, device and runtime approval.

## Change and acceptance

The native application operation validates the complete project, selects a firmware
ID, captures the complete resource inventory and inspects the exact selected owned
bytes. It retains all inputs and source positions. No verified pathname is reopened
and no engine is invoked. Declared architecture and raw ELF machine evidence remain
separate fields; readiness stays false. Existing parsers/path policies are reused.

Nine regression cases cover full metadata and selection gates; actual NTFS capture;
ownership after file replacement; size/hash/missing-file rejection; matching-hash
invalid ELF; inventory ordering and explicit reference selection; identical resource
IDs in different dependencies; whole-inventory requirements; unsupported platform.
Windows passes eight with one non-Windows-only skip. The complete Windows suite
passes 47 CTests. Hosted Linux/Windows results are recorded by the publication PR.
The earlier targeted run had seven passes/one skip before the dependency-identity
case was added; its log is retained. No failed acceptance attempt occurred here.

The explicit historical fixture run checks the original SN-012 image hash
`7895196a7e63134e5576f9bae2f4b124e7b0f1a3487891bea5a6ad56576b3689`
against its original build record, copies it into a fresh ignored package, creates
an inert firmware declaration and invokes the native physical composition. After
capture the copied image is replaced. The result still owns exactly the original
9712 bytes, reports machine 40 and preserves `load_ordered: false`. The declaration's
`arm-cortex-m3` label is retained, not certified as equivalent to that machine code.
The original fixture is unchanged; no image is booted or loaded into an engine.

The [audit](evidence/SN-021-firmware-inspection-summary.json) records source and
historical hashes, raw logs, generated project/package hashes and the fixture
result. Previous ELF order failures and all historical evidence are preserved.

```sh
cmake -S . -B build/sn021-save
cmake --build build/sn021-save --config Debug
ctest --test-dir build/sn021-save -C Debug --output-on-failure
python tests/schema/native_firmware_regression.py --probe build/sn021-save/Debug/native_firmware_probe.exe --image build/sn012/firmware/firmware.elf --build-record build/sn012/firmware/build.json --output build/sn021-firmware-new-run
python tools/check_repository.py
git diff --check
```

The fixture command requires existing local inputs and a new output directory.
No dependencies or binary images are published/downloaded. This proves physical
association and retained-byte inspection, not real firmware/runtime integration.

## Remaining work

Next define explicit architecture/device/boot correspondence for the existing
owned fixture, including ELF ABI/flags, memory/entry/vector requirements and
rejection of unsupported semantics. Do not infer a loader plan from this reader
or silently approve its unordered/opaque records. Obtain real-engine evidence
before claiming target/runtime integration. Safe overwrite remains pending its
identity/version ownership protocol. Preserve all accepted profiles/tolerances,
SN-017 Python/GDB ownership, SN-044, instrumentation, MCU/toolchain independence,
PDF/PID behavior and the unrelated local overlay. No UI or other SN is advanced.
The next-cycle prompt remains pending full SN-021 acceptance.
