# Explicit SN-012 reference target

`inspect_reference_target(project, root, path, profile)` is an inert application
composition. Select `sn012_stm32f103c8` explicitly and an exact occurrence path.
Validate the whole project, require exactly one target and unconfigured temporal
policy, and resolve the selected platform/firmware through declared identities.
Do not infer a profile from labels or start a process while opening the document.

The offline reference requires device `stm32f103c8`, architecture `arm-cortex-m3`,
null board and exactly platform pin IDs `pa0` through `pa4`. Complete declaration
validation enforces the component/target path, null electrical model, architecture
agreement and bijective pin map. Retain the explicit logical-to-platform mapping;
this operation does not compile electrical wiring or execute other project graphs.

Use the unchanged native firmware composition to capture the entire inventory.
Locate the platform by its full dependency/resource pair. Require the captured
platform and firmware hashes to match the owned SN-012 reference inputs:

- Platform: `e8c8e3b588a80573cf98fabdebc78afa499f8504a10bf01fb89c7ac47016837b`.
- Firmware: `7895196a7e63134e5576f9bae2f4b124e7b0f1a3487891bea5a6ad56576b3689`.

These pins identify one previously measured owned experiment, not a general
project trust database or a claim that user-declared hashes authorize execution.
Other images/platform descriptions reject even when the manifest matches their
bytes. Run the existing static boot-candidate check against captured firmware.
Return retained declaration/resource/boot results, occurrence and platform IDs,
pin map, target/platform source offsets and selected platform snapshot index.
No pathname is reopened. Errors distinguish request, declaration, selection,
firmware/capture and boot stages with project/resource-index/ELF coordinates.
All declaration readiness flags remain false. Root/path and byte budgets do not
change. The native operation never invokes Renode, loads firmware or grants a
portable execution capability.

## Explicit real-engine acceptance

The Python-only experiment writes captured firmware/platform bytes and verified
control code into a fresh ignored stage. Its test helper reuses handle-relative
Windows/NTFS traversal, retaining every ancestor and every opened file without
write/delete sharing. It compares staged content with owned bytes through those
same handles before starting Renode, and retains the handles through both runs
and normal shutdown. A volume-GUID spelling is obtained from the retained stage
handle; namespace protection comes from retained handles, never string prefixes
or a resolve-then-open comparison. No original package path is consumed by Renode.

The test replaces original package files after native capture, rejects reinspection
and verifies the captured result is unchanged. Writes to staged files and a rename
of the staged directory must fail while pinned. The native source snapshots and
the experiment staging lease are separate: neither is an unrestricted pathname
capability. This helper is experimental fixture transport, not a production import,
save/overwrite API, crash-durable store or privileged-host defense.

Python retains preparation, transport and fixture sequencing. Reuse the unchanged
E-02 engine runner/assertions with only its input directory redirected to the pinned
stage. Explicitly verify local backend pins and prepared-control provenance before
the run. Test existing 100/1000 us quantums without changing tolerances, pacing,
peripheral characterization, callback boundaries, cleanup or the loopback policy.
No dependencies are downloaded. The outcome proves this reference-input workflow;
configured project co-simulation and general firmware execution remain pending.

See [ADR 0074](../decisions/0074-explicit-reference-target-boot.md) and the
[acceptance report](../experiments/SN-021-reference-target.md).
