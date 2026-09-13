# ADR 0020: complete native debug reply ingress

Date: 2026-09-10. Status: accepted for the Windows SN-017 fixture slice.

## Decision

Add opt-in `--native-debug-results` to the existing native command path.
`sample-ingress` and `notify-ingress` arm a fresh reply reader before command
emission; `poll-debug` consumes the reply. Legacy commands remain available for
differential controls. Host `observe` or `notified` cannot bypass a pending native
reply. Progress updates observation only; notification clears its pending gate
without committing. The host still checks actual GDB SIGINT/time and stable
inspection before the joint commit.

The owned bridge writes each reply once using one file handle. On Windows,
exclusive `CreateFileW` opening fences closure of that writer. Sharing/access/lock
conflicts remain pending under the original deadline. A partially written numeric
prefix is never accepted while the writer is open. Closed empty, malformed or
oversized files fail; this does not support writers that close and reopen between
chunks. No bridge source or backend capability was changed. The closure fence
fails closed outside Windows; Linux ingress is not validated.

Replies are bounded to 4096 bytes. Progress is an exact unsigned integer checked
against the native grant and monotonic observation. Notification must be exactly
`notified`; `rejected: ` is a backend error. Existing reply/error files are stale;
subsequent error sidecars fail. Duplicate consumption, pending rearm and host
attestation bypass fail without a new joint commit.

All progress samples in one grant share 1900 ms from the first native request.
Notification reuses the cancellation command's existing 1900 ms deadline; analog
catch-up cannot renew it. Expiry is checked before and after reads/parsing. The
host retains the complete 2000 ms MI acknowledgement check. A GDB notification
already delivered cannot be withdrawn after a later read/inspection failure.

## Evidence and limits

See the [protocol](../../tests/headless/DEBUG_INGRESS.md) and
[evidence](../experiments/evidence/SN-017-debug-ingress-summary.json).
Real Renode/ngspice/GDB integration and host-read differential controls passed.
Windows pipe/file tests exercise actual partial writers and sharing locks;
engine-loss controls occur at fixed checkpoints, not during notification ingress.
Fresh path preflight is not an atomic reservation against hostile writers. Only
owned disposable fixture directories are supported. SN-017 remains in progress;
fixture preparation, GDB relay, inspection and orchestration remain in Python.
ADR 0014 restrictions, all prior evidence and PDF suppression are preserved.
