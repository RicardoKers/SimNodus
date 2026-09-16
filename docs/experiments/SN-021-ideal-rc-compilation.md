# SN-021 explicit ideal RC compilation

Date: 2026-09-16. Status: accepted bounded slice; full SN-021 in progress.
Base main: `edbb87fa4182f7b5c27174936b1fef832f177c7c` (PR #37).
Branch: `codex/sn-021-ideal-rc-compile`.

The [contract](../architecture/IDEAL_RC_COMPILATION.md) and
[ADR 0071](../decisions/0071-explicit-ideal-rc-compilation.md) define explicit
standalone analysis authority and predeclared acceptance. Complete project and
connectivity validation precede retained-handle resource capture. Owned captured
bytes feed the existing source/interface/numerical gates. A fixed ideal RC artifact
is emitted only for explicit distinct reference/drive/output root ports, three
connection classes and correctly oriented 1 kohm/1 uF primitives. Original project,
snapshots, bindings, node groups and element/parameter/model provenance are retained.
The netlist contains generated primitives only, with unchanged E-01 options.

Eight regression cases: seven Windows passes and one explicit non-Windows-only
skip. They cover request/metadata gates, extra/floating connectivity, distinct
port names sharing a node, ownership/source maps, metadata order, physical
missing/hash failures, profile values/wiring, absent models and unsupported source
directives even with a matching updated hash. All 44 Windows CTests passed.
Linux can validate pre-I/O gates and unsupported-platform rejection; it does not
claim Windows filesystem acceptance. See the [audit](evidence/SN-021-ideal-rc-summary.json).

## Real engine acceptance

The explicit runner derived one RC branch from the owned fixture without changing
historical inputs. The native compiler verified the real NTFS package and returned
the netlist. The runner replaced only the copied model afterward: recompilation
rejected its size while the owned artifact executed successfully without reopening
the resource. The nine pinned ngspice files passed hash verification; no local
dependencies were downloaded. The unchanged E-01 host ran the generated artifact.

Result: 5012 samples through 5 ms, maximum analytical error
`9.889724283951296e-08 V`, normal idle shutdown and matching callback/final vectors.
Both unchanged E-01 `0.0165 V` and fixture metadata `10 uV` voltage thresholds passed;
endpoint tolerance remained `1e-12 s`. This evidence proves the fixed ideal RC
artifact only. No other electrical profile, general project runtime or solver
rounding guarantee is inferred. Attempt outputs/hashes, netlist and generated
project metadata are retained in the audit; raw files remain in ignored build.

```powershell
cmake --build build/sn021-save --config Debug
ctest --test-dir build/sn021-save -C Debug --verbose
python tests/experiments/ngspice/rc_compilation_acceptance.py --output build/sn021-rc-new-run
python tools/check_repository.py
```

Engine reproduction requires the existing local E-01 executable/pinned dependencies
and a new output directory. The explicit profile does not override configured
co-simulation; it requires `unconfigured` with no targets/platforms/firmware.
Compilation and execution are separate actions. Declaration readiness flags stay
false. Project opening does not compile/run anything. SN-017 Python/GDB/fixture
ownership, MCU/toolchain independence, UI, instrumentation, PDF/PID and historical
tolerances stay unchanged. Remaining project/runtime integration and safe overwrite
under ADR 0064 still prevent closing full SN-021. Review remaining acceptance gates
before the next slice; prepare the next-cycle prompt only after full completion.

## Publication record

Source `d605827` was published in [PR #38](https://github.com/RicardoKers/SimNodus/pull/38),
which records final required hosted checks and protected-main squash integration.
Eight source hashes match publication; 184 historical hashes and twelve local
SN-045 files are preserved. The checker passed for 637 workspace / 635 publication
files. Verify final PR/head/checks and main identity before continuation.
