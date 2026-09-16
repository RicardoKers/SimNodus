# Explicit standalone ideal RC compilation

SN-021's `compile_ideal_rc` accepts original project bytes, a local package root,
and an explicit request selecting `e01_ideal_rc` plus root reference, drive and
output port IDs. An unspecified profile rejects. This request authorizes creation
of an inert compilation artifact only; it is not permission to load/run an engine.
Project opening never invokes this operation automatically.

## Supported gate and output

Validate the complete project and structural connectivity first. Require the
project's co-simulation temporal mode to remain `unconfigured`, with no targets,
platforms or firmware. This separate standalone analysis neither rewrites nor
pretends to implement the project's co-simulation policy. Require exactly three
connection classes and two reachable components. The requested root ports must
exist and select distinct classes. No ground or stimulus is inferred from labels.

Capture the entire lock inventory with the existing native Windows/fixed-NTFS
retained-handle verifier. Match/bind the selected models against those owned bytes;
never reopen their paths. Keep the snapshots in the result. No DLL/model execution,
SVG rendering, dependency download or firmware loading occurs. Hashes do not prove
trusted origin or redistribution permission; those remain separate policy gates.

Require exactly one recognized resistor and one capacitor, with effective values
exactly 1000 ohm and 0.000001 F. Preserve prior exact-default/range checks. Require
source-formal R pins from drive to output and C pins from output to reference;
reversed maps reject in this narrow first profile. Reject all extra/floating nodes,
unsupported components, missing bindings, values or physical verification failures.
Hierarchical occurrence paths and net aliases remain intact.

Lower the recognized single-primitive wrappers into generated `R1` and `C1` lines.
No caller source token or directive is copied into the netlist. The explicit preset
supplies 3.3 V, zero capacitor initial voltage, 1 us step/max step and 5 ms duration,
using unchanged E-01 solver options. Preserve the request, full original graph,
captured resources, numerical bindings, node-to-connectivity groups and element
line-to-instance/parameter/model source maps. Lines are one-based; source spans
are zero-based half-open. Error coordinates identify project offsets, lock request
indices or selected model-source offsets. No partial netlist is returned on error.

## Predeclared acceptance

Derive one reachable RC branch from the owned two-RC fixture in memory; do not
modify historical fixtures or hashes. Verify explicit request/port failures,
complete metadata validation, extra connectivity, wrong values/maps, missing/null
models, missing/changed physical bytes, output ownership and full provenance.
Windows tests use real local NTFS. Other platforms test unsupported-platform
rejection and pre-I/O gates; filesystem acceptance is not claimed there.

An explicit engine runner uses only this owned fixture and fixed request. Verify
the nine pinned ngspice files, materialize the returned artifact into a fresh
ignored output directory, and run the unchanged E-01 host in a 30-second isolated
process. After compilation, replace the copied model resource: recompilation must
reject while the already owned artifact remains usable without path reopening.
Require finite monotonic samples, over 100 samples, 5 ms endpoint within 1 ps,
analytical RC error at most the unchanged E-01 0.0165 V, matching callbacks and
normal idle shutdown. Also report/check the unchanged fixture's 10 uV metadata
tolerance separately; do not silently rewrite it to the E-01 acceptance limit.
Preserve every attempted run and its hashes, including failures.

## Remaining boundaries

This is one explicit ideal RC profile, not general project execution or mixed-
signal compilation. All declarative readiness flags remain false. The co-simulation
temporal policy, Renode/firmware, SN-017 Python/GDB fixture ownership, UI and accepted
profiles/tolerances remain unchanged. Engine/profile integration beyond this gate
and safe overwrite remain pending. Full SN-021 is not complete.

See [ADR 0071](../decisions/0071-explicit-ideal-rc-compilation.md).
