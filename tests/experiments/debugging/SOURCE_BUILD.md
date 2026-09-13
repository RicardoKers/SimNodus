# Reproducing the experimental Renode managed build

This recipe builds the managed code with the pinned release's native CPU
translators. It is not a full native rebuild or a replacement distribution.
All output is local to `build/sn016/`; the validated `build/deps/` package is
read-only input. Exact revisions, SDK SHA-512 and native SHA-256 values are in
[the evidence manifest](../../../docs/experiments/evidence/E-05-cooperative-cancellation-summary.json).

## Inputs and configuration

1. Clone Renode tag `v1.16.1` into `build/sn016/renode-cancel-src`, verifying HEAD
   `d66b0c2aa3d420408eccecfd1d3bab0fd702a6db`. Initialize its pinned recursive
   submodules under `src/Infrastructure` and `lib/`. The obsolete `lib/cctask`
   entry in `.gitmodules` has no gitlink in this tag and is not a build input.
   Infrastructure must be `add012af003a0f620d3da52828262676f374d121`.
2. Clone `https://github.com/renode/renode-resources.git` into `lib/resources` and
   select revision `14b80cde0a136b684f316eb7f6a31aeaae0684bf`. The upstream build
   script normally fetches a moving branch; retain this recorded revision.
3. Extract the verified Windows x64 .NET SDK 8.0.424 ZIP into
   `build/sn016/dotnet-sdk`. No global SDK install is needed. The app uses the
   installed .NETCore/WindowsDesktop 8.0.22 runtime. The official portable binary
   uses 8.0.10, so source-build equivalence is measured, not assumed.
4. From a prior verified portable Renode extraction, load `Infrastructure.dll`
   using .NET reflection and enumerate resources beginning with
   `Antmicro.Renode.translate-` and ending with `.so`. Copy each stream under
   its filename without the `Antmicro.Renode.` prefix to
   `src/Infrastructure/src/Emulator/Cores/bin/Release/lib/`. Verify every copied
   resource against the manifest's `native_translators_reused` hashes. These are
   Windows native libraries despite the upstream `.so` resource names.
5. Copy `src/Infrastructure/src/Emulator/Cores/windows-properties.csproj` into
   `output/properties.csproj`. At the clone root, write `Directory.Build.targets`:

```xml
<Project>
  <PropertyGroup>
    <TargetFrameworks>net8.0-windows10.0.17763.0</TargetFrameworks>
    <CsWinRTAotOptimizerEnabled>false</CsWinRTAotOptimizerEnabled>
  </PropertyGroup>
</Project>
```

The upstream Windows source still references WPF in the nominal no-GUI build;
that attempted configuration failed. Use the full Windows target above and
continue to launch experiments with the existing headless runtime arguments.

## Reference, patch and checks

From the SimNodus workspace, configure local caches and compile:

```powershell
$env:DOTNET_CLI_HOME = (Resolve-Path build/sn016/dotnet-sdk).Path
$env:DOTNET_CLI_TELEMETRY_OPTOUT = '1'
$env:NUGET_PACKAGES = Join-Path $env:DOTNET_CLI_HOME 'packages'
& build/sn016/dotnet-sdk/dotnet.exe build build/sn016/renode-cancel-src/src/Renode/Renode_NET.csproj -c Release -p:NET=true -v:minimal
```

Preserve the original successful managed output as the reference. Prepare a
separate `source-reference` directory by copying the verified portable package
and overlaying `output/bin/Release`. Rename only that copy's `hostfxr.dll` to
`hostfxr.dll.portable-reference`: the new executable is framework-dependent and
must resolve the installed runtime, rather than the old portable host. Keep the
original package intact. Run the existing `run.normal_case` with this directory
as `args.renode`, all existing firmware/circuit/GDB inputs and firmware
`build.json` as `args.build`. Preserve the returned JSON and logs.

Apply [renode-cancellation.patch](renode-cancellation.patch) from the
`src/Infrastructure` checkout root using `git apply`, rebuild with the identical
command, and overlay a separate `source-cancellation` copy of the reference.
The only source changes should be the three time/emulation files in that patch.
Run `normal_case` against the modified package and compare its complete result
to the unmodified source-build result. Then run the dedicated matrix:

```powershell
python tests/experiments/debugging/cancel_probe.py --renode build/sn016/source-cancellation --ide C:/path/to/STM32CubeIDE --output build/sn016/cancel-new
```

The recorded reference and modified normal-case JSON objects are exactly equal,
including firmware ADC code 2048 and final real-ngspice checkpoint at 4,027,000 ns.
Both builds succeeded with upstream warnings and zero errors. The cancellation
matrix's three final repetitions use the same modified assemblies and bridge.
Runtime-extracted input paths and generated trees remain ignored build artifacts;
no SDK, dependency, DLL or source checkout is added to the project distribution.
