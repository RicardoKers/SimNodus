# SN-021: explicit reference target and real boot

Date: 2026-09-22. Status: bounded isolated reference acceptance; full SN-021 pending.
See the [contract](../architecture/REFERENCE_TARGET.md) and [ADR 0074](../decisions/0074-explicit-reference-target-boot.md).

## Acceptance and implementation

The native operation selects one explicitly requested project occurrence, resolves
platform/firmware identity, checks the offline device/architecture/pin interface,
captures all resources and checks exact owned reference pins plus static boot
criteria. It retains source positions and snapshots without executing them.
The separate explicit Python experiment consumes those bytes through a pinned
stage using the unchanged E-02 runner and all its assertions.

Seven native gate cases passed: six Windows passes/one non-Windows-only skip.
They cover explicit selection, complete metadata, device/architecture/interface,
board and target count, unpinned platform/firmware and physical missing resources.
These synthetic rejection cases are not positive backend evidence. The real
compiler-produced image and real Renode run below supply positive composition
evidence. All 49 Windows CTests passed; hosted results belong to the branch PR.

The first local gate run failed six cases because the generated circuit omitted
its mandatory explicit `symbol: null`. The declaration validator correctly
rejected before I/O. Only the test document was corrected; preserve both logs.
No failed engine attempt occurred in this slice.

## Real engine observations

The explicit run checked five pinned backend files and original firmware/control
provenance. It derived a single five-pin MCU occurrence in an unconfigured project,
captured the exact owned firmware/platform, then replaced both original package
files. Reinspection rejected; the retained native result still matched both inputs.

Staged firmware, platform and control-file writes were denied while their read-only
handles were retained; a stage-directory rename was denied with Windows error 5.
Python's buffered file-write exceptions exposed no Win32 error code (recorded as
null); do not relabel them as a measured specific sharing-violation code. Stage
hashes match expected input bytes after the run. All ancestor/file handles remained
open during consumption through volume-GUID paths. No textual-prefix containment
test or original-resource pathname reopening was used.

Both existing 100 and 1000 us quantum profiles passed in fresh Renode processes:

| Observation | Result |
|---|---|
| Final virtual time | 23,900 us in each profile |
| Periodic GPIO | 23 tick callbacks; measured gaps 998..1000 us / 999..1000 us |
| Boot data/BSS | Initialized value `0x13579bdf`; BSS zero; native boot assertions passed |
| Inputs/EXTI/modes/RCC stub | Unchanged E-02 assertions passed |
| Callback boundaries | Every recorded event was inside its native RunFor request |
| Exposure/cleanup | Expected loopback-only endpoint; exit zero; listener removed |

The unchanged native probe checks initial vectors/boot alias, stack and poisoned
data/BSS restoration. This is evidence for the known offline model and firmware,
not full STM32 peripheral/electrical fidelity. No co-simulation policy or GDB
ownership is changed. The image remains unordered by virtual address; it was not
rewritten or generalized into an accepted ELF loader contract.

The [audit](evidence/SN-021-reference-target-summary.json) records generated inputs,
binary/source hashes, engine results and preserved historical/negative evidence.
Raw outputs remain under `build/sn021-target-engine-01`; binaries are not published.

```sh
cmake -S . -B build/sn021-save
cmake --build build/sn021-save --config Debug
ctest --test-dir build/sn021-save -C Debug --output-on-failure
python tests/experiments/renode-stm32/project_boot_acceptance.py --output build/sn021-target-new-run
python tools/check_repository.py
git diff --check
```

The engine command requires the already installed pinned local runtime, prepared
control script, native probe and historical image; it downloads nothing and needs
a new output directory. It is an explicit fixture run, never a project-open action.

## Remaining gates

Configured temporal policy/runtime correspondence, arbitrary project-to-engine
compilation, safe overwrite and broader product acceptance remain pending. Review
the accumulated acceptance matrix before selecting the next bounded slice.
Preserve SN-017 Python/GDB/fixture ownership, SN-044, instrumentation, MCU/toolchain
independence, profiles/tolerances, PDF/PID and twelve unrelated local files. No UI
or other SN is advanced. Next-cycle prompt awaits full SN-021 completion.
