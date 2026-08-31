# SN-019: Native Windows Renode control prerequisite

Predeclared on 2026-08-31, before executing the native client. This is a transport/time experiment, not E-02 firmware or peripheral validation.

## Acceptance criteria

- Use the unchanged, verified Renode 1.16.1 portable executable and the matching external C client/header. Generate adapted sources under ignored build output; preserve upstream notices and hashes.
- Compile natively for Windows x64. Preserve the release protocol and microsecond time units. Do not replace Renode with a fake integration backend.
- The pinned server provider binds `IPAddress.Any`. Do not start that listener. An explicitly loaded experiment extension must retain the pinned command implementation and bind only `127.0.0.1`. Confirm the actual listening endpoint and process ownership before connecting.
- On a real empty Renode machine (no CPU or firmware), complete the handshake, query time, run for 0/1/999/1000 microseconds, 1 millisecond, and 1 second. Each reported difference must exactly match the requested microseconds. Time queries must not advance time. Repeat using a fresh connection without resetting the emulation.
- Validate machine lookup and a missing-machine diagnostic without corrupting the next command. Reject invalid units and overflowing duration conversion before sending.
- Exercise real connection refusal, peer closure, stalled handshake, stalled/truncated command response, malformed response, and repeated failed connection cleanup. Each client operation has a fixed 1000 ms wall-time deadline, shared across all bytes/events of that operation. Timeout tests must report a timeout, not successful completion; a 15-second child-process limit is a last-resort failure guard.
- Force one-byte send/receive transfers over real sockets and repeat the real Renode test. This is deterministic transfer fragmentation, not a claim that the operating system naturally returned a short send in every run. Use scripted loopback peers only for fault injection, clearly separate from real Renode evidence.
- After transport/protocol failure, close the affected connection and reject reuse; reconnect explicitly. Disconnect must clear the owned handle. Do not claim that disconnecting cancels an in-flight Renode run or restores MCU/circuit consistency.
- Store versions, source/build hashes, commands, requested/observed virtual times, elapsed wall times, failure outcomes, and limitations. Do not generalize to GPIO, ADC, callbacks, GDB, Linux, or coupled causality.

Only trusted experiment sources/scripts are loaded explicitly. Opening a user circuit must never compile or load this extension automatically. Do not change firewall rules, machine-wide DLL paths, or the installed Renode package.

## Observed setup details

- Use a checkout path without whitespace for this experiment's Monitor `include @path` command. Quoting the path after `@` is not supported by the tested syntax.
- Renode receives absolute paths, an experiment-local configuration, and fresh process-local `TEMP`/`TMP` directories. Its temporary-file cleanup otherwise attempted to inspect an inaccessible stale process on the development host.
- An unavailable Windows loopback port may reach the client's 1000 ms deadline before Windows returns connection refused. An independent native-socket query reported WSAECONNREFUSED (10061) after approximately 2.035 s locally. Either a connection failure or the earlier explicit timeout is correct; successful connection is never accepted. Post-handshake closure separately exercises connection-failure classification.

## Reproduce

First follow the [Windows package setup](../../../docs/development/WINDOWS_BACKENDS.md). Renode is the only engine needed here. Run from the repository root in PowerShell:

```powershell
python tools/check_backend_assets.py --renode-only
if ($LASTEXITCODE -ne 0) { throw 'Package verification failed' }
python tests/experiments/renode-client/prepare.py --download
if ($LASTEXITCODE -ne 0) { throw 'Source preparation failed' }
$prepared = (Resolve-Path build/sn019/generated).Path
cmake -S tests/experiments/renode-client -B build/sn019/native -A x64 "-DPREPARED_ROOT=$prepared"
if ($LASTEXITCODE -ne 0) { throw 'Native configuration failed' }
cmake --build build/sn019/native --config Debug
if ($LASTEXITCODE -ne 0) { throw 'Native build failed' }
python tests/experiments/renode-client/run.py --output build/sn019/results
if ($LASTEXITCODE -ne 0) { throw 'SN-019 failed; inspect its logs and summary' }
```

`prepare.py --download` explicitly obtains the small pinned C# sources and license from official repositories. Omit `--download` to reuse already verified sources offline. Neither CMake nor the runner downloads anything. The runner compiles the generated C# extension using Renode's bundled compiler; a separate .NET SDK is unnecessary. Generated client/server source, copied headers/notices, executables, configuration, temporary files, raw logs, and result JSON stay under ignored `build/`.

The normal and fragmented executables use the same adapted source. The latter limits each socket transfer to one byte, while still communicating with real Renode. The runner has one real-backend case (six connections / 36 RunFor requests) and 19 fault/input cases. `--only real` or `--only faults` selects a group. Use fresh output directories to preserve previous evidence; `--renode`, `--prepared`, and `--native` support alternative build/dependency locations while retaining the manifest's package directory layout.

## Provenance and supported scope

[upstream.json](upstream.json) pins the server sources, transport-provider source, socket-policy audit source, and license. The [package manifest](../../../tools/backend-baseline.json) pins the executable and original C client/header. [UPSTREAM-LICENSE](UPSTREAM-LICENSE.txt) preserves the upstream MIT terms; generated files retain Antmicro and Realtime Embedded copyright notices. Namespace changes, one loopback binding change, and local using directives isolate the server variant without rewriting its command protocol. The C client receives native transport, packing/cleanup compatibility, bounded frame/error handling, checked size conversions, and duration validation.

Original SimNodus experiment code is MIT. This does not approve distribution of the complete Renode binary/runtime bundle. GPIO/ADC/system-bus methods compile but their behavior is not validated here. Static callback ownership, arbitrary reentrancy, cancellation of in-flight emulation, concurrent clients, resource-exhaustion recovery, and long-duration leak behavior remain outside this experiment.
