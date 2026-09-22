# ADR 0073: Separate a static boot candidate from runtime acceptance

Date: 2026-09-22. Status: accepted bounded static inspection.

The resource/ELF composition exposes architecture evidence but no device/boot
correspondence. Add an explicitly selected, adapter-local SN-012 reference profile
with the [static candidate criteria](../architecture/BOOT_CANDIDATE.md). Reuse the
owned linker/platform/compiler evidence and bounded ELF reader. Do not infer a
profile from filenames or labels, and never normalize program order silently.

This provides testable architecture-field, region, initial-stack and reset-word
correspondence without invoking a loader. It neither approves arbitrary firmware
for this MCU nor expands the existing runtime/electrical profiles. Original bytes,
source offsets and the unordered-load diagnostic remain available to stricter
future consumers. No generic domain type depends on STM32 or its toolchain.

Require real-engine evidence when the project workflow is connected to a loader;
static inspection alone cannot prove loader mapping, execution or boot behavior.
SN-021 remains in progress. Next compose the explicit candidate with selected
project target/platform identity and trusted fixture resources before executing
the existing real-engine acceptance. Preserve SN-017 Python/GDB ownership,
SN-044, instrumentation, MCU independence, tolerances and PDF/PID behavior.
Safe overwrite remains independently pending ADR 0064.
