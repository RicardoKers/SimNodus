# ADR 0075: Bind one configured project replay to the measured E-01 profile

Date: 2026-09-22. Status: accepted for fixed single-interval analog-only E-01.

Use a separate explicit `compile_fixed_rc_replay` operation under the
[contract](../architecture/FIXED_RC_REPLAY.md). Keep existing standalone APIs'
configured-policy rejection. Reuse the same physical capture, R/C interface,
exact numerical binding and source-preserving compiler; never rewrite the
project policy to unconfigured in order to pass the old gate.

Bind one 5 ms replay interval, disabled debugging and the exact owned fixed-drive
schedule bytes to the existing ideal RC analysis. Duration and exchange quantum
are both 5 ms; internal solver step stays 1 us. This defines a narrow project
input binding, not a general CSV parser or a new engine/circuit fidelity profile.
Other schedules, quanta, duration, sampled mode, targets and debug requests reject.

Keep the complete original declaration and snapshot provenance, plus schedule
index and policy/reference offsets. All readiness flags remain false. An inert
compilation request grants no engine/library, origin or redistribution authority.
Run the separately invoked real-engine harness against the owned artifact, pinning
its stage through consumption. Preserve both the initial harness argument failure
and corrected positive run in the [report](../experiments/SN-021-fixed-replay.md).

The narrower E-01 binding avoids pretending that generic project fields match
SN-017's cooperative grant/stop sequence. Its preparation, GDB transport and Python
fixture control remain intact. Preserve SN-044, shared instrumentation, MCU/toolchain
independence, all tolerances, PDF suppression and PID retry. Safe overwrite and
full lifecycle acceptance remain pending; do not expand profiles or UI implicitly.
