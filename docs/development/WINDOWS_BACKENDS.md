# Windows backend experiment setup

This reproduces **SN-010 dependency checks**, not E-01/E-02. Read the [inventory](../research/DEPENDENCIES.md) and [results](../experiments/SN-010-results.md) first. Use a trusted local checkout, Python 3.10+, CMake, a native x64 C++ compiler, and 7-Zip. The tested compiler/generator is Visual Studio 18 2026 Build Tools; other compiler versions are not established by this report.

## Download explicitly and verify before extraction

Run from the repository root in PowerShell. These commands download about 149 MB into ignored build output; they do not install services, change PATH, or configure the operating system. Review the manifest URLs before running.

```powershell
$ErrorActionPreference = 'Stop'
$manifest = Get-Content tools/backend-baseline.json -Raw | ConvertFrom-Json
New-Item -ItemType Directory -Force build/deps/downloads | Out-Null
foreach ($asset in $manifest.archives) {
    $destination = Join-Path build/deps $asset.path
    curl.exe --fail --location --silent --show-error --max-time 180 $asset.url --output $destination
    if ($LASTEXITCODE -ne 0) { throw "Download failed: $($asset.path)" }
    if ((Get-FileHash $destination -Algorithm SHA256).Hash.ToLowerInvariant() -ne $asset.sha256) {
        throw "Checksum mismatch: $($asset.path). Do not extract or run this file."
    }
}
python tools/check_backend_assets.py --archives-only
if ($LASTEXITCODE -ne 0) { throw 'Archive verification failed' }
```

An HTTP success response can contain an HTML download page. This occurred with a generic SourceForge request; the pinned mirror URLs worked with `curl.exe`. Never accept a mismatched hash or replace it merely to make a download pass. If a mirror fails, obtain the same named artifact from its official release page and retain the expected hash.

## Extract locally

Use empty destination directories for a fresh reproduction. Do not overlay a newer package onto this baseline. Adjust the 7-Zip executable path if necessary.

```powershell
$sevenZip = Join-Path $env:ProgramFiles '7-Zip/7z.exe'
& $sevenZip x build/deps/downloads/renode-1.16.1.windows-portable-dotnet.zip '-obuild/deps/renode' -y
if ($LASTEXITCODE -ne 0) { throw 'Renode extraction failed' }
& $sevenZip x build/deps/downloads/ngspice-47_dll_64.7z '-obuild/deps/ngspice' -y
if ($LASTEXITCODE -ne 0) { throw 'ngspice DLL extraction failed' }
& $sevenZip x build/deps/downloads/ngspice-47_64.7z '-obuild/deps/ngspice-console' -y
if ($LASTEXITCODE -ne 0) { throw 'ngspice console extraction failed' }
New-Item -ItemType Directory -Force build/deps/ngspice-source | Out-Null
tar -xf build/deps/downloads/ngspice-47.tar.gz -C build/deps/ngspice-source
if ($LASTEXITCODE -ne 0) { throw 'ngspice source extraction failed' }
python tools/check_backend_assets.py
if ($LASTEXITCODE -ne 0) { throw 'Extracted-file verification failed' }
```

Expected: 14 checked files, zero failures. The checker neither loads binaries nor downloads anything. It checks the listed files, not every extracted transitive dependency; keep the full packages unmodified. `--root` supports an alternative dependency directory with the same relative layout.

## Run the C++ ngspice probe

The [standalone probe](../../tools/backend_probe/ngspice_probe.cpp) is intentionally separate from the root foundation build. It includes the verified upstream header without copying it into the repository and dynamically loads the selected DLL.

```powershell
$spiceRoot = (Resolve-Path build/deps/ngspice/Spice64_dll).Path
cmake -S tools/backend_probe -B build/backend-probe -G 'Visual Studio 18 2026' -A x64 "-DNGSPICE_ROOT=$spiceRoot"
if ($LASTEXITCODE -ne 0) { throw 'Probe configuration failed' }
cmake --build build/backend-probe --config Debug
if ($LASTEXITCODE -ne 0) { throw 'Probe build failed' }
& build/backend-probe/Debug/ngspice-probe.exe `
    build/deps/ngspice/Spice64_dll/dll-vs/ngspice.dll `
    build/deps/ngspice-console/Spice64/bin `
    tools/backend_probe/initialization
if ($LASTEXITCODE -ne 0) { throw 'Probe failed' }
```

Expected: version 47, idle state, controlled exit callback status 0, and a `PASS` line. The `quit` command itself returns **1** in this version; the probe also checks its exit callback and does not interpret that return alone as success. No numerical tolerances apply because no circuit is simulated.

The third argument must contain the exact owned one-line `spinit`. Before loading the DLL, the probe sets `SPICE_SCRIPTS` for its own process. The script sets `no_spiceinit`, preventing the user's configuration search. The probe verifies the emitted confirmation. It loads no optional code models. Do not replace this with the crashing pre-init call or a user's `spinit`.

For failure checks, pass a missing DLL, an existing empty directory instead of the audio directory, or a directory with a different `spinit`; each must exit nonzero. A harmless `.spiceinit` containing `echo UNEXPECTED_USER_INIT_LOADED` in the working directory must not appear in the normal probe output. Run native probes in a child process with a timeout when automating them; the DLL can crash its host.

## Check Renode startup

```powershell
$renode = 'build/deps/renode/renode_1.16.1-dotnet_portable/Renode.exe'
& $renode --version
if ($LASTEXITCODE -ne 0) { throw 'Renode version check failed' }
& $renode --disable-xwt --console --plain --execute 'quit'
if ($LASTEXITCODE -ne 0) { throw 'Renode startup check failed' }
```

Expected: `v1.16.1.19220`, build `d66b0c2a-202602161036`, bundled .NET `8.0.10`, then a successful headless quit. Renode creates its normal per-user configuration directory; a restricted runner may need permission for that write. This check opens no control/debugger port and loads no firmware or platform.

## Reproduce the known native client build failure

The client source is already in the Renode package. This is a **diagnostic command expected to fail**, not an installation step that has succeeded:

```powershell
cmake -S build/deps/renode/renode_1.16.1-dotnet_portable/tools/external_control_client/lib `
    -B build/sn010-renode-client -G 'Visual Studio 18 2026' -A x64 `
    '-DCMAKE_POLICY_VERSION_MINIMUM=3.5'
cmake --build build/sn010-renode-client --config Debug
```

CMake 4 needs the policy minimum override for this old upstream project. With it, configuration passes; the MSVC build fails with D8021 for `/Werror`. Fixing flags alone is insufficient: source also includes POSIX sockets/`unistd.h` and GCC-specific constructs. [SN-019 / #11](https://github.com/RicardoKers/SimNodus/issues/11) owns the bounded Windows adaptation and real loopback/time test. No alternative protocol is assumed here.

## Firmware tools and next experiments

Use the `arm-none-eabi-gcc.exe` and `arm-none-eabi-gdb.exe` bundled in the selected CubeIDE installation, under its GNU-tools plugin's `tools/bin` directory. Run each with `--version` and compare with the inventory. Keep the installation path in local configuration, not committed files. Do not download an additional ARM toolchain just because these executables are absent from PATH.

E-01 has passed its bounded RC/lifecycle profile; see [results](../experiments/E-01-results.md) and [reproduction commands](../../tests/experiments/ngspice/README.md). E-02 may prepare owned firmware and an offline C8 platform, but its external-control acceptance requires SN-019. Qt is unnecessary for both experiments. No upstream SVD/model URL should be downloaded automatically when opening a user project.
