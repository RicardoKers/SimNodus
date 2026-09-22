# ADR 0072: Inspect ELF bytes without authorizing firmware loading

Date: 2026-09-16. Status: accepted for bounded SN-021 inspection.

## Decision

Add the pure [ELF32 envelope reader](../architecture/ELF_INSPECTION.md) behind a
firmware adapter. Accept only the explicitly bounded little-endian executable
envelope and return immutable bytes, raw header metadata and program records with
source offsets. Keep section contents and non-load semantics explicitly unverified. Report load
ordering as a diagnostic without reordering or approving it for a loader.
Reject dynamic/interpreter/TLS requirements and unsupported class/encoding/counts.

Do not interpret inspection as complete ELF validation, MCU/board support, a
memory map, boot readiness or execution permission. Do not make architecture
names or toolchain identifiers dependencies of the domain. Physical byte capture
and typed project association remain separate; no path is opened by this reader.

## Rationale and consequences

The project schema's `image_format: elf` and architecture string are declarations.
Safe byte inspection is needed before target-specific boot/interface checks, but
running a generic native loader would cross the untrusted-input boundary. This
small reader performs bounded arithmetic without loading dependencies or firmware.
No new runtime or electrical profile is accepted.

Real historical compiler output is inspected read-only alongside adversarial
synthetic inputs. No new engine integration is claimed because this operation
does not execute or alter engine behavior. Later target/runtime integration still
requires real-engine evidence. Preserve ADRs 0027/0028/0043/0051/0064/0071, all
historical evidence, numerical tolerances, SN-044 and the local SN-045 overlay.

The initial historical ELF inspection rejected nonmonotonic virtual addresses.
Preserve that failed attempt. The final reader reports `load_ordered: false` for
that file; successful inspection is not ABI conformance or loader acceptance.

SN-021 remains in progress. Next compose selected firmware declarations with
physically captured bytes and explicit architecture evidence before selecting a
device-specific boot contract. Overwrite ownership remains pending separately.
