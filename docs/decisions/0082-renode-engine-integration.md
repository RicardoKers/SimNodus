# ADR 0082: Renode engine integration

Date: 2026-10-14. Status: accepted.

## Context

Phase 2 (release 0.2) runs real STM32 firmware next to the analog circuit.
Experiments SN-019, E-02 and E-03 drove the pinned Renode 1.16.1 from a native
client over its external control protocol. The product needs that path as a
library (`src/mcu`, SN-130) on Windows and Linux, without the experiment
runners. The facts to respect are:

- Upstream `CreateExternalControlServer` listens on `IPAddress.Any`, which
  exposes the emulator to the network and triggers a firewall prompt.
- The upstream C client is POSIX-only, has a process-wide callback table that is
  never cleared, and the experiment transport had a fixed 1 s deadline that
  would also cut long `RunFor` requests.
- The monitor's `include @file` cannot quote paths, so paths with spaces fail.
- Renode takes about 3.5–6 s to start (warm), most of it .NET start-up.
- Renode's own temporary-file cleanup misbehaves with a shared `TEMP` (SN-019).
- The server sends GPIO events only while a `RunFor` is running.

## Decision

- **One Renode process per run**, started by `RenodeEngine::start()` and never
  by opening a project. On Windows the process runs in a Job Object with
  kill-on-close; on Linux it gets `SIGKILL` when the parent dies. Stop sends
  `quit` on stdin and kills after a timeout.
- **Loopback-only server**: `resources/renode/LoopbackControl.cs` is the
  experiment's generated server (pinned upstream sources, renamed namespace,
  one `IPAddress.Any` → `IPAddress.Loopback` change, MIT notices kept). Renode
  compiles it at start-up with `include`; no .NET SDK is needed.
- **Run folder**: every run gets `%TEMP%/SimNodus/renode-<id>/` (on Linux
  `$TMPDIR`) with copies of the server, the platform description and the
  firmware, named relative to it. The folder doubles as Renode's `TEMP`, its
  config location and the place of `renode.log`. A clean exit removes it;
  failed runs keep it for diagnosis and are pruned after a day.
- **Vendored client**: `third_party/renode_external_control` holds the
  experiment's adapted client with two more changes: the callback table belongs
  to the connection, and the per-operation deadline can be set (`RunFor` gets
  10 s plus 100 ms per virtual millisecond). The transport is rewritten for
  Winsock and POSIX sockets.
- **Readiness**: the client connects only after the log reports the listener
  on the chosen port, then completes the protocol handshake. Errors in the log
  ("error executing command", "Fatal error", ...) or an early exit fail the
  start with the last log lines.
- **Board knowledge is data**: platform description and board-specific
  monitor commands (RCC seeding, vector table) are inputs of the engine, set by
  the board profile (SN-131), not fixed in the engine.

## Alternatives

- *Renode's own server on all interfaces*: rejected (network exposure).
- *Monitor commands over a telnet port or stdin only*: no binary GPIO events,
  text parsing of responses; rejected.
- *One long-lived Renode for the whole session*: faster runs, but firmware
  reload, machine reset and crash recovery become harder; reconsider if the
  start-up time bothers users.
- *Generating the client and server at build time from downloaded sources*:
  builds would need network access; rejected. The generated files are
  vendored with their provenance instead.

## Consequences

- Start-up costs 3.5–6 s per run; the application must start the engine
  asynchronously and show progress (SN-131/132).
- GPIO event timestamps have the granularity of Renode's master clock
  (one quantum); changes made with bus writes outside `RunFor` are not events.
- Another local process could take the chosen port between the check and
  Renode's bind; the start then fails (no wrong connection is accepted, as the
  listener line must name the port and the handshake must succeed). Retry is
  left to the caller.
- **Distribution is not decided here.** The Renode bundle contains components
  under other licenses (for example LGPL-2.1 tlib) and its own .NET runtime; a
  license and update review must precede shipping it in the Windows zip
  (packaging task of release 0.2). Until then developers fetch it with
  `tools/fetch_backends.py --renode`.
- Tests run against the real Renode on Windows and Linux in CI
  (`engine-renode`, required there with `SIMNODUS_REQUIRE_RENODE=ON`).

## Revisit criteria

Start-up time above about 10 s on classroom PCs, a need for several machines
per run, or a Renode upgrade that changes the external control protocol.
Related: MCU-02, SN-130..SN-133, [E-02](../experiments/E-02-results.md),
[E-03](../experiments/E-03-results.md),
[SN-019](../experiments/SN-019-results.md), ADR 0011 (sampled feedback).
