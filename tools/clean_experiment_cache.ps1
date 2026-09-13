# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
<#
.SYNOPSIS
Preview or remove extracted Renode runtimes from stopped local experiments.
.DESCRIPTION
Only build/sn012, sn014, sn015, sn016, and sn019/**/runtime-*/.net/Renode
are eligible. Dependencies, evidence, firmware, and workspaces are retained.
No permissions are changed. Run without -Apply to preview.
#>
[CmdletBinding()]
param([switch]$Apply)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryPath = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$buildPath = Join-Path $repositoryPath 'build'
$scopeNames = @('sn012', 'sn014', 'sn015', 'sn016', 'sn019')
$candidates = [Collections.Generic.List[object]]::new()
$skipped = [Collections.Generic.List[object]]::new()
$results = [Collections.Generic.List[object]]::new()

function Assert-Stopped {
    $names = @('renode', 'stm32cubeide', 'stm32cubeidec',
        'arm-none-eabi-gdb', 'e05_control', 'e05_circuit')
    $active = @(Get-Process -Name $names -ErrorAction SilentlyContinue)
    if ($active.Count -gt 0) {
        throw 'Close Renode, CubeIDE, and experiment/debugger processes before cache maintenance.'
    }
}

function Assert-CachePath([string]$Path) {
    $resolved = (Resolve-Path -LiteralPath $Path).ProviderPath
    if (-not $resolved.StartsWith($buildPath + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "Cache path escapes this repository's build directory: $resolved"
    }
    $relative = $resolved.Substring($buildPath.Length + 1)
    if ($relative -notmatch '^sn(012|014|015|016|019)\\(.+\\)?runtime-[^\\]+\\\.net\\Renode$') {
        throw "Not an eligible extracted-runtime path: $relative"
    }
    $ancestor = Get-Item -LiteralPath $resolved -Force
    while ($null -ne $ancestor) {
        if (($ancestor.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Refusing a linked/reparse ancestor: $($ancestor.FullName)"
        }
        if ($ancestor.FullName -eq $repositoryPath) { break }
        $ancestor = $ancestor.Parent
    }
    $payloads = @(Get-ChildItem -LiteralPath $resolved -Force)
    if ($payloads.Count -eq 0) { throw "No extracted runtime payload: $relative" }
    foreach ($payload in $payloads) {
        if (-not $payload.PSIsContainer -or
            ($payload.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Unexpected runtime cache entry: $($payload.FullName)"
        }
        foreach ($marker in @('Renode.dll', 'Infrastructure.dll', 'Renode.runtimeconfig.json')) {
            if (-not (Test-Path -LiteralPath (Join-Path $payload.FullName $marker) -PathType Leaf)) {
                throw "Missing extracted-runtime marker $marker in $($payload.FullName)"
            }
        }
    }
    return $resolved
}

function Measure-SafeTree([string]$Path) {
    $pending = [Collections.Generic.Stack[string]]::new()
    $pending.Push($Path)
    [long]$bytes = 0
    [long]$files = 0
    while ($pending.Count -gt 0) {
        $directory = [IO.DirectoryInfo]::new($pending.Pop())
        foreach ($entry in $directory.EnumerateFileSystemInfos()) {
            if (($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Refusing a linked/reparse cache entry: $($entry.FullName)"
            }
            if ($entry -is [IO.DirectoryInfo]) {
                $pending.Push($entry.FullName)
            } else {
                $bytes += $entry.Length
                $files++
            }
        }
    }
    return [pscustomobject]@{ bytes = $bytes; files = $files }
}

Assert-Stopped
foreach ($scopeName in $scopeNames) {
    $scopePath = Join-Path $buildPath $scopeName
    if (-not (Test-Path -LiteralPath $scopePath -PathType Container)) { continue }
    $pending = [Collections.Generic.Stack[string]]::new()
    $pending.Push($scopePath)
    while ($pending.Count -gt 0) {
        $directory = $pending.Pop()
        try {
            $item = Get-Item -LiteralPath $directory -Force
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Refusing a linked/reparse directory: $directory"
            }
            if ($item.Name -like 'runtime-*') {
                $netPath = Join-Path $directory '.net'
                $cachePath = Join-Path $netPath 'Renode'
                # Enumerate the runtime root so access errors are reported, not mistaken for absence.
                $children = @(Get-ChildItem -LiteralPath $directory -Force -Directory)
                if ($children.Name -contains '.net' -and
                    (Test-Path -LiteralPath $cachePath -PathType Container)) {
                    $resolved = Assert-CachePath $cachePath
                    $size = Measure-SafeTree $resolved
                    $candidates.Add([pscustomobject]@{
                        path = $resolved; bytes = $size.bytes; files = $size.files
                    })
                }
                continue
            }
            foreach ($child in Get-ChildItem -LiteralPath $directory -Directory -Force) {
                $pending.Push($child.FullName)
            }
        } catch {
            $skipped.Add([pscustomobject]@{ path = $directory; error = $_.Exception.Message })
        }
    }
}

if ($Apply) {
    foreach ($candidate in $candidates) {
        try {
            Assert-Stopped
            $resolved = Assert-CachePath $candidate.path
            $size = Measure-SafeTree $resolved
            # Revalidate the absolute target and its complete tree before recursive removal.
            Remove-Item -LiteralPath $resolved -Recurse -Force
            if (Test-Path -LiteralPath $resolved) { throw "Cache survived removal: $resolved" }
            $results.Add([pscustomobject]@{
                path = $resolved; bytes = $size.bytes; files = $size.files; status = 'removed'
            })
        } catch {
            $skipped.Add([pscustomobject]@{ path = $candidate.path; error = $_.Exception.Message })
        }
    }
}

$mode = if ($Apply) { 'apply' } else { 'preview' }
$reportDirectory = Join-Path $buildPath 'maintenance'
New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null
$reportPath = Join-Path $reportDirectory ("renode-cache-{0}-{1}-{2}.json" -f
    (Get-Date -Format 'yyyyMMdd-HHmmss'), $mode, $PID)
$report = [ordered]@{
    mode = $mode
    created_utc = (Get-Date).ToUniversalTime().ToString('o')
    candidates = @($candidates.ToArray())
    removed = @($results.ToArray())
    skipped = @($skipped.ToArray())
}
[IO.File]::WriteAllText($reportPath, ($report | ConvertTo-Json -Depth 6) + "`n",
    [Text.UTF8Encoding]::new($false))
[long]$eligibleBytes = 0
[long]$removedBytes = 0
foreach ($candidate in $candidates) { $eligibleBytes += $candidate.bytes }
foreach ($result in $results) { $removedBytes += $result.bytes }
Write-Output ("Eligible caches: {0}; logical size: {1:N3} GiB" -f $candidates.Count, ($eligibleBytes / 1GB))
Write-Output ("Removed caches: {0}; logical size: {1:N3} GiB" -f $results.Count, ($removedBytes / 1GB))
Write-Output ("Skipped paths: {0}; details: {1}" -f $skipped.Count, $reportPath)
if (-not $Apply) { Write-Output 'Preview only. Pass -Apply to remove the eligible caches.' }
