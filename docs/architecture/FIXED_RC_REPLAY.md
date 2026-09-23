# Fixed E-01 project replay binding

Predeclared 2026-09-22 for SN-021. This binds configured project metadata to the
already measured ideal RC E-01 experiment; it adds no circuit or engine profile.
`compile_fixed_rc_replay` is a separate explicit inert operation. The existing
`compile_ideal_rc` and reference-target APIs still reject every configured policy.

Require complete project validation, explicit `e01_ideal_rc` request and reference,
drive and output ports, no platforms/firmware/targets, `known-schedule-replay`,
`causal-replay`, duration and exchange quantum both exactly 5000000 ns, and disabled
debugging. Existing 10 microvolt, 1 ps, 2000 ms and reject-on-unsupported declaration
bounds remain unchanged. A single interval has no intermediate exchange or pause;
quantum here is the whole 5 ms run, not ngspice's internal step or MCU timing.
The circuit, 3.3 V stimulus, zero initial charge, solver settings and 1 us maximum
step remain the exact E-01 profile. No SN-017 grant/GDB behavior is inferred.

The required locked schedule resource has exactly these UTF-8/ASCII LF bytes:

```text
time_ns,drive_uv
0,3300000
```

A final LF is mandatory. This is one fixed owned-fixture encoding, not a general
CSV importer: reject any other bytes, including extra events, directives, BOM,
CRLF or whitespace, even if their manifest hash matches. The single initial value
is held for the requested duration and binds to the explicitly selected drive
port. Do not infer a drive or ground from labels. No caller text enters the netlist.

Capture the whole lock inventory once through existing handle-relative Windows/NTFS
verification. Locate the schedule by full dependency/resource identity and consume
only its owned snapshot. Preserve original configured project bytes, resource index,
temporal/schedule source offsets and accepted duration/quantum beside the existing
circuit/node/element provenance. Errors identify policy fields or the schedule
reference. Keep all declaration readiness flags false: an inert prepared artifact
is not an engine, origin, redistribution or execution permission.

Acceptance requires valid policy binding, wrong mode/debug/time, missing/malformed
schedule, matching-hash unsupported contents, physical missing/changed bytes,
unchanged existing standalone rejection and retained-source ownership. Real ngspice
must consume the artifact in a separately invoked owned-fixture harness: replace
original schedule/model files after capture, reject recompilation and retain the
original artifact. Pin the generated netlist and every ancestor through consumption,
using a volume-GUID spelling from the retained stage handle. String prefix checks
and resolve-then-open are not containment proofs. Preserve E-01 assertions and also
check the project's 10 microvolt and 1 ps bounds against actual results.

No automatic execution, dependency download, DLL loading during compilation, SVG
rendering, firmware loading, new MCU, sampled feedback or debugging is introduced.
Safe overwrite and broader mixed-signal correspondence remain pending. The explicit
Python engine harness and SN-017 preparation/GDB/fixture ownership are unchanged.
