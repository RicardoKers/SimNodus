# SN-019: Native Windows Renode client results

Date: 2026-08-31. Task: [SN-019 / #11](https://github.com/RicardoKers/SimNodus/issues/11). **Passed for the bounded Windows transport/time profile below.** This is a prerequisite for E-02, not firmware or peripheral validation.

## Scope and reproduction

The [predeclared specification and commands](../../tests/experiments/renode-client/README.md) require native compilation, real Renode handshake/time advancement, loopback-only exposure, fragmented transfers, and bounded failures. [Compact evidence](evidence/SN-019-summary.json) records two complete local runs, source/build hashes, expected/actual times, and fault results. Raw logs, generated sources, executables, and dependencies remain in ignored build output.

Environment: Windows 11 x64 10.0.26200; MSVC 19.51.36246.0, Windows SDK 10.0.26100.0, CMake 4.2.3-msvc3, Python 3.14.4. The client uses C11 with `/W3 /WX`; the host uses C++20 with `/W4 /WX /permissive-`; both are Debug/x64. Renode is the unchanged 1.16.1.19220 portable package, build `d66b0c2a-202602161036`, bundled .NET 8.0.10. No additional .NET SDK is needed: Renode compiles the explicitly included C# extension itself.

[The baseline manifest](../../tools/backend-baseline.json) verifies the release archive and selected executable/client/header/platform files. [The additional source manifest](../../tests/experiments/renode-client/upstream.json) pins 13 small official source/license files. Generated client/header/server hashes are recorded separately from upstream hashes. The local executable hashes identify these builds, not bit-for-bit portability across compilers or paths.

The separate [Windows workflow](../../.github/workflows/renode-client.yml) obtains and verifies the pinned files, builds both clients, and runs the complete suite twice. Hosted executions are listed in [workflow runs](https://github.com/RicardoKers/SimNodus/actions/workflows/renode-client.yml); committed evidence here records local execution.

## Why a loopback server variant was necessary

The pinned `ExternalControlServer` delegates socket creation to `SocketServerProvider.Start`, which supplies `IPAddress.Any`; `SocketsManager` binds that endpoint without narrowing it. Connecting from localhost does not restrict such a server to localhost. **The original all-interface listener was not started for this experiment.**

The generated extension retains the selected release's command handlers and framing in a separate namespace. Its provider changes exactly one bind address to `IPAddress.Loopback`, and its Monitor entry point is `CreateLoopbackControlServer`. No installed Renode file or Windows firewall rule changes. Before any client connects, the runner reads Windows' actual TCP listener table and requires `127.0.0.1`, the chosen ephemeral port, and the owned Renode process. Normal shutdown is required, followed by an empty listener table for that process.

This is an explicitly loaded experiment extension, not an upstream Renode fix or a claim that localhost authenticates local users. It must never be loaded merely by opening an untrusted circuit.

## Real-backend results

Both local runs passed all **20 cases**: one real-backend case containing six connections and 36 RunFor requests, plus 19 separate fault/input cases. The normal client and the build restricted to one-byte socket transfers each completed three connections to the same real Renode process. The machine was empty: no CPU, platform file, firmware, peripheral model, or analog engine was loaded.

Every connection ran this sequence; actual differences matched exactly in integer microseconds:

| Requested operation | Observed elapsed virtual time |
|---|---:|
| 0 us | 0 us |
| 1 us | 1 us |
| 999 us | 999 us |
| 1000 us | 1000 us |
| 1 ms | 1000 us |
| 1 s | 1000000 us |

Each connection advanced 1,003,000 us. Time began at zero and reached 6,018,000 us after both client variants. Queries did not advance time; reconnecting preserved it. The complete real-backend result, including timestamps and cleanup assertions, repeated identically in the two runs. Machine lookup succeeded; a missing machine produced a recoverable diagnostic followed by a valid time query. Invalid time units and overflowing conversion were rejected without advancing emulation.

These are measurements of an empty machine, not guaranteed GPIO stop boundaries, CPU timing accuracy, future-event prediction, or coupled causality.

## Fault and ownership results

Scripted peers use real loopback sockets but do not simulate Renode. They test failure handling only and are never counted as engine integration evidence.

| Cases | Observed behavior |
|---|---|
| Unavailable port, closed handshake, invalid handshake | Failure returned; no retained client handle |
| Stalled handshake, stalled response, slowly delivered response | Explicit timeout around the declared 1000 ms deadline; the slow stream could not renew the deadline byte by byte |
| Truncated response | Connection failure; a later command returned `ERR_NOT_CONNECTED` |
| Invalid return code, mismatched command, oversized error/event, fatal error, invalid event during RunFor | Fatal/protocol failure; stream retired and reuse rejected |
| Short and empty command-error messages | Owned diagnostic freed safely; the next query succeeded with the peer's declared 123 us value |
| 21 rejected handshakes in one process | After warmup, 20 further failures changed the measured handle count from 63 to 64 in one run and 63 to 63 in the other, within the test's two-handle allowance |
| Port zero, port 65536, nonnumeric port | Rejected before connecting; no retained handle |

The unavailable-port case reached the client's deadline first on this Windows host. A separate OS socket query reported WSAECONNREFUSED (10061) after about 2.035 s. The client does not wait past its shorter deadline to obtain that later error. No timeout, process kill, or missing result is classified as successful simulation.

The one-byte build exercises real send/receive loops and protocol assembly deterministically. It does not establish naturally occurring partial-send frequency, send-buffer backpressure behavior, or arbitrary network performance.

## Compatibility changes and findings

- Replace POSIX descriptors/I/O with native Winsock socket ownership, numeric IPv4 loopback, nonblocking connect/send/receive, and a shared per-operation deadline. Advance the buffer after each successful partial transfer; the original write loop did not.
- Replace GNU packed/cleanup constructs with equivalent MSVC packing and explicit frees on early returns. Keep the upstream public header and wire units; bound size conversions before allocation/casting.
- Own error strings independently of request buffers, including short/empty diagnostics. The upstream receive path could label the caller's stack or request buffer as dynamically owned. Cap error/event allocations at 1 MiB for this experiment.
- Retire failed streams and distinguish fatal errors from recoverable command errors. Validate duration units before division/multiplication; initialize the time-query payload instead of transmitting uninitialized output storage.
- Use absolute configuration paths, unquoted `include @path` syntax without whitespace in the checkout path, and fresh process-local temporary directories. Initial setup attempts exposed unsupported path quoting and Renode temporary cleanup trying to inspect an inaccessible stale process; neither was treated as an integration pass.

Changes are generated from hash-verified upstream sources by the reviewed preparation script. [Upstream MIT terms](../../tests/experiments/renode-client/UPSTREAM-LICENSE.txt), Antmicro notices, and the provider's Realtime Embedded notice are retained. Original SimNodus additions are MIT. Complete binary/runtime redistribution approval remains separate.

## Limits and next step

No STM32 firmware, GPIO/ADC/system-bus behavior, callback reentrancy, concurrent-client semantics, debugger, analog coupling, Linux execution, send-buffer saturation, forced Renode crash recovery, or long-duration leak test is validated. The finite failure loop is a limited handle check, not proof of leak freedom. Other inherited peripheral APIs compile but remain unvalidated. Runtime/compiler exhaustion and allocation failure are not comprehensively tested.

Closing a failed socket does **not** prove that an already accepted Renode RunFor stopped. The selected server runs advancement on its own thread. The future session coordinator must reconcile actual engine state after timeout/disconnect; it must not report a globally paused or rolled-back circuit from client failure alone.

[ADR 0008](../decisions/0008-windows-renode-control.md) retains these restrictions. SN-019 completes the Windows control prerequisite. Next: [SN-012 / E-02](https://github.com/RicardoKers/SimNodus/issues/3), with owned firmware, an offline C8 profile, GPIO/input evidence, and a peripheral audit. The joint temporal profile and coupled experiments remain pending.

## Primary implementation references

- [Pinned external-control server](https://github.com/renode/renode/blob/d66b0c2aa3d420408eccecfd1d3bab0fd702a6db/src/Renode/Network/ExternalControl/ExternalControlServer.cs), [RunFor](https://github.com/renode/renode/blob/d66b0c2aa3d420408eccecfd1d3bab0fd702a6db/src/Renode/Network/ExternalControl/RunFor.cs), and [GetTime](https://github.com/renode/renode/blob/d66b0c2aa3d420408eccecfd1d3bab0fd702a6db/src/Renode/Network/ExternalControl/GetTime.cs).
- [Pinned socket provider](https://github.com/renode/renode-infrastructure/blob/add012af003a0f620d3da52828262676f374d121/src/Emulator/Main/Utilities/SocketServerProvider.cs) and [socket manager](https://github.com/renode/renode-infrastructure/blob/add012af003a0f620d3da52828262676f374d121/src/Emulator/Main/Sockets/SocketsManager.cs).
- Microsoft [nonblocking connect](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-connect), [select](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-select), and [TCP endpoint/PID table](https://learn.microsoft.com/en-us/windows/win32/api/iphlpapi/nf-iphlpapi-getextendedtcptable).
