# ADR 0026: native bounded ADC helper invocation and result ingress

Date: 2026-09-11. Status: accepted for the Windows SN-017 fixture slice.

## Decision

Add opt-in `--native-adc-process`. Explicit startup configuration supplies the
verified ADC helper executable and the owned Renode control port. `apply-adc`
accepts no command, channel or voltage arguments: it launches only the prepared
channel-0 microvolt input. The host no longer invokes the helper or supplies an
`adc-applied` attestation on this path; such attestations are rejected.

The adapter starts the helper suspended, assigns a non-inherited kill-on-close
Job Object, and then resumes it. Only null stdin and captured stdout/stderr are
inherited. Paths are absolute and Windows argv quoting is applied without a shell.
This remains an explicit trusted-fixture launcher, not a project-file executable
interface. The existing measured helper binary is unchanged.

`poll-adc` drains available bytes without waiting for unavailable pipe data.
Accept at most 4096 bytes per stream; retain at most one additional detection
byte when oversized output fails. Hex diagnostics preserve exact captured bytes.
Confirmation requires root exit zero, empty stderr, both streams closed, the exact
owned ADC result frame with unchanged 4010 us timestamps, and confirmed cleanup
of the whole job. A completed stdout frame alone never confirms application.
Remaining descendants are terminated after root exit before success is returned.

The original 1900 ms preparation deadline governs launch and result acceptance;
it is checked before creation, before resume, during polling and after result
validation. No launch or poll renews it. Failure/timeout kills the job. Failure
cleanup has a separate maximum five-second wait, which cannot turn a late result
into success. Runner death also closes the job. A completed ADC write cannot be
rolled back if reading, cleanup or later checks fail; no joint commit follows.

## Evidence and remaining scope

See the [protocol](../../tests/headless/ADC_PROCESS.md) and
[evidence](../experiments/evidence/SN-017-adc-process-summary.json).
Actual process tests cover malformed/fragmented/oversized output, stderr, exit
failure, hanging roots, descendants, runner loss and shared deadline. Real
Renode/ngspice/GDB runs validate the unchanged helper and compare against the
host-invoked path. The initial passing run predates tighter capture/resume checks;
its original report remains preserved.

The helper still reports microsecond time without channel/value readback. Exact
boundary evidence remains in native CPU/analog state and actual GDB checks;
general ADC compatibility is not inferred. Host analytical validation and final
GDB/commit scheduling remain pending extraction. Next move bounded analytical RC
checks into native coordination. SN-017 remains in progress; capability limits,
PID retry, PDF suppression and all evidence remain unchanged. No commit or
publication is authorized.
