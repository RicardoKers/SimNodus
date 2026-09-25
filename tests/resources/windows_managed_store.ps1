# Manual disposable VMware write-enabled managed-store experiment. No services.
param([ValidateSet('Inventory','Probe')][string]$Mode,
    [ValidatePattern('^[a-f0-9]{12}$')][string]$Tag,[string]$OutputPath)
$ErrorActionPreference='Stop';$utf8=New-Object Text.UTF8Encoding($false)
$report=[ordered]@{status='started';stage='environment';tag=$Tag;observations=@();roots=@()}
function Save-Report{[IO.File]::WriteAllText($OutputPath,($report|ConvertTo-Json -Depth 18)+"`n",$utf8)}
function Require($Value,[string]$Code){if(-not $Value){throw "PROBE:$Code"}}
try{
    Require ((Get-CimInstance Win32_ComputerSystem).Manufacturer -match 'VMware') 'not-vmware'
    $id=[Security.Principal.WindowsIdentity]::GetCurrent();$admin=(New-Object Security.Principal.WindowsPrincipal($id)).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    $report.token=[ordered]@{sid=$id.User.Value;administrator=$admin;groups=@($id.Groups|ForEach-Object {$_.Value})}
    $os=Get-CimInstance Win32_OperatingSystem;$report.os=[ordered]@{version=$os.Version;build=$os.BuildNumber;caption=$os.Caption}
    $report.filesystem=[IO.DriveInfo]::new('C:\').DriveFormat;Require ($report.filesystem -eq 'NTFS') 'not-ntfs'
    $report.reboot_pending=(Test-Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Component Based Servicing\RebootPending') -or (Test-Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\WindowsUpdate\Auto Update\RebootRequired')
    if($Mode -eq 'Inventory'){$report.status='observed';return}
    Require ($admin -and -not $report.reboot_pending) 'setup-requires-elevated-stable-guest'
    $clock=[Diagnostics.Stopwatch]::StartNew();$harness="C:\SN021ManagedHarness-$Tag"
    function Protect([string]$Path,[string]$Writer=''){
        $acl=New-Object Security.AccessControl.DirectorySecurity;$acl.SetAccessRuleProtection($true,$false)
        $acl.SetOwner([Security.Principal.SecurityIdentifier]'S-1-5-32-544')
        foreach($sid in @('S-1-5-18','S-1-5-32-544')){$acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule(([Security.Principal.SecurityIdentifier]$sid),'FullControl','ContainerInherit,ObjectInherit','None','Allow')))}
        $acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule(([Security.Principal.SecurityIdentifier]'S-1-5-32-545'),'ReadAndExecute','ContainerInherit,ObjectInherit','None','Allow')))
        if($Writer){$acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule(([Security.Principal.SecurityIdentifier]$Writer),'FullControl','ContainerInherit,ObjectInherit','None','Allow')))}
        Require (-not (Test-Path -LiteralPath $Path)) 'fresh-harness-required';[IO.Directory]::CreateDirectory($Path,$acl)|Out-Null
    }
    $credentials=@{};$sids=@{}
    foreach($role in @('w','c')){
        $name="snm${role}_$Tag";$password=ConvertTo-SecureString ('Q!7a'+[guid]::NewGuid().ToString('N')) -AsPlainText -Force
        New-LocalUser -Name $name -Password $password -Description 'Disposable SN-021 managed store probe'|Out-Null
        Add-LocalGroupMember -Group (Get-LocalGroup -SID 'S-1-5-32-545') -Member $name
        $credentials[$role]=New-Object Management.Automation.PSCredential("$env:COMPUTERNAME\$name",$password);$sids[$role]=(Get-LocalUser -Name $name).SID.Value
    }
    $report.principals=$sids;Protect $harness;Protect "$harness\admin";Protect "$harness\writer" $sids.w;Protect "$harness\client" $sids.c
    $exe="$harness\probe.exe";$project="$harness\project.json"
    Copy-Item -LiteralPath "C:\Windows\Temp\sn021-managed-store-$Tag.exe" -Destination $exe
    Copy-Item -LiteralPath "C:\Windows\Temp\sn021-managed-store-$Tag.project.json" -Destination $project
    $report.binary_sha256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant()
    $report.project_sha256=(Get-FileHash -LiteralPath $project -Algorithm SHA256).Hash.ToLowerInvariant()
    Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
public static class SNManagedObserve {
    [StructLayout(LayoutKind.Sequential)] struct Info { public uint Attr,C0,C1,A0,A1,W0,W1,Volume,High,Low,Links,IndexHigh,IndexLow; }
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern SafeFileHandle CreateFileW(string p,uint access,uint share,IntPtr sa,uint mode,uint flags,IntPtr template);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetFileInformationByHandle(SafeFileHandle h,out Info info);
    [DllImport("kernel32.dll",SetLastError=true)] public static extern bool TerminateProcess(IntPtr handle,uint code);
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] public static extern bool CreateHardLinkW(string alias,string existing,IntPtr security);
    public static string[] Read(string path) {
        using(var h=CreateFileW(path,0x80,7,IntPtr.Zero,3,0x02200000,IntPtr.Zero)) {
            if(h.IsInvalid)throw new Win32Exception(Marshal.GetLastWin32Error());
            Info i;if(!GetFileInformationByHandle(h,out i))throw new Win32Exception(Marshal.GetLastWin32Error());
            return new[]{String.Format("{0:x8}:{1:x8}:{2:x8}",i.Volume,i.IndexHigh,i.IndexLow),i.Links.ToString(),i.Attr.ToString()};
        }
    }
}
'@
    $script:counter=0
    function Start-Child([string]$Role,[string]$Label,[string[]]$Arguments){
        Require ($clock.ElapsedMilliseconds -lt 300000) 'batch-deadline-no-retry'
        $script:counter++;$directory=@{a='admin';w='writer';c='client'}[$Role]
        $output="$harness\$directory\$($script:counter)-$Label.jsonl";$errors="$output.stderr"
        $nativeArguments=@('--report',$output)+$Arguments
        $options=@{FilePath=$exe;ArgumentList=($nativeArguments|ForEach-Object {'"'+$_+'"'});WorkingDirectory=$harness;WindowStyle='Hidden';PassThru=$true}
        if($Role -ne 'a'){$options.Credential=$credentials[$Role];$options.LoadUserProfile=$true}
        $process=Start-Process @options
        # Retain the exact process handle before any barrier/termination; never reopen by PID.
        $handle=$process.Handle
        return @{process=$process;handle=$handle;output=$output;errors=$errors;role=$Role;label=$Label}
    }
    function Lines($Child){
        if(-not (Test-Path -LiteralPath $Child.output)){return @()}
        $items=@();foreach($line in @(Get-Content -LiteralPath $Child.output)){if($line){try{$items+=($line|ConvertFrom-Json)}catch{}}};return $items
    }
    function Collect($Child){
        $v=[ordered]@{case=$Child.label;role=$Child.role;pid=$Child.process.Id;exited=$Child.process.HasExited;lines=@(Lines $Child);stdout_path=$Child.output;stderr_path=$Child.errors}
        if($Child.process.HasExited){$v.exit_code=$Child.process.ExitCode};if(Test-Path -LiteralPath $Child.errors){$v.stderr=Get-Content -LiteralPath $Child.errors -Raw}
        $report.observations+=$v;Save-Report;return $v
    }
    function Finish($Child,[int]$Timeout=30000){
        $remaining=[Math]::Min($Timeout,300000-$clock.ElapsedMilliseconds);Require ($remaining -gt 0) 'batch-deadline-no-retry'
        if(-not $Child.process.WaitForExit([int]$remaining)){Collect $Child|Out-Null;throw 'PROBE:child-timeout-left-running'}
        $v=Collect $Child
        Require ($v.exit_code -eq 0 -and $v.lines.Count -ge 2) 'child-failed-inspect-retained-output'
        if($Child.role -ne 'a'){Require ($v.lines[0].sid -eq $sids[$Child.role] -and $v.lines[0].pid -eq $Child.process.Id) 'child-token-mismatch'}
        return $v.lines[-1]
    }
    function Invoke-Child([string]$Role,[string]$Label,[string[]]$Arguments,[int]$Timeout=30000){return Finish (Start-Child $Role $Label $Arguments) $Timeout}
    function Fresh([string]$Label){
        $leaf='SN021Managed-'+[guid]::NewGuid().ToString('N').Substring(0,12)
        $report.roots+=@{case=$Label;leaf=$leaf};Save-Report
        $v=Invoke-Child 'a' ($Label+'-provision') @('provision',$leaf,$sids.w,$sids.c)
        Require ($v.status -eq 'provisioned') 'provision';return $leaf
    }
    function Write-Next([string]$Leaf,[string]$Label){
        $v=Invoke-Child 'w' $Label @('write',$Leaf,$project);Require ($v.status -eq 'committed') 'write-not-committed';return $v
    }
    function Snapshot([string]$Leaf,[bool]$MetadataOnly=$false){
        $root="C:\$Leaf";$values=[ordered]@{}
        $paths=@($root)+@(Get-ChildItem -LiteralPath $root -Force -Recurse|Sort-Object FullName|ForEach-Object {$_.FullName})
        foreach($path in $paths){
            $value=[ordered]@{identity=[SNManagedObserve]::Read($path);security=(Get-Acl -LiteralPath $path).Sddl}
            if([IO.File]::Exists($path)){
                $value.length=(Get-Item -LiteralPath $path).Length
                if(-not $MetadataOnly){
                    $bytes=[IO.File]::ReadAllBytes($path);$sha=[Security.Cryptography.SHA256]::Create()
                    try{
                        $value.sha256=([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-','').ToLowerInvariant()
                        if($path.EndsWith('.commit') -and $bytes.Length -ge 32){
                            $value.record_body_sha256=([BitConverter]::ToString($sha.ComputeHash($bytes,0,$bytes.Length-32))).Replace('-','').ToLowerInvariant()
                            $value.record_digest=([BitConverter]::ToString($bytes,$bytes.Length-32,32)).Replace('-','').ToLowerInvariant()
                            $value.integrity_matches=$value.record_body_sha256 -eq $value.record_digest
                        }
                    }finally{$sha.Dispose()}
                }
            }
            $values[$path]=$value
        }
        return $values
    }
    function Record-Snapshot([string]$Leaf,[string]$Label,[bool]$MetadataOnly=$false){
        $snapshot=Snapshot $Leaf $MetadataOnly;$report.observations+=@{case=$Label;snapshot=$snapshot};Save-Report;return $snapshot
    }
    function Ready($Child,[string]$Phase){
        $wait=[Diagnostics.Stopwatch]::StartNew()
        while($wait.ElapsedMilliseconds -lt 20000 -and -not $Child.process.HasExited){
            $lines=@(Lines $Child);if(@($lines|Where-Object {$_.barrier -eq $Phase}).Count){
                Require ($lines[0].pid -eq $Child.process.Id -and $lines[0].sid -eq $sids.w) 'barrier-token';return
            };Start-Sleep -Milliseconds 100
        }
        Collect $Child|Out-Null;throw 'PROBE:barrier-not-observed'
    }
    function Kill-Exact($Child){
        Require (-not $Child.process.HasExited) 'writer-exited-before-kill'
        Require ([SNManagedObserve]::TerminateProcess($Child.handle,99)) 'terminate-held-handle'
        Require ($Child.process.WaitForExit(10000)) 'termination-not-confirmed';Collect $Child|Out-Null
    }
    $report.stage='sequence';Save-Report
    $sequence=Fresh 'sequence'
    $v=Invoke-Child 'w' 'sequence' @('sequence',$sequence,$project) 90000
    Require ($v.status -eq 'sequence-observed' -and $v.revisions -eq 64) 'sequence'
    $snapshot=Record-Snapshot $sequence 'sequence-independent-bytes'
    foreach($entry in $snapshot.GetEnumerator()){if($entry.Key.EndsWith('.commit')){Require $entry.Value.integrity_matches 'independent-digest'}}
    $report.stage='write-enabled-isolation';Save-Report
    $isolation=Fresh 'isolation';Write-Next $isolation 'isolation-baseline'|Out-Null
    $holder=Start-Child 'w' 'isolation-holder' @('write',$isolation,$project,'created','hold');Ready $holder 'created'
    $stage=(Get-ChildItem -LiteralPath "C:\$isolation\document" -Filter 'stage-*.tmp').Name;Require ($stage -is [string]) 'single-staging-object'
    $before=Snapshot $isolation $true
    $v=Invoke-Child 'c' 'client-live' @('attack',$isolation,$stage);Require ($v.status -eq 'mutations-denied' -and $v.count -eq 27) 'client-live'
    $v=Invoke-Child 'w' 'lock-mutation' @('lockattack',$isolation,'none');Require ($v.status -eq 'mutations-denied' -and $v.count -eq 6) 'lock-mutation'
    $v=Invoke-Child 'w' 'second-writer' @('open',$isolation);Require ($v.status -eq 'rejected' -and $v.code -eq 'child-open' -and $v.system -eq 3221225539) 'second-writer-not-excluded'
    $after=Snapshot $isolation $true;Require (($before|ConvertTo-Json -Depth 10 -Compress) -eq ($after|ConvertTo-Json -Depth 10 -Compress)) 'live-mutation-changed-store'
    $report.observations+=@{case='live-isolation-preservation';before=$before;after=$after;preserved=$true};Save-Report
    Kill-Exact $holder
    $v=Invoke-Child 'c' 'client-stopped' @('attack',$isolation,$stage);Require ($v.status -eq 'mutations-denied') 'client-stopped'
    $restarted=Start-Child 'w' 'restarted-holder' @('open',$isolation,$project,'opened','hold');Ready $restarted 'opened'
    $v=Invoke-Child 'c' 'client-restarted' @('attack',$isolation,$stage);Require ($v.status -eq 'mutations-denied') 'client-restarted'
    Kill-Exact $restarted
    $v=Invoke-Child 'w' 'isolation-recovery' @('open',$isolation);Require ($v.status -eq 'opened' -and $v.revision -eq 1) 'orphan-promoted'
    Record-Snapshot $isolation 'isolation-final-preserved-orphan'|Out-Null
    $report.stage='injected-failures';Save-Report
    foreach($phase in @('before-stage','created','chunk','flush','verified','before-publish','published','receipt')){
        $leaf=Fresh ('failure-'+$phase);Write-Next $leaf ('baseline-'+$phase)|Out-Null
        $before=Snapshot $leaf
        $v=Invoke-Child 'w' ('failure-'+$phase) @('write',$leaf,$project,$phase,'fail')
        $published=$phase -in @('published','receipt')
        Require ($v.status -eq 'rejected' -and $v.code -eq 'injected' -and $v.indeterminate -eq $published) 'injected-outcome'
        $v=Invoke-Child 'w' ('recover-failure-'+$phase) @('open',$leaf);$expected=if($published){2}else{1}
        Require ($v.status -eq 'opened' -and $v.revision -eq $expected) 'failure-recovery'
        $after=Record-Snapshot $leaf ('failure-preservation-'+$phase)
        $key="C:\$leaf\document\00000001.commit";Require ($before[$key].sha256 -eq $after[$key].sha256 -and $before[$key].identity[0] -eq $after[$key].identity[0]) 'prior-commit-changed'
    }
    $report.stage='process-interruption';Save-Report
    foreach($phase in @('before-stage','chunk','flushed','before-publish','published','receipt')){
        $leaf=Fresh ('kill-'+$phase);Write-Next $leaf ('kill-baseline-'+$phase)|Out-Null
        $child=Start-Child 'w' ('kill-'+$phase) @('write',$leaf,$project,$phase,'hold');Ready $child $phase
        Record-Snapshot $leaf ('namespace-at-'+$phase) $true|Out-Null;Kill-Exact $child
        $v=Invoke-Child 'w' ('recover-kill-'+$phase) @('open',$leaf);$expected=if($phase -in @('published','receipt')){2}else{1}
        Require ($v.status -eq 'opened' -and $v.revision -eq $expected) 'kill-recovery'
        Record-Snapshot $leaf ('recovered-'+$phase)|Out-Null
        if($phase -eq 'published'){
            Write-Next $leaf 'later-commit-after-lost-reply'|Out-Null
            $v=Invoke-Child 'w' 'reconcile-older-lost-reply' @('reconcile-second',$leaf,$project);Require ($v.status -eq 'older-receipt-reconciled') 'older-reconciliation'
        }
    }
    $report.stage='startup-corruption';Save-Report
    foreach($case in @('corrupt','gap','operation','acl','owner','hardlink','unknown','budget')){
        $leaf=Fresh ('negative-'+$case);Write-Next $leaf ('negative-baseline-'+$case)|Out-Null
        if($case -in @('gap','operation')){Write-Next $leaf ('negative-second-'+$case)|Out-Null}
        $directory="C:\$leaf\document";$first="$directory\00000001.commit"
        $before=Snapshot $leaf
        switch($case){
            corrupt{$bytes=[IO.File]::ReadAllBytes($first);$bytes[$bytes.Length-1]=$bytes[$bytes.Length-1] -bxor 1;[IO.File]::WriteAllBytes($first,$bytes)}
            gap{Move-Item -LiteralPath $first -Destination "$directory\stage-$([guid]::NewGuid().ToString('N')).tmp"}
            operation{
                $path="$directory\00000002.commit";$bytes=[IO.File]::ReadAllBytes($path);$prior=[IO.File]::ReadAllBytes($first);[Array]::Copy($prior,80,$bytes,80,16)
                $request=New-Object IO.MemoryStream
                try{
                    $magic=[Text.Encoding]::ASCII.GetBytes('SNREQ001');$request.Write($magic,0,8)
                    foreach($span in @(@(32,32),@(80,16),@(96,108),@(20,12),@(236,($bytes.Length-268)))){$request.Write($bytes,$span[0],$span[1])}
                    $sha=[Security.Cryptography.SHA256]::Create()
                    try{$hash=$sha.ComputeHash($request.ToArray());[Array]::Copy($hash,0,$bytes,204,32);$hash=$sha.ComputeHash($bytes,0,$bytes.Length-32);[Array]::Copy($hash,0,$bytes,$bytes.Length-32,32)}finally{$sha.Dispose()}
                }finally{$request.Dispose()}
                [IO.File]::WriteAllBytes($path,$bytes)
            }
            acl{$acl=Get-Acl -LiteralPath $first;$acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule(([Security.Principal.SecurityIdentifier]$sids.c),'Read','Allow')));Set-Acl -LiteralPath $first -AclObject $acl}
            owner{$acl=Get-Acl -LiteralPath $first;$acl.SetOwner([Security.Principal.SecurityIdentifier]'S-1-5-32-544');Set-Acl -LiteralPath $first -AclObject $acl}
            hardlink{Require ([SNManagedObserve]::CreateHardLinkW("$directory\stage-$([guid]::NewGuid().ToString('N')).tmp",$first,[IntPtr]::Zero)) 'hardlink-setup'}
            unknown{[IO.File]::WriteAllBytes("$directory\unknown.bin",[byte[]]@())}
            budget{for($i=0;$i -lt 125;$i++){[IO.File]::WriteAllBytes("$directory\stage-$([guid]::NewGuid().ToString('N')).tmp",[byte[]]@())}}
        }
        $injected=Snapshot $leaf
        $v=Invoke-Child 'w' ('negative-open-'+$case) @('open',$leaf);Require ($v.status -eq 'rejected') 'corruption-not-refused'
        $after=Snapshot $leaf;Require (($injected|ConvertTo-Json -Depth 10 -Compress) -eq ($after|ConvertTo-Json -Depth 10 -Compress)) 'recovery-mutated-evidence'
        $report.observations+=@{case=('negative-preservation-'+$case);before=$before;injected=$injected;after=$after;preserved=$true};Save-Report
    }
    $report.stage='complete';$report.elapsed_ms=$clock.ElapsedMilliseconds
    $report.status='observed-managed-store-candidate-only'
    $report.limits='Manual storage candidate; no authenticated save transport, installed service, power-loss durability, or engine integration. Injected I/O failures are labeled; no disk exhaustion claim.'
}catch{
    $report.status='failed';$report.error_type=$_.Exception.GetType().FullName
    $report.error=$_.Exception.Message;$report.error_line=$_.InvocationInfo.ScriptLineNumber
}finally{Save-Report}
