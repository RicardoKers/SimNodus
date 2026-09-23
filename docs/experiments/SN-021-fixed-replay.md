# SN-021 fixed configured replay acceptance

Date: 2026-09-22. Result: passed for the [fixed E-01 binding](../architecture/FIXED_RC_REPLAY.md)
under [ADR 0075](../decisions/0075-fixed-e01-project-replay.md). Full SN-021 pending.

The new explicit native operation binds original configured policy and a physically
captured schedule to the existing ideal RC compiler. Both requested duration and
quantum are 5 ms, debugging is disabled, and the one known initial drive is held at
3.3 V. Original declaration, graph, node/element source maps and resource snapshots
are retained, with policy/reference offsets and schedule resource index. It returns
an inert artifact; neither project opening nor compilation starts an engine.

Eight regression cases (seven Windows passes, one unsupported-platform skip) and
all 50 Windows CTests passed. They cover policy-before-I/O, explicit request,
metadata, unchanged standalone rejection, byte-identical netlist versus the old
operation, ownership after source replacement, altered but correctly hashed schedules,
missing files, changed model values and full schedule identity. Extra rows, altered
values, empty data, CRLF, BOM and directives reject. Non-Windows coverage checks
pre-I/O policy and unsupported-platform rejection; no portable filesystem claim.

## Real engine result and retained negative

The explicit Python harness verified the nine pinned ngspice assets, compiled a
fixture-derived configured project and replaced copied model and schedule files.
Recompilation rejected at physical size verification. The owned netlist remained
usable and matched the existing E-01 artifact. The staging helper verified bytes
through retained file/ancestor handles and used the directory's volume-GUID spelling;
those handles remained alive through the unchanged E-01 host's consumption/exit.
Netlist writes and directory rename were denied. The buffered write exception did
not expose a Win32 code (null); rename reported error 5. Do not invent error 32.

Attempt 01 failed: the new harness supplied the protected stage as the output
argument instead of the fixture argument. The unchanged host reported `Missing
owned netlist` and exited 1. The initial suspicion about GUID paths was not proven.
Only the harness argument positions were corrected; attempt 02 used a fresh output
directory and passed. Both full reports and hashes are retained in the
[audit](evidence/SN-021-fixed-replay-summary.json); historical evidence is unchanged.

| Observation | Attempt 02 |
|---|---|
| Samples | 5012; finite monotonic output |
| Maximum analytical error | 9.889724283951296e-08 V, below unchanged project 10 microvolt bound |
| Final virtual time | 0.004999999999999989 s, within unchanged project 1 ps bound of 5 ms |
| Callbacks | Match retained vectors exactly |
| Lifecycle | No callback fault; idle before quit; normal exit 0 |
| Resource substitution | Original model/schedule replaced after capture; owned artifact consumed |

E-01's original 0.0165 V assertion remains, with the stricter project bound checked
separately. No tolerance is relaxed. No Renode, firmware, mixed-signal coupling,
paused session or debugging behavior is claimed by this analog-only run.

```sh
cmake -S . -B build/sn021-save
cmake --build build/sn021-save --config Debug
ctest --test-dir build/sn021-save -C Debug --output-on-failure
python tests/experiments/ngspice/fixed_replay_acceptance.py --output build/sn021-replay-new-run
python tools/check_repository.py
git diff --check
```

The engine command requires existing pinned local runtime and E-01/native probes;
it downloads nothing and requires a fresh output directory. Raw logs/traces and
binaries remain ignored. Historical source/engine results are not relabelled.

## Remaining boundary

Configured runtime correspondence is now measured for this fixed analog-only
replay. Next compose configured acquisition/edit/save-copy/reopen with this same
operation, retaining source/policy/schedule provenance. Safe overwrite still needs
ADR 0064's identity/version protocol and failure evidence. Mixed-signal/runtime
modes beyond the fixed reference remain unsupported, not silently accepted by
label or hash. SN-021 stays in progress; final acceptance and the next-cycle prompt
must wait. Preserve SN-017 Python/GDB control, SN-044, instrumentation, MCU/toolchain
independence, historical negatives, twelve local SN-045 files and PDF/PID.
