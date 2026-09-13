# ADR 0028: MCU platform and external toolchain independence

Date: 2026-09-11. Status: accepted architectural principle; additional platform support pending.

## Context

STM32F103C8/Blue Pill and STM32CubeIDE are the initial reference platform and
debugging workflow. These choices must not make generic core or instrumentation
concepts depend on one MCU family or IDE. This records the owner's additional
principle within documentation-only SN-043.

## Decision

Keep STM32F103C8/Blue Pill as the MVP focus while preserving family-neutral MCU,
pin, firmware, and instance concepts. Put device-specific behavior in platform
descriptions and adapters. Treat Renode as a multi-platform emulation backend,
with other suitable backends possible. The [architecture boundary](../architecture/README.md#mcu-and-toolchain-independence)
defines the principle and illustrative naming; it does not mandate new classes.

Keep STM32CubeIDE, ESP-IDF, VS Code, and other IDEs/toolchains outside the core.
Prefer ELF and GDB-compatible interfaces where supported by the selected
platform/backend. Preserve the initial CubeIDE integration requirement and its
bounded tested profile without generalizing its capabilities to other devices.

The [shared instrumentation](../architecture/DEBUGGING.md#instruments) remains
MCU-family independent, including acquisition, signal/event stores, instruments,
and decoders. Device-specific inspection enriches common records through explicit
capabilities; it does not impose STM32 register or pin assumptions on consumers.

## Alternatives

An STM32-specific core would couple generic firmware and instrument concepts to
the first device. Embedding IDE/toolchain dependencies would couple simulation
to external workflows. Implementing multiple platforms now would expand the MVP
before validating their backend models and required features.

## Consequences

ESP32, AVR, other STM32 families, and other devices remain future candidates.
Effective support depends on available and mature CPU/peripheral models in the
chosen backend, including Renode, plus validated device, timing, electrical, and
debug behavior. No particular candidate is asserted to be supported by the pinned
Renode version. Model presence alone does not establish complete platform support.

This complements [ADR 0001](0001-core-and-backends.md) and
[ADR 0027](0027-shared-instrumentation.md); no prior decision is superseded and no
conflict was identified. Backend selection for new devices, concrete generic
interfaces, image/boot requirements, and platform-specific debug capabilities
remain open. No code, SN-017 scope/status, or classroom schedule changes.

## Revisit criteria

Before selecting another platform in the existing expansion roadmap, record
exact backend/version, CPU and required peripheral coverage, firmware and debug
compatibility, temporal/electrical limitations, and reproducible device-specific
evidence. Any proposed change to core timing invariants needs its own decision.
