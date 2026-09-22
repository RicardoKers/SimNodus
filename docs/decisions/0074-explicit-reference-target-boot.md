# ADR 0074: Bind an inert project target to explicit reference boot evidence

Date: 2026-09-22. Status: accepted for the owned isolated SN-012 fixture.

Compose complete project validation, exact target/platform identities, physical
capture and the static boot candidate under the [reference target contract](../architecture/REFERENCE_TARGET.md).
Require explicit profile selection and the exact owned platform/image pins. This
is a narrow experimental input association, not a general hash-based trust policy.
Do not negotiate or override a configured co-simulation policy; reject it here.

Keep native results inert and readiness false. A separate explicit Python runner
may materialize the captured reference bytes, pin the staged files and ancestors
through real Renode consumption, and reuse unchanged E-02 acceptance. Original
package resources must not be reopened for execution. Record substitutions and
write/rename denial alongside real loader/boot observations.

The accepted staging helper is test-only. Its success does not provide a production
execution capability, safe overwrite or general executable project support.
MCU-specific policy remains outside the domain. Preserve SN-017 Python/GDB/fixture
ownership, ADRs 0027/0028/0043, SN-044, shared instrumentation and all accepted
electrical/temporal tolerances, PDF suppression and PID retry behavior.

SN-021 remains in progress. Review the remaining acceptance matrix, especially
configured project temporal/runtime correspondence and overwrite ownership, before
choosing another implementation slice. Do not infer full completion from two engine
runs of one reference image. No UI, issue synchronization or binary release.
