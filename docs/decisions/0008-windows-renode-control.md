# ADR 0008: Bound native Windows Renode control and server exposure

Date: 2026-08-31. Status: accepted for the SN-019 experiment profile; firmware and coupled control pending.

## Context

[SN-019](../experiments/SN-019-results.md) found that the selected C client needs Windows transport/compiler adaptation, while the matching server binds every IPv4 interface. A local client address alone does not constrain server exposure. The experiment then reproduced actual handshake/time advancement, reconnection, fragmented transfers, and bounded fault handling.

## Decision

- Generate the adapted client and loopback server variant from verified pinned sources under ignored build output. Preserve upstream protocol, microsecond units, source hashes, and MIT notices. Do not patch installed Renode files or machine-wide network settings.
- Compile/load the server extension only through an explicit trusted experiment action. Verify its real loopback endpoint and process ownership before connecting; require normal shutdown and listener removal.
- Keep native client operations serialized with one fixed wall-time deadline across the whole operation. Retire failed transport/protocol streams; reconnect explicitly. Recoverable application diagnostics have independent ownership.
- Treat exact empty-machine time advancement as bounded evidence only. Preserve the separation between client disconnect/timeout, actual Renode execution state, and the future joint committed state.

## Consequences and revisit criteria

SN-012 can now test firmware through native Windows control. It must still validate an offline C8 platform, GPIO/input behavior, and relevant peripheral capabilities. Production cancellation, concurrency, callback ownership, long-running calls, authentication/local-user trust, paths with whitespace, and packaging need further work. Do not turn the experiment's 1000 ms deadline or 1 MiB frame cap into an undocumented product limit.

Revisit after an upstream client/server change, a security requirement, or contradictory experiment evidence. An upstream contribution can be considered separately; none was submitted as part of this task.
