# Explicit project firmware capture and inspection

SN-021's `inspect_project_firmware(project, root, firmware_id)` composes the
existing complete declaration validator, physical resource verifier and inert
ELF32 reader. Call it explicitly; project opening never calls it automatically.

Validate the entire project first, then select an exact firmware ID before any
resource I/O. Resolve the declared dependency/resource pair, not a filename,
catalog position or architecture label. Capture the entire lock inventory under
the unchanged bounded Windows/fixed-NTFS handle contract. Missing or changed
unrelated inventory entries also reject; no partial inspection is returned.

Locate the selected record by both dependency and resource identity in the owned
snapshots. Pass only those retained bytes to the ELF reader; never reopen a path.
Retain the complete project declaration, all physical snapshots and the immutable
ELF inspection. Return firmware/architecture source offsets and the selected
snapshot index for provenance. Resource byte/count/total limits and ELF limits
remain unchanged. No additional image-size allocation is derived from ELF fields.

Expose `declared_architecture` separately from the observed `elf.machine`, ABI,
flags, entry and `load_ordered` diagnostic. There is deliberately no implicit
string-to-machine mapping or architecture-equivalence boolean. Even a matching
pair of labels/numbers would not establish device, boot, memory-map or ABI support.
No architecture is inferred from resource names or toolchain paths.

Errors distinguish declaration/selection project offsets, physical resource indices
(including the existing global-error sentinel), and ELF byte offsets. Preserve OS
error codes from physical capture. Results keep `firmware_verified`,
`runtime_profile_verified` and `simulation_ready` false. Captured hashes do not
prove trusted origin, licensing or permission to execute/distribute an image.

Accept identity-based association despite inventory reordering; reject unknown
selection before I/O, invalid complete metadata, missing files, size/hash changes
and matching-hash non-ELF bytes. Replace a fixture file after inspection and require
the retained bytes/provenance to remain unchanged. Include duplicate resource IDs
in distinct dependencies to verify the full identity pair. Inspect the historical
SN-012 compiler-produced image through this physical path without loading it.

This composes existing contracts under ADRs 0051/0053/0072; no new format, runtime
profile or architectural policy is introduced. Symlink/junction/reparse/alias and
race handling remain the physical verifier's existing tested responsibility.
Other platforms retain pre-I/O validation and unsupported-platform rejection.
No DLL/model/firmware execution, SVG rendering, dependency download or native loader
is invoked. A selected declaration may be unreferenced by any target: this is an
inspection request, not project simulation preparation.

Next define explicit architecture and device/boot correspondence for the accepted
fixture, preserving MCU/toolchain independence and requiring real engines before
integration claims. SN-017 Python/GDB/fixture ownership and all electrical/timing
profiles stay unchanged. Safe overwrite remains pending ADR 0064. Full SN-021
remains in progress. See the [acceptance report](../experiments/SN-021-firmware-inspection.md).
