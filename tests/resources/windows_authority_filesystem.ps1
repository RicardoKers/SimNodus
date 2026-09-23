# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
# Disposable VMware guest experiment. Never run on the development host.
param(
    [ValidateSet('Inventory','Probe','WriterInit','WriterPublish','Client')][string]$Mode,
    [ValidatePattern('^[a-f0-9]{12}$')][string]$Tag,
    [string]$OutputPath,
    [ValidateSet('original fixture','replacement fixture')][string]$Expected = 'original fixture'
)
$ErrorActionPreference = 'Stop'
$root = "C:\SN021Authority-$Tag"
$store = Join-Path $root 'store'
$target = Join-Path $store 'project.json'
$utf8 = New-Object System.Text.UTF8Encoding($false)
$report = [ordered]@{status='started'; mode=$Mode; tag=$Tag; pid=$PID; stage='environment'; observations=@()}
function Save-Report {
    [IO.File]::WriteAllText($OutputPath, ($report | ConvertTo-Json -Depth 12), $utf8)
}
function Require($Condition, [string]$Code) { if (-not $Condition) { throw "PROBE:$Code" } }
function Token {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return [ordered]@{sid=$identity.User.Value; administrator=$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator); groups=@($identity.Groups | ForEach-Object {$_.Value})}
}
function Describe([string]$Path) {
    $item = Get-Item -LiteralPath $Path -Force
    Require (-not ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) 'reparse-object'
    $acl = Get-Acl -LiteralPath $Path
    $owner = (New-Object Security.Principal.NTAccount($acl.Owner)).Translate([Security.Principal.SecurityIdentifier]).Value
    $rules = @($acl.GetAccessRules($true,$true,[Security.Principal.SecurityIdentifier]) | ForEach-Object {
        [ordered]@{sid=$_.IdentityReference.Value;rights=[int64]$_.FileSystemRights;type=$_.AccessControlType.ToString();inherited=$_.IsInherited}
    })
    $physical = [Sn021FileIdentity]::Read($Path)
    Require ($item.PSIsContainer -or $physical[1] -eq '1') 'unexpected-hardlink-count'
    return [ordered]@{leaf=$item.Name; identity=$physical[0]; links=$physical[1]; owner=$owner; sddl=$acl.Sddl; rules=$rules; directory=$item.PSIsContainer}
}
function Protect-Directory([string]$Path, [string]$WriterSid, [string]$ReaderSid) {
    $acl = New-Object Security.AccessControl.DirectorySecurity
    $acl.SetAccessRuleProtection($true,$false)
    foreach ($sid in @('S-1-5-18','S-1-5-32-544')) {
        $rule = New-Object Security.AccessControl.FileSystemAccessRule(([Security.Principal.SecurityIdentifier]$sid),'FullControl','ContainerInherit,ObjectInherit','None','Allow')
        $acl.AddAccessRule($rule)
    }
    if ($WriterSid) {
        $acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule(([Security.Principal.SecurityIdentifier]$WriterSid),'FullControl','ContainerInherit,ObjectInherit','None','Allow')))
    }
    if ($ReaderSid) {
        $acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule(([Security.Principal.SecurityIdentifier]$ReaderSid),'ReadAndExecute','ContainerInherit,ObjectInherit','None','Allow')))
    }
    [IO.Directory]::CreateDirectory($Path,$acl) | Out-Null
}
function Run-Child($Credential, [string]$ChildMode, [string]$Destination, [string]$Value) {
    $args = @('-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',('"'+(Join-Path $root 'probe.ps1')+'"'),'-Mode',$ChildMode,'-Tag',$Tag,'-OutputPath',('"'+$Destination+'"'),'-Expected',('"'+$Value+'"'))
    $process = Start-Process -FilePath "$env:windir\System32\WindowsPowerShell\v1.0\powershell.exe" -ArgumentList $args -Credential $Credential -WorkingDirectory $root -LoadUserProfile -PassThru -WindowStyle Hidden
    if (-not $process.WaitForExit(60000)) { throw 'PROBE:child-timeout-process-left-for-inspection' }
    Require (Test-Path -LiteralPath $Destination) 'missing-child-report'
    $result = Get-Content -LiteralPath $Destination -Raw | ConvertFrom-Json
    $report.observations += $result
    Save-Report
    Require ($result.status -eq 'observed') 'child-observation-failed'
    Require (-not $result.token.administrator) 'child-elevated'
    $expectedSid = if ($ChildMode -eq 'Client') {$script:clientSid} else {$script:writerSid}
    Require ($result.token.sid -eq $expectedSid) 'unexpected-child-identity'
    return $result
}
try {
    $machine = Get-CimInstance Win32_ComputerSystem
    Require ($machine.Manufacturer -match 'VMware') 'not-a-vmware-guest'
    $token = Token
    $report.token = $token
    $os = Get-CimInstance Win32_OperatingSystem
    $report.os = [ordered]@{caption=$os.Caption; version=$os.Version; build=$os.BuildNumber}
    $report.filesystem = [IO.DriveInfo]::new('C:\').DriveFormat
    Require ($report.filesystem -eq 'NTFS') 'not-ntfs'
    $reboot = (Test-Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Component Based Servicing\RebootPending') -or (Test-Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\WindowsUpdate\Auto Update\RebootRequired')
    $report.reboot_pending = $reboot
    if ($Mode -eq 'Inventory') { $report.status='observed'; return }
    Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
public static class Sn021FileIdentity {
    [StructLayout(LayoutKind.Sequential)]
    struct Info { public uint Attr,C0,C1,A0,A1,W0,W1,Volume,High,Low,Links,IndexHigh,IndexLow; }
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]
    static extern SafeFileHandle CreateFileW(string p,uint access,uint share,IntPtr sa,uint mode,uint flags,IntPtr template);
    [DllImport("kernel32.dll",SetLastError=true)]
    static extern bool GetFileInformationByHandle(SafeFileHandle h,out Info info);
    [DllImport("kernel32.dll")] static extern uint GetFileType(SafeFileHandle h);
    public static string[] Read(string path) {
        using(var h=CreateFileW(path,0x80,7,IntPtr.Zero,3,0x02200000,IntPtr.Zero)) {
            if(h.IsInvalid) throw new Win32Exception(Marshal.GetLastWin32Error());
            Info i;
            if(!GetFileInformationByHandle(h,out i)) throw new Win32Exception(Marshal.GetLastWin32Error());
            if(GetFileType(h)!=1 || (i.Attr & 0x400)!=0) throw new InvalidOperationException("Unexpected object type");
            return new [] {String.Format("{0:x8}:{1:x8}:{2:x8}",i.Volume,i.IndexHigh,i.IndexLow),i.Links.ToString()};
        }
    }
}
'@
    if ($Mode -in @('WriterInit','WriterPublish','Client')) {
        Require (-not $token.administrator) 'measured-process-is-administrator'
        Require (-not ($token.groups -contains 'S-1-5-32-544')) 'administrator-group-in-measured-token'
        $privileges=@(& "$env:windir\System32\whoami.exe" /priv /fo csv /nh | ConvertFrom-Csv -Header Name,Description,State)
        Require ($LASTEXITCODE -eq 0 -and $privileges.Count -gt 0) 'privilege-inventory-failed'
        $report.privileges=@($privileges | Select-Object Name,State)
        foreach ($privilege in $privileges) {
            Require ($privilege.Name -notmatch '^Se(TakeOwnership|Restore|Backup|Debug|Tcb|Impersonate|AssignPrimaryToken|CreateToken)Privilege$') 'privileged-measured-token'
        }
        Require (Test-Path -LiteralPath (Join-Path $root 'probe.ps1')) 'missing-owned-harness'
        $report.stage = 'physical-operation'
        if ($Mode -eq 'WriterInit') {
            $stream = [IO.File]::Open($target,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
            try { $bytes=$utf8.GetBytes('original fixture');$stream.Write($bytes,0,$bytes.Length);$stream.Flush($true) } finally { $stream.Dispose() }
        } elseif ($Mode -eq 'WriterPublish') {
            Require ([IO.File]::ReadAllText($target) -eq 'original fixture') 'changed-before-writer'
            $temp=Join-Path $store 'replacement.tmp'
            $stream=[IO.File]::Open($temp,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
            try { $bytes=$utf8.GetBytes('replacement fixture');$stream.Write($bytes,0,$bytes.Length);$stream.Flush($true) } finally {$stream.Dispose()}
            # Windows PowerShell converts an untyped $null string argument to "".
            [IO.File]::Replace($temp,$target,[System.Management.Automation.Language.NullString]::Value)
        } else {
            $before = Describe $target
            $candidate = Join-Path (Split-Path $OutputPath) 'client-candidate.txt'
            [IO.File]::WriteAllText($candidate,'client replacement',$utf8)
            $operations = [ordered]@{
                write = { [IO.File]::WriteAllText($target,'client write',$utf8) }
                create = { [IO.File]::WriteAllText((Join-Path $store 'client-created.txt'),'client create',$utf8) }
                rename = { [IO.File]::Move($target,(Join-Path $store 'client-renamed.json')) }
                delete = { [IO.File]::Delete($target) }
                replace = { [IO.File]::Replace($candidate,$target,[System.Management.Automation.Language.NullString]::Value) }
                hardlink = { New-Item -ItemType HardLink -Path (Join-Path (Split-Path $OutputPath) 'client-link.txt') -Target $target | Out-Null }
                change_acl = { $acl=Get-Acl -LiteralPath $target;$acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule(([Security.Principal.SecurityIdentifier]$token.sid),'FullControl','Allow')));Set-Acl -LiteralPath $target -AclObject $acl }
                change_owner = { $acl=Get-Acl -LiteralPath $target;$acl.SetOwner([Security.Principal.SecurityIdentifier]$token.sid);Set-Acl -LiteralPath $target -AclObject $acl }
                rename_store = { [IO.Directory]::Move($store,(Join-Path $root 'client-store')) }
            }
            foreach ($name in $operations.Keys) {
                $observation=[ordered]@{operation=$name; denied=$false}
                try { & $operations[$name] } catch {
                    $exception=$_.Exception
                    while ($exception.InnerException) {$exception=$exception.InnerException}
                    $observation.error_type=$exception.GetType().FullName
                    $observation.hresult=$exception.HResult
                    $observation.error_code=$exception.HResult -band 65535
                    if ($exception -is [System.ComponentModel.Win32Exception]) {
                        # Win32Exception's generic HRESULT does not encode GetLastError.
                        $observation.native_error_code=$exception.NativeErrorCode
                        $observation.error_code=$exception.NativeErrorCode
                    }
                    $observation.category=$_.CategoryInfo.Category.ToString()
                    $observation.denied=($observation.error_code -in @(5,1314)) -or ($observation.category -eq 'PermissionDenied')
                }
                $report.observations += $observation
                Save-Report
                Require ([IO.File]::ReadAllText($target) -eq $Expected) ('bytes-changed-'+$name)
                $after = Describe $target
                Require ($after.sddl -eq $before.sddl) ('security-changed-'+$name)
                Require ($after.identity -eq $before.identity) ('identity-changed-'+$name)
                $observation.preservation_verified=$true
                Save-Report
                Require $observation.denied ('client-operation-not-denied-'+$name)
            }
        }
        $report.file = Describe $target
        $report.bytes = [IO.File]::ReadAllText($target)
        $report.sha256 = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant()
        $report.status='observed'
        return
    }
    Require ($Mode -eq 'Probe') 'missing-mode'
    Require $token.administrator 'provisioning-needs-guest-administrator'
    Require (-not $reboot) 'guest-restart-pending'
    Require (-not (Test-Path -LiteralPath $root)) 'fixture-root-already-exists'
    $report.stage='fresh-accounts'
    $writerName='snw'+$Tag
    $clientName='snc'+$Tag
    Require (-not (Get-LocalUser -Name $writerName -ErrorAction SilentlyContinue)) 'writer-exists'
    Require (-not (Get-LocalUser -Name $clientName -ErrorAction SilentlyContinue)) 'client-exists'
    $credentials=@{}
    foreach ($name in @($writerName,$clientName)) {
        $password=ConvertTo-SecureString ('Sn!9'+[guid]::NewGuid().ToString('N')) -AsPlainText -Force
        New-LocalUser -Name $name -Password $password -Description 'Disposable SN-021 guest fixture' | Out-Null
        Add-LocalGroupMember -Group (Get-LocalGroup -SID 'S-1-5-32-545') -Member $name
        $credentials[$name]=New-Object Management.Automation.PSCredential("$env:COMPUTERNAME\$name",$password)
    }
    $script:writerSid=(Get-LocalUser -Name $writerName).SID.Value
    $script:clientSid=(Get-LocalUser -Name $clientName).SID.Value
    Require ($writerSid -ne $clientSid) 'same-principal'
    $report.principals=[ordered]@{writer=$writerSid;client=$clientSid}
    $report.stage='protected-fixture'
    Protect-Directory $root '' 'S-1-5-32-545'
    Protect-Directory $store $writerSid $clientSid
    $writerOutput=Join-Path $root 'writer-output'
    $clientOutput=Join-Path $root 'client-output'
    Protect-Directory $writerOutput $writerSid ''
    Protect-Directory $clientOutput $clientSid ''
    Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $root 'probe.ps1')
    $report.fixture=@(Describe $root; Describe $store; Describe $writerOutput; Describe $clientOutput; Describe (Join-Path $root 'probe.ps1'))
    $allowed=@('S-1-5-18','S-1-5-32-544','S-1-5-32-545',$writerSid,$clientSid)
    foreach ($object in $report.fixture) {
        foreach ($rule in $object.rules) { Require ($rule.sid -in $allowed -and $rule.type -eq 'Allow') 'unexpected-acl-principal' }
    }
    Save-Report
    $report.stage='writer-init'
    $init=Run-Child $credentials[$writerName] 'WriterInit' (Join-Path $writerOutput 'init.json') 'original fixture'
    Require ($init.token.sid -eq $writerSid) 'wrong-writer-sid'
    Require ($init.file.owner -eq $writerSid) 'wrong-file-owner'
    $report.stage='first-independent-client'
    $first=Run-Child $credentials[$clientName] 'Client' (Join-Path $clientOutput 'before.json') 'original fixture'
    Require ($first.token.sid -eq $clientSid) 'wrong-client-sid'
    $report.stage='new-writer-process-publication'
    $published=Run-Child $credentials[$writerName] 'WriterPublish' (Join-Path $writerOutput 'publish.json') 'replacement fixture'
    Require ($published.bytes -eq 'replacement fixture') 'publication-bytes'
    $report.stage='second-independent-client'
    $second=Run-Child $credentials[$clientName] 'Client' (Join-Path $clientOutput 'after.json') 'replacement fixture'
    Require ($second.token.sid -eq $clientSid) 'wrong-second-client-sid'
    $report.status='observed-filesystem-candidate-only'
    $report.stage='complete'
    $report.limits=@('No service or IPC','No expected-version/ABA protocol','No crash or power-loss acceptance','No arbitrary-directory overwrite','No production readiness')
} catch {
    $report.status='failed'
    $report.error_type=$_.Exception.GetType().FullName
    $report.error_code=$_.Exception.HResult -band 65535
    if ($_.Exception.Message.StartsWith('PROBE:')) {$report.failure=$_.Exception.Message}
    $report.error_position=$_.InvocationInfo.ScriptLineNumber
} finally { Save-Report }
