# SN-021 configured replay lifecycle acceptance

Predeclared and executed 2026-09-22. Result: passed for fixed configured replay.
Full SN-021 remains in progress.

Compose existing native create/acquire/name-edit/save-copy/reacquire APIs and
explicit fixed replay compilation in the test-only lifecycle probe. Select replay
explicitly; the original standalone path remains the default. Do not add automatic
compilation to project acquisition, revision or save APIs. No production API changes.

Acceptance requires exact original capture, a name-only byte edit, persisted/reopened
byte identity, unchanged complete temporal policy and schedule reference/inventory,
source-map offsets into the revised capture and an identical pre/post-save netlist.
Exercise existing-destination preservation, capture surviving original replacement,
invalid metadata/edit failures, and missing/replaced model or schedule stopping only
at explicit compile. Valid unsupported temporal requests remain editable/saveable
but reject compilation without silent policy changes.

The explicit real ngspice harness must consume the post-reopen native artifact
under the existing retained staging handles and unchanged 10 microvolt/1 ps project
bounds. Compare the artifact against independent pre-save compilation, then replace
source model/schedule files and reject recompilation. Keep prior engine assertions,
source mapping, readiness false and the SN-017 Python/GDB/fixture boundary.

Safe overwrite remains pending ADR 0064; create-only save/copy is not an overwrite
protocol. No UI, engine profile expansion, downloads, issues or binary release.

## Measured result

All 19 Windows lifecycle cases passed: eight existing unconfigured cases and eleven
configured cases (the same eight plus schedule replacement, missing schedule and
valid-but-unsupported policy). Three targeted CTests passed. Non-Windows CI skips
these physical lifecycle cases; it does not establish portable persistence.
The test probe selects replay explicitly; no production API or engine input changed.

The real run `build/sn021-replay-lifecycle-engine-01` passed on its first attempt.
The original document was preserved; the saved/reopened bytes differ only in the
name token. Policy, schedule reference and inventory remain unchanged. Revised
policy/reference offsets are 12582/12734 bytes in this generated fixture; element
source offsets are independently checked against the revised document. The native
post-reopen netlist equals independent pre-save compilation and is the artifact
actually handed to the protected stage. Source schedule/model replacement after
compilation rejects recompilation without invalidating that artifact.

Ngspice produced 5012 samples with maximum analytical error
9.889724283951296e-08 V and final virtual time 0.004999999999999989 s, satisfying
unchanged 10 microvolt and 1 ps project bounds. Callback samples match vectors;
callback faults are zero, engine was idle before quit and process exit was zero.
Stage handles retained through consumption denied netlist writes (Win32 code not
exposed by Python) and directory rename (error 5). No original resource path is
reopened for execution; the existing volume-GUID/handle-relative stage is reused.

The [audit](evidence/SN-021-replay-lifecycle-summary.json) records three changed
test/harness source hashes, six unchanged prior source hashes, 105 unchanged
historical evidence files, fourteen schema fixtures and twelve preserved local
files. Raw traces/binaries remain ignored. The prior replay's first failed harness
attempt remains unchanged; this cycle does not relabel or overwrite its result.

```sh
cmake --build build/sn021-save --config Debug --target native_lifecycle_probe
python tests/schema/native_lifecycle_regression.py --probe build/sn021-save/Debug/native_lifecycle_probe.exe -v
ctest --test-dir build/sn021-save -C Debug -R 'native-project-lifecycle|native-fixed-rc-replay|native-ideal-rc-compilation' --output-on-failure
python tests/experiments/ngspice/fixed_replay_acceptance.py --lifecycle --output build/sn021-replay-lifecycle-new-run
python tools/check_repository.py
git diff --check
```

Use a fresh engine output directory and existing pinned dependencies; nothing is
downloaded. This composition is test-only under ADR 0075 and needs no new ADR.
The bounded configured lifecycle is now measured; safe overwrite remains the
explicit implementation gate before final SN-021 acceptance. Define a candidate
ownership/concurrency protocol under ADR 0064 before testing it; do not repeat
check-then-replace or infer ownership from metadata/hash/read handles. Keep other
runtime modes unsupported, SN-017 Python/GDB control, SN-044, instrumentation,
MCU independence, tolerances and PDF/PID. Next-cycle prompt awaits full acceptance.
