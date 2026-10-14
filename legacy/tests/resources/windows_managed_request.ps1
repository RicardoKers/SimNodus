# Manual disposable-VM authenticated managed-document request experiment. No service.
param([Parameter(Mandatory=$true)][ValidateSet('Inventory','Probe','Boundary')][string]$Mode,
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-f0-9]{12}$')][string]$Tag,
    [Parameter(Mandatory=$true)][string]$OutputPath,
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-f0-9]{64}$')][string]$ExpectedStoreSha256,
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-f0-9]{64}$')][string]$ExpectedRequestSha256,
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-f0-9]{64}$')][string]$ExpectedProjectSha256)
$ErrorActionPreference='Stop';$utf8=New-Object Text.UTF8Encoding($false)
$reserved=[IO.File]::Open($OutputPath,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read);$reserved.Dispose()
$report=[ordered]@{status='started';stage='environment';tag=$Tag;observations=@();processes=@();artifacts=@();roots=@()}
function Save-Report{[IO.File]::WriteAllText($OutputPath,($report|ConvertTo-Json -Depth 20)+"`n",$utf8)}
function Require($Value,[string]$Code){if(-not $Value){throw "PROBE:$Code"}}
function Hex([byte[]]$Value){return [BitConverter]::ToString($Value).Replace('-','').ToLowerInvariant()}
function Bytes([string]$Value){
    Require ($Value -cmatch '^(?:[a-f0-9]{2})+$') 'hex-input'
    $result=New-Object byte[] ($Value.Length/2)
    for($i=0;$i -lt $result.Length;$i++){$result[$i]=[Convert]::ToByte($Value.Substring(2*$i,2),16)}
    return ,$result
}
function Digest([byte[]]$Value){$sha=[Security.Cryptography.SHA256]::Create();try{return Hex ($sha.ComputeHash($Value))}finally{$sha.Dispose()}}
try{
    Require ((Get-CimInstance Win32_ComputerSystem).Manufacturer -match 'VMware') 'not-vmware'
    $id=[Security.Principal.WindowsIdentity]::GetCurrent()
    $admin=(New-Object Security.Principal.WindowsPrincipal($id)).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    $report.token=@{sid=$id.User.Value;administrator=$admin;groups=@($id.Groups|ForEach-Object {$_.Value})}
    $os=Get-CimInstance Win32_OperatingSystem;$report.os=@{version=$os.Version;build=$os.BuildNumber;caption=$os.Caption}
    $report.filesystem=[IO.DriveInfo]::new('C:\').DriveFormat;Require ($report.filesystem -eq 'NTFS') 'not-ntfs'
    $report.reboot_pending=(Test-Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Component Based Servicing\RebootPending') -or
        (Test-Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\WindowsUpdate\Auto Update\RebootRequired')
    $report.stage='staged-inputs'
    $report.staged=[ordered]@{
        store_binary_sha256=(Get-FileHash -LiteralPath "C:\Windows\Temp\sn021-managed-request-$Tag.store.exe" -Algorithm SHA256).Hash.ToLowerInvariant()
        binary_sha256=(Get-FileHash -LiteralPath "C:\Windows\Temp\sn021-managed-request-$Tag.exe" -Algorithm SHA256).Hash.ToLowerInvariant()
        project_sha256=(Get-FileHash -LiteralPath "C:\Windows\Temp\sn021-managed-request-$Tag.project.json" -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    Require ($report.staged.store_binary_sha256 -ceq $ExpectedStoreSha256 -and
        $report.staged.binary_sha256 -ceq $ExpectedRequestSha256 -and
        $report.staged.project_sha256 -ceq $ExpectedProjectSha256) 'staged-input-identity'
    if($Mode -eq 'Inventory'){$report.status='observed';return}
    Require ($admin -and -not $report.reboot_pending) 'setup-requires-elevated-stable-guest'
    $clock=[Diagnostics.Stopwatch]::StartNew();$harness="C:\SN021SaveHarness-$Tag"
    function Protect([string]$Path,[string]$Principal=''){
        Require (-not (Test-Path -LiteralPath $Path)) 'fresh-harness-required'
        $acl=New-Object Security.AccessControl.DirectorySecurity;$acl.SetAccessRuleProtection($true,$false)
        $acl.SetOwner([Security.Principal.SecurityIdentifier]'S-1-5-32-544')
        foreach($sid in @('S-1-5-18','S-1-5-32-544')){
            $acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule(([Security.Principal.SecurityIdentifier]$sid),'FullControl','ContainerInherit,ObjectInherit','None','Allow')))
        }
        $acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule(([Security.Principal.SecurityIdentifier]'S-1-5-32-545'),'ReadAndExecute','ContainerInherit,ObjectInherit','None','Allow')))
        if($Principal){$acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule(([Security.Principal.SecurityIdentifier]$Principal),'FullControl','ContainerInherit,ObjectInherit','None','Allow')))}
        [IO.Directory]::CreateDirectory($Path,$acl)|Out-Null
    }
    $description='SN-021 disposable request probe'
    Require ($description.Length -le 48) 'account-description-bounds'
    $credentials=@{};$sids=@{}
    foreach($role in @('w','c','u')){
        $name="sns${role}_$Tag";$password=ConvertTo-SecureString ('Q!7a'+[guid]::NewGuid().ToString('N')) -AsPlainText -Force
        New-LocalUser -Name $name -Password $password -Description $description|Out-Null
        Add-LocalGroupMember -Group (Get-LocalGroup -SID 'S-1-5-32-545') -Member $name
        $credentials[$role]=New-Object Management.Automation.PSCredential("$env:COMPUTERNAME\$name",$password)
        $sids[$role]=(Get-LocalUser -Name $name).SID.Value
    }
    $report.principals=$sids;Protect $harness
    foreach($role in @('w','c','u')){Protect "$harness\$role" $sids[$role]}
    Protect "$harness\admin"
    $storeExe="$harness\store.exe";$requestExe="$harness\request.exe";$project="$harness\project.json"
    Copy-Item -LiteralPath "C:\Windows\Temp\sn021-managed-request-$Tag.store.exe" -Destination $storeExe
    Copy-Item -LiteralPath "C:\Windows\Temp\sn021-managed-request-$Tag.exe" -Destination $requestExe
    Copy-Item -LiteralPath "C:\Windows\Temp\sn021-managed-request-$Tag.project.json" -Destination $project
    $report.store_binary_sha256=(Get-FileHash -LiteralPath $storeExe -Algorithm SHA256).Hash.ToLowerInvariant()
    $report.binary_sha256=(Get-FileHash -LiteralPath $requestExe -Algorithm SHA256).Hash.ToLowerInvariant()
    $report.project_sha256=(Get-FileHash -LiteralPath $project -Algorithm SHA256).Hash.ToLowerInvariant()
    Require ($report.store_binary_sha256 -ceq $ExpectedStoreSha256 -and
        $report.binary_sha256 -ceq $ExpectedRequestSha256 -and
        $report.project_sha256 -ceq $ExpectedProjectSha256) 'private-input-identity'
    Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
public static class SNManagedRequestIdentity {
    [StructLayout(LayoutKind.Sequential)] struct Id { public ulong Volume; [MarshalAs(UnmanagedType.ByValArray,SizeConst=16)] public byte[] File; }
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern SafeFileHandle CreateFileW(string p,uint a,uint s,IntPtr sa,uint mode,uint flags,IntPtr t);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetFileInformationByHandleEx(SafeFileHandle h,int kind,out Id id,uint size);
    public static string[] Read(string path) {
        using(var h=CreateFileW(path,0x80,7,IntPtr.Zero,3,0x00200000,IntPtr.Zero)) {
            if(h.IsInvalid)throw new Win32Exception(Marshal.GetLastWin32Error());
            Id id;if(!GetFileInformationByHandleEx(h,18,out id,24))throw new Win32Exception(Marshal.GetLastWin32Error());
            return new[]{id.Volume.ToString("x16"),BitConverter.ToString(id.File).Replace("-","").ToLowerInvariant()};
        }
    }
}
'@
    $script:counter=0
    function Start-Child([string]$Role,[string]$Label,[string]$Executable,[string[]]$Arguments){
        Require ($clock.ElapsedMilliseconds -lt 300000) 'batch-deadline-no-retry';$script:counter++
        $directory=if($Role -eq 'a'){'admin'}else{$Role}
        $output="$harness\$directory\$($script:counter)-$Label.jsonl"
        $options=@{FilePath=$Executable;ArgumentList=(@('--report',$output)+$Arguments|ForEach-Object {'"'+$_+'"'});WorkingDirectory=$harness;WindowStyle='Hidden';PassThru=$true}
        if($Role -ne 'a'){$options.Credential=$credentials[$Role];$options.LoadUserProfile=$true}
        $process=Start-Process @options;$handle=$process.Handle
        $report.processes+=@{case=$Label;role=$Role;pid=$process.Id;stdout_path=$output;stderr_path=($output+'.stderr');exact_handle_retained=$true};Save-Report
        return @{process=$process;handle=$handle;output=$output;role=$Role;label=$Label}
    }
    function Lines($Child){
        if(-not (Test-Path -LiteralPath $Child.output)){return @()}
        $stream=[IO.File]::Open($Child.output,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
        try{
            Require ($stream.Length -le 2097152) 'bounded-child-report'
            $reader=New-Object IO.StreamReader($stream,$utf8,$false,4096,$true)
            try{$content=$reader.ReadToEnd()}finally{$reader.Dispose()}
        }finally{$stream.Dispose()}
        # Only complete JSONL records participate in readiness or acceptance.
        $last=$content.LastIndexOf("`n");if($last -lt 0){return @()}
        return @($content.Substring(0,$last+1).Split("`n")|Where-Object {$_}|ForEach-Object {$_|ConvertFrom-Json})
    }
    function Finish($Child,[int]$Timeout=30000){
        $remaining=[Math]::Min($Timeout,300000-$clock.ElapsedMilliseconds);Require ($remaining -gt 0) 'batch-deadline-no-retry'
        $exited=$Child.process.WaitForExit([int]$remaining)
        $v=@{case=$Child.label;role=$Child.role;pid=$Child.process.Id;exited=$exited;lines=@(Lines $Child);stdout_path=$Child.output}
        if($exited){$v.exit_code=$Child.process.ExitCode}
        if(Test-Path -LiteralPath ($Child.output+'.stderr')){$v.stderr=Get-Content -LiteralPath ($Child.output+'.stderr') -Raw}
        $report.observations+=$v;Save-Report
        Require $exited 'child-timeout-left-running';Require ($v.exit_code -eq 0 -and $v.lines.Count -gt 0) 'child-failed-inspect-retained-output'
        Artifact $Child.output ($Child.label+'.'+$Child.role+'.jsonl')
        if(Test-Path -LiteralPath ($Child.output+'.stderr')){Artifact ($Child.output+'.stderr') ($Child.label+'.'+$Child.role+'.stderr')}
        return $v.lines[-1]
    }
    function Artifact([string]$Path,[string]$Name){
        Require (Test-Path -LiteralPath $Path -PathType Leaf) 'artifact-missing'
        $report.artifacts+=@{name=$Name;guest_path=$Path;sha256=(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()};Save-Report
    }
    function Snapshot([string]$Leaf,[bool]$MetadataOnly=$true){
        $result=[ordered]@{}
        foreach($file in @(Get-ChildItem -LiteralPath "C:\$Leaf\document" -File -Force|Sort-Object Name)){
            if($file.Name -eq 'writer.lock'){continue}
            $result[$file.Name]=@{length=$file.Length;sddl=(Get-Acl -LiteralPath $file.FullName).Sddl;identity=[SNManagedRequestIdentity]::Read($file.FullName)}
            if(-not $MetadataOnly){$result[$file.Name].sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
        }
        return $result
    }
    function Same($A,$B){return ($A|ConvertTo-Json -Depth 8 -Compress) -ceq ($B|ConvertTo-Json -Depth 8 -Compress)}
    function Decision($Result,[string]$Code,[string]$Sid,[bool]$Reverted,[bool]$Dispatched,[int]$System=-1){
        $v=$Result.decision
        Require ($v.code -ceq $Code -and $v.authenticated_sid -ceq $Sid -and $v.reverted -eq $Reverted -and $v.dispatched -eq $Dispatched) 'wrong-server-decision'
        if($System -ge 0){Require ($v.store_system -eq $System) 'wrong-store-refusal'}
        if($Result.status -eq 'accepted'){
            Require ($Result.server_verified -and $Result.reply_status -eq 0 -and $Result.token -ceq $v.token) 'unconfirmed-client-result'
        }
    }
    function Correspondence($Result,[string]$Leaf,[int]$Revision,[bool]$Receipt=$true){
        $path="C:\$Leaf\document\$('{0:x8}' -f $Revision).commit";$data=[IO.File]::ReadAllBytes($path)
        Require ($data.Length -ge 268 -and $data.Length -le 2097152 -and [BitConverter]::ToUInt32($data,16) -eq $Revision) 'record-correspondence-bounds'
        $token=Bytes $Result.token;$identity=[SNManagedRequestIdentity]::Read($path)
        Require ($token.Length -eq 108 -and (Hex $token[0..31]) -ceq (Hex $data[32..63]) -and
            [BitConverter]::ToUInt32($token,32) -eq $Revision -and (Hex $token[36..51]) -ceq (Hex $data[64..79]) -and
            (Hex $token[52..83]) -ceq (Hex $data[($data.Length-32)..($data.Length-1)]) -and
            [BitConverter]::ToUInt64($token,84).ToString('x16') -ceq $identity[0] -and (Hex $token[92..107]) -ceq $identity[1] -and
            (Digest $data[0..($data.Length-33)]) -ceq (Hex $data[($data.Length-32)..($data.Length-1)])) 'token-record-identity-correspondence'
        if($Receipt){Require ($Result.operation_id -ceq (Hex $data[80..95]) -and $Result.request_digest -ceq (Hex $data[204..235])) 'receipt-record-correspondence'}
    }
    function Frame([int]$Operation,$Ready,[string]$OperationId='', [string]$Expected='', [byte[]]$ProjectBytes=@()){
        $body=New-Object IO.MemoryStream;$wire=New-Object IO.MemoryStream
        try{
            foreach($value in @($Ready.run,$Ready.generation,$Ready.document)){$bytes=Bytes $value;$body.Write($bytes,0,$bytes.Length)}
            if($Operation -ne 1){
                foreach($value in @($OperationId,$Expected)){$bytes=Bytes $value;$body.Write($bytes,0,$bytes.Length)}
                $bytes=[BitConverter]::GetBytes([uint32]$ProjectBytes.Length);$body.Write($bytes,0,4);$body.Write($ProjectBytes,0,$ProjectBytes.Length)
            }
            $bytes=[Text.Encoding]::ASCII.GetBytes('SN21SAVE');$wire.Write($bytes,0,8)
            foreach($bytes in @([BitConverter]::GetBytes([uint16]3),[BitConverter]::GetBytes([uint16]$Operation),[BitConverter]::GetBytes([uint32]$body.Length))){$wire.Write($bytes,0,$bytes.Length)}
            $bytes=[guid]::NewGuid().ToByteArray();$wire.Write($bytes,0,16);$bytes=$body.ToArray();$wire.Write($bytes,0,$bytes.Length)
            return ,$wire.ToArray()
        }finally{$body.Dispose();$wire.Dispose()}
    }
    function Server([string]$Leaf,[int]$Count,[string]$Label,[int]$DropIndex=0){
        $pipe='SN021Save-'+[guid]::NewGuid().ToString('N').Substring(0,12);$ready="$harness\w\$Label.ready"
        $child=Start-Child 'w' $Label $requestExe @('server',$Leaf,$pipe,$ready,[string]$Count,$sids.u,[string]$DropIndex)
        $deadline=[Math]::Min(300000,$clock.ElapsedMilliseconds+10000)
        while(-not (Test-Path -LiteralPath $ready)){
            Require (-not $child.process.HasExited -and $clock.ElapsedMilliseconds -lt $deadline) 'server-not-ready-left-for-inspection'
            Start-Sleep -Milliseconds 50
        }
        $state=@(Lines $child|Where-Object {($_.status -eq 'ready') -or ($_.event -eq 'ready')})[-1]
        Require ($state.run.Length -eq 32 -and $state.generation.Length -eq 32 -and $state.document.Length -eq 32 -and $state.token.Length -eq 216) 'ready-fields'
        $report.observations+=@{case=($Label+'-ready');state=$state;pid=$child.process.Id};Save-Report
        return @{child=$child;state=$state;pipe=$pipe;next=0}
    }
    function Server-Fault([string]$Leaf,[string]$Label,[string]$Fault){
        Require ($Fault -in @('terminal-close','terminal-stall')) 'server-fault-kind'
        $pipe='SN021Save-'+[guid]::NewGuid().ToString('N').Substring(0,12);$ready="$harness\w\$Label.ready"
        $child=Start-Child 'w' $Label $requestExe @('server-fault',$Leaf,$pipe,$ready,'2',$sids.u,$Fault)
        $deadline=[Math]::Min(300000,$clock.ElapsedMilliseconds+10000)
        while(-not (Test-Path -LiteralPath $ready)){
            Require (-not $child.process.HasExited -and $clock.ElapsedMilliseconds -lt $deadline) 'fault-server-not-ready'
            Start-Sleep -Milliseconds 50
        }
        $state=@(Lines $child|Where-Object {$_.event -eq 'ready'})[-1]
        Require ($state.run.Length -eq 32 -and $state.token.Length -eq 216) 'fault-server-ready-fields'
        $report.observations+=@{case=($Label+'-ready');state=$state;pid=$child.process.Id};Save-Report
        return @{child=$child;state=$state;pipe=$pipe;next=0}
    }
    function Client($Server,[string]$Label,[byte[]]$Request,[string]$Role='c',[string]$ResponseOption=''){
        $Server.next++
        $input="$harness\$Role\$Label.request.bin";$output="$harness\$Role\$Label.response.bin"
        [IO.File]::WriteAllBytes($input,$Request);Artifact $input ($Label+'.request.bin')
        $arguments=@('client',$Server.pipe,$sids.w,$sids.c,$sids.u,$input,$output)
        if($ResponseOption){$arguments+=$ResponseOption}
        $result=Finish (Start-Child $Role $Label $requestExe $arguments)
        $deadline=[Math]::Min(300000,$clock.ElapsedMilliseconds+2000);$decision=$null
        while(-not $decision){
            $items=@(Lines $Server.child|Where-Object {$_.event -eq 'decision' -and $_.connection -eq $Server.next})
            if($items.Count){Require ($items.Count -eq 1) 'duplicate-server-decision';$decision=$items[0];break}
            Require ($clock.ElapsedMilliseconds -lt $deadline -and -not $Server.child.process.HasExited) 'missing-server-decision'
            Start-Sleep -Milliseconds 20
        }
        $result|Add-Member -NotePropertyName decision -NotePropertyValue $decision
        $report.observations+=@{case=($Label+'-decision');role=$Role;connection=$Server.next;request_sha256=(Digest $Request);decision=$decision};Save-Report
        if(Test-Path -LiteralPath $output){Artifact $output ($Label+'.response.bin')}
        return $result
    }
    function Fault-Listener([string]$Role,[string]$Label,[string]$Kind){
        $pipe='SN021Save-'+[guid]::NewGuid().ToString('N').Substring(0,12)
        $ready="$harness\$Role\$Label.ready"
        $release="$harness\admin\$Label.release"
        $extra=if($Role -eq 'u'){'none'}else{$sids.u}
        $arguments=@('fault-listener',$pipe,$ready,$sids.c,$extra,$Kind)
        if($Kind -eq 'occupied'){$arguments+=$release}
        $child=Start-Child $Role $Label $requestExe $arguments
        $deadline=[Math]::Min(300000,$clock.ElapsedMilliseconds+10000)
        while(-not (Test-Path -LiteralPath $ready)){
            Require (-not $child.process.HasExited -and $clock.ElapsedMilliseconds -lt $deadline) 'listener-not-ready'
            Start-Sleep -Milliseconds 20
        }
        $event=@(Lines $child|Where-Object {$_.event -eq 'listener-ready'})[-1]
        Require ($event.kind -eq $Kind -and $event.descriptor.owner -eq $sids[$Role] -and
            $event.descriptor.dacl_protected) 'listener-identity'
        $report.observations+=@{case=($Label+'-ready');event=$event;pid=$child.process.Id};Save-Report
        return @{child=$child;pipe=$pipe;ready=$ready;release=$release}
    }
    function Expected-Failure($Child,[string]$Code){
        $remaining=[Math]::Min(30000,300000-$clock.ElapsedMilliseconds)
        Require ($remaining -gt 0 -and $Child.process.WaitForExit([int]$remaining)) 'expected-failure-timeout'
        $lines=@(Lines $Child)
        $report.observations+=@{case=$Child.label;role=$Child.role;pid=$Child.process.Id;exited=$true;exit_code=$Child.process.ExitCode;lines=$lines;stdout_path=$Child.output};Save-Report
        Require ($Child.process.ExitCode -eq 2 -and $lines.Count -gt 0 -and $lines[-1].code -eq $Code) 'expected-failure-result'
        Artifact $Child.output ($Child.label+'.'+$Child.role+'.jsonl')
        if(Test-Path -LiteralPath ($Child.output+'.stderr')){Artifact ($Child.output+'.stderr') ($Child.label+'.'+$Child.role+'.stderr')}
        return $lines[-1]
    }
    if($Mode -eq 'Boundary'){
        $report.stage='competing-save';Save-Report
        $leaf='SN021Managed-'+[guid]::NewGuid().ToString('N').Substring(0,12)
        $report.roots+=@{case='v3-boundary';leaf=$leaf}
        $v=Finish (Start-Child 'a' 'boundary-provision' $storeExe @('provision',$leaf,$sids.w,$sids.c))
        Require ($v.status -eq 'provisioned') 'boundary-provision'
        $v=Finish (Start-Child 'w' 'boundary-seed' $storeExe @('write',$leaf,$project))
        Require ($v.status -eq 'committed' -and $v.revision -eq 1) 'boundary-seed'
        $baseline=Snapshot $leaf $false
        $manifestPath="C:\$leaf\generation.manifest"
        $manifestBefore=@{sha256=(Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant();sddl=(Get-Acl -LiteralPath $manifestPath).Sddl;identity=[SNManagedRequestIdentity]::Read($manifestPath)}
        $sourceBefore=(Get-FileHash -LiteralPath $project -Algorithm SHA256).Hash.ToLowerInvariant()
        $seed=[IO.File]::ReadAllBytes($project);$a=New-Object byte[] 71680;[Array]::Copy($seed,$a,$seed.Length)
        for($i=$seed.Length;$i -lt $a.Length;$i++){$a[$i]=32}
        $b=New-Object byte[] ($a.Length+1);[Array]::Copy($a,$b,$a.Length);$b[-1]=32
        $server=Server $leaf 2 'boundary-competing';$initial=$server.state
        $gate="$harness\admin\boundary-competing.start"
        Require (-not (Test-Path -LiteralPath $gate)) 'competing-gate-not-fresh'
        $clients=@()
        foreach($case in @(@{name='compete-a';operation=('61'*16);project=$a},
                           @{name='compete-b';operation=('62'*16);project=$b})){
            $input="$harness\c\$($case.name).request.bin";$output="$harness\c\$($case.name).response.bin"
            [IO.File]::WriteAllBytes($input,(Frame 2 $initial $case.operation $initial.token $case.project))
            Artifact $input ($case.name+'.request.bin')
            $clients+=Start-Child 'c' $case.name $requestExe @('client-wait',$server.pipe,$sids.w,$sids.c,$sids.u,$input,$output,$gate)
        }
        $readyDeadline=[Math]::Min(300000,$clock.ElapsedMilliseconds+10000)
        foreach($child in $clients){
            while(-not (@(Lines $child|Where-Object {$_.event -eq 'client-ready'}).Count -eq 1)){
                Require (-not $child.process.HasExited -and $clock.ElapsedMilliseconds -lt $readyDeadline) 'competing-client-not-ready'
                Start-Sleep -Milliseconds 20
            }
        }
        $gateFile=[IO.File]::Open($gate,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
        try{$gateFile.WriteByte(1)}finally{$gateFile.Dispose()}
        Artifact $gate 'competing.start'
        $report.observations+=@{case='competing-gate';client_pids=@($clients|ForEach-Object {$_.process.Id});elapsed_ms=$clock.ElapsedMilliseconds};Save-Report
        $left=Finish $clients[0];$right=Finish $clients[1]
        foreach($case in @('compete-a','compete-b')){
            $output="$harness\c\$case.response.bin";if(Test-Path -LiteralPath $output){Artifact $output ($case+'.response.bin')}
        }
        Finish $server.child|Out-Null
        $results=@($left,$right);$accepted=@($results|Where-Object {$_.status -eq 'accepted'})
        $refused=@($results|Where-Object {$_.status -eq 'rejected'})
        $decisions=@(Lines $server.child|Where-Object {$_.event -eq 'decision'})
        Require ($accepted.Count -eq 1 -and $refused.Count -eq 1 -and $decisions.Count -eq 2) 'competing-result-count'
        Require ($accepted[0].server_verified -and $refused[0].server_verified -and
            $accepted[0].revision -eq 2 -and $refused[0].reply_status -eq 1) 'competing-client-result'
        Require (@($decisions|Where-Object {$_.code -eq 'ok' -and $_.token -eq $accepted[0].token -and $_.authenticated_sid -eq $sids.c -and $_.reverted -and $_.dispatched}).Count -eq 1 -and
            @($decisions|Where-Object {$_.code -eq 'request' -and $_.store_system -eq 10 -and $_.authenticated_sid -eq $sids.c -and $_.reverted -and $_.dispatched}).Count -eq 1) 'competing-server-decisions'
        Correspondence $accepted[0] $leaf 2
        $after=Snapshot $leaf $false
        Require ($after.Count -eq 2 -and (Same $baseline['00000001.commit'] $after['00000001.commit'])) 'competing-record-set'
        $record=[IO.File]::ReadAllBytes("C:\$leaf\document\00000002.commit")
        $sidLength=[BitConverter]::ToUInt32($record,20);$contextLength=[BitConverter]::ToUInt32($record,24)
        $winnerBytes=if($accepted[0].operation_id -eq ('61'*16)){$a}else{$b}
        Require ($accepted[0].operation_id -in @(('61'*16),('62'*16)) -and
            (Digest $record[(236+$sidLength+$contextLength)..($record.Length-33)]) -eq (Digest $winnerBytes)) 'competing-winner-bytes'
        $manifestAfter=@{sha256=(Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant();sddl=(Get-Acl -LiteralPath $manifestPath).Sddl;identity=[SNManagedRequestIdentity]::Read($manifestPath)}
        Require ((Same $manifestBefore $manifestAfter) -and
            (Get-FileHash -LiteralPath $project -Algorithm SHA256).Hash.ToLowerInvariant() -eq $sourceBefore) 'competing-source-or-manifest-changed'
        $report.competing=@{accepted_operation=$accepted[0].operation_id;rejected_operation=$refused[0].operation_id;final=$after;manifest=$manifestAfter}
        $report.stage='endpoint-authenticity';Save-Report
        foreach($case in @(@{name='wrong-owner';role='u';kind='owner';code='pipe-owner'},
                           @{name='wrong-rights';role='w';kind='rights';code='pipe-rights'})){
            $listener=Fault-Listener $case.role $case.name $case.kind
            $input="$harness\c\$($case.name).request.bin"
            [IO.File]::WriteAllBytes($input,(Frame 1 $initial));Artifact $input ($case.name+'.request.bin')
            $result=Finish (Start-Child 'c' ($case.name+'-client') $requestExe @('client',$listener.pipe,$sids.w,$sids.c,$sids.u,$input))
            Require ($result.status -eq 'failed' -and $result.stage -eq 'verify-server' -and
                $result.code -eq $case.code -and -not $result.request_sent -and -not $result.server_verified) 'hostile-endpoint-client'
            $observed=Finish $listener.child
            Require ($observed.event -eq 'listener-result' -and $observed.status -eq 'closed-without-request' -and
                $observed.request_bytes -eq 0) 'hostile-endpoint-no-request'
        }
        $occupied=Fault-Listener 'w' 'occupied-name' 'occupied'
        $occupiedReady="$harness\w\occupied-second-writer.ready"
        $failed=Start-Child 'w' 'occupied-second-writer' $requestExe @('server',$leaf,$occupied.pipe,$occupiedReady,'1',$sids.u,'0')
        $refusal=Expected-Failure $failed 'pipe-create'
        $listenerAlive=-not $occupied.child.process.HasExited
        Require (-not (Test-Path -LiteralPath $occupiedReady) -and $refusal.system -ne 0 -and
            $listenerAlive) 'occupied-endpoint-exposed'
        $releaseFile=[IO.File]::Open($occupied.release,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
        try{$releaseFile.WriteByte(1)}finally{$releaseFile.Dispose()}
        Artifact $occupied.release 'occupied.release'
        $held=Finish $occupied.child
        Require ($held.event -eq 'listener-result' -and $held.status -eq 'held-name' -and $held.release_observed) 'occupied-name-not-held'
        $report.observations+=@{case='occupied-name-refusal';ready_exists=(Test-Path -LiteralPath $occupiedReady);listener_alive_at_refusal=$listenerAlive;refusal=$refusal;listener=$held};Save-Report
        Require (Same $after (Snapshot $leaf $false)) 'endpoint-mutated-records'

        $report.stage='uncertain-terminal-and-reconcile';Save-Report
        $previous=$accepted[0].token
        $nextBytes=if($accepted[0].operation_id -eq ('61'*16)){$b}else{$a}
        foreach($case in @(@{name='terminal-close';operation=('63'*16);fault='terminal-close';revision=3},
                           @{name='terminal-stall';operation=('64'*16);fault='terminal-stall';revision=4})){
            $fault=Server-Fault $leaf ($case.name+'-writer') $case.fault
            Require ($fault.state.token -eq $previous) 'terminal-initial-token'
            $save=Client $fault ($case.name+'-save') (Frame 2 $fault.state $case.operation $previous $nextBytes)
            Require ($save.status -eq 'failed' -and $save.server_verified -and $save.request_sent -and
                $save.indeterminate -and $save.stage -eq 'terminal' -and $save.decision.indeterminate) 'terminal-failure-falsely-accepted'
            Decision $save $(if($case.fault -eq 'terminal-close'){'terminal-closed'}else{'terminal-stalled'}) $sids.c $true $true
            if($case.fault -eq 'terminal-stall'){
                $saveObservation=@($report.observations|Where-Object {$_.case -eq ($case.name+'-save')})[-1]
                $events=@($saveObservation.lines|Where-Object {$_.event -eq 'cancellation'})
                Require ($events.Count -eq 1 -and $events[0].cancel_error -eq 0 -and $events[0].completion -eq 995) 'terminal-cancellation'
            }
            $reconciled=Client $fault ($case.name+'-reconcile') (Frame 3 $fault.state $case.operation $previous $nextBytes)
            Require ($reconciled.status -eq 'accepted' -and $reconciled.revision -eq $case.revision -and
                $reconciled.operation_id -eq $case.operation) 'terminal-reconcile'
            Decision $reconciled 'ok' $sids.c $true $true
            Finish $fault.child|Out-Null
            Correspondence $reconciled $leaf $case.revision
            $previous=$reconciled.token
            $nextBytes=if($case.revision -eq 3){$winnerBytes}else{$nextBytes}
        }

        $report.stage='silent-client-deadline';Save-Report
        $beforeIdle=Snapshot $leaf $false
        $idleServer=Server $leaf 1 'silent-client-writer'
        $input="$harness\c\silent-client.request.bin"
        [IO.File]::WriteAllBytes($input,(Frame 1 $idleServer.state));Artifact $input 'silent-client.request.bin'
        $idle=Finish (Start-Child 'c' 'silent-client' $requestExe @('client-idle',$idleServer.pipe,$sids.w,$sids.c,$sids.u,$input))
        Require ($idle.status -eq 'held-idle' -and $idle.server_verified -and -not $idle.request_sent) 'silent-client-result'
        Finish $idleServer.child|Out-Null
        $idleLines=@(Lines $idleServer.child)
        $cancel=@($idleLines|Where-Object {$_.event -eq 'cancellation'})
        $denial=@($idleLines|Where-Object {$_.event -eq 'decision'})
        Require ($cancel.Count -eq 1 -and $cancel[0].cancel_error -eq 0 -and $cancel[0].completion -eq 995 -and
            $denial.Count -eq 1 -and $denial[0].code -eq 'deadline' -and $denial[0].stage -eq 'read' -and
            -not $denial[0].dispatched -and (Same $beforeIdle (Snapshot $leaf $false))) 'silent-client-published-or-missed-deadline'

        $final=Snapshot $leaf $false
        Require ($final.Count -eq 4 -and (Same $after['00000001.commit'] $final['00000001.commit']) -and
            (Same $after['00000002.commit'] $final['00000002.commit'])) 'boundary-prior-records'
        $manifestFinal=@{sha256=(Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant();sddl=(Get-Acl -LiteralPath $manifestPath).Sddl;identity=[SNManagedRequestIdentity]::Read($manifestPath)}
        Require ((Same $manifestBefore $manifestFinal) -and
            (Get-FileHash -LiteralPath $project -Algorithm SHA256).Hash.ToLowerInvariant() -eq $sourceBefore) 'boundary-source-or-manifest-changed'
        foreach($file in @(Get-ChildItem -LiteralPath "C:\$leaf\document" -File -Filter '*.commit')){Artifact $file.FullName $file.Name}
        Artifact $manifestPath 'generation.manifest'
        $report.final=$final;$report.manifest=@{before=$manifestBefore;after=$manifestFinal;preserved=$true}
        $report.elapsed_ms=$clock.ElapsedMilliseconds;$report.stage='complete'
        $report.status='observed-v3-boundary-candidate-only'
        $report.limits='Disposable-VM native v3 fixture only; no service, external overwrite, power-loss durability or general runtime readiness.'
        return
    }
    $report.stage='provision-seed';Save-Report
    $leaf='SN021Managed-'+[guid]::NewGuid().ToString('N').Substring(0,12);$report.roots+=@{case='authenticated-requests';leaf=$leaf}
    $v=Finish (Start-Child 'a' 'provision' $storeExe @('provision',$leaf,$sids.w,$sids.c));Require ($v.status -eq 'provisioned') 'provision'
    $v=Finish (Start-Child 'w' 'seed-import' $storeExe @('write',$leaf,$project));Require ($v.status -eq 'committed' -and $v.revision -eq 1) 'seed-import'
    $baseline=Snapshot $leaf $false;$baselineMeta=Snapshot $leaf;$originalHash=$report.project_sha256
    $manifestPath="C:\$leaf\generation.manifest"
    $manifestBefore=@{sha256=(Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant();sddl=(Get-Acl -LiteralPath $manifestPath).Sddl;identity=[SNManagedRequestIdentity]::Read($manifestPath)}
    $seed=[IO.File]::ReadAllBytes($project);$a=New-Object byte[] 71680;[Array]::Copy($seed,$a,$seed.Length)
    for($i=$seed.Length;$i -lt $a.Length;$i++){$a[$i]=32}
    $b=New-Object byte[] ($a.Length+1);[Array]::Copy($a,$b,$a.Length);$b[-1]=32
    $report.stage='authenticated-boundary';Save-Report
    $server=Server $leaf 15 'requests';$initial=$server.state
    $opened=Client $server 'open-authorized' (Frame 1 $initial)
    Require ($opened.status -eq 'accepted' -and $opened.token -eq $initial.token) 'authorized-open'
    Decision $opened 'ok' $sids.c $true $true
    $openBytes=[IO.File]::ReadAllBytes("$harness\c\open-authorized.response.bin")
    Require ($openBytes.Length -ge 152 -and [BitConverter]::ToUInt32($openBytes,32) -eq 0) 'open-response-bounds'
    $contextLength=[BitConverter]::ToUInt32($openBytes,144)
    Require ($contextLength -le 16384 -and $openBytes.Length -ge 152+$contextLength) 'open-context-bounds'
    $projectLength=[BitConverter]::ToUInt32($openBytes,148+$contextLength)
    Require ($projectLength -eq $a.Length -and $openBytes.Length -eq 152+$contextLength+$projectLength) 'open-project-bounds'
    $firstRecord=[IO.File]::ReadAllBytes("C:\$leaf\document\00000001.commit")
    $sidLength=[BitConverter]::ToUInt32($firstRecord,20);$storedContextLength=[BitConverter]::ToUInt32($firstRecord,24)
    Require ($contextLength -eq $storedContextLength -and
        (Hex $openBytes[148..(147+$contextLength)]) -ceq (Hex $firstRecord[(236+$sidLength)..(235+$sidLength+$storedContextLength)]) -and
        (Digest $openBytes[(152+$contextLength)..($openBytes.Length-1)]) -ceq (Digest $a)) 'open-exact-owned-bytes'
    $operation='51'*16;$save=Frame 2 $initial $operation $initial.token $b
    $committed=Client $server 'save-authorized' $save
    Require ($committed.status -eq 'accepted' -and $committed.token.Length -eq 216 -and $committed.token -ne $initial.token) 'authorized-save'
    Decision $committed 'ok' $sids.c $true $true
    $afterSave=Snapshot $leaf;Require ($afterSave.Count -eq 2 -and (Same $baselineMeta['00000001.commit'] $afterSave['00000001.commit'])) 'save-prior-preservation'
    $reconciled=Client $server 'reconcile-committed' (Frame 3 $initial $operation $initial.token $b)
    Require ($reconciled.status -eq 'accepted' -and $reconciled.token -eq $committed.token) 'same-request-reconciliation'
    Decision $reconciled 'ok' $sids.c $true $true
    $negatives=@(
        @{name='stale-version';frame=(Frame 2 $initial ('52'*16) $initial.token $b);role='c';code='request';system=10;dispatch=$true;reverted=$true},
        @{name='operation-collision';frame=(Frame 2 $initial $operation $initial.token $a);role='c';code='request';system=9;dispatch=$true;reverted=$true},
        @{name='unauthorized-principal';frame=(Frame 2 $initial ('53'*16) $committed.token $b);role='u';code='principal';system=0;dispatch=$false;reverted=$true}
    )
    foreach($field in @('generation','document','run')){
        $wrong=@{run=$initial.run;generation=$initial.generation;document=$initial.document};$wrong[$field]='a5'*16
        $negatives+=@{name=('wrong-'+$field);frame=(Frame 2 $wrong ('54'*16) $committed.token $b);role='c';code=('request-'+$field);system=0;dispatch=$false;reverted=$true}
    }
    $bad=Frame 2 $initial ('55'*16) $committed.token $b;$bad[0]=$bad[0] -bxor 1
    $negatives+=@{name='malformed-magic';frame=$bad;role='c';code='request-magic';system=0;dispatch=$false;reverted=$true}
    $bad=Frame 2 $initial ('56'*16) $committed.token $b;$bad[12]=$bad[12] -bxor 1
    $negatives+=@{name='malformed-length';frame=$bad;role='c';code='request-length';system=0;dispatch=$false;reverted=$true}
    $large=New-Object byte[] 1048577;[Array]::Copy($a,$large,$a.Length)
    for($i=$a.Length;$i -lt $large.Length;$i++){$large[$i]=32}
    $bad=Frame 2 $initial ('59'*16) $committed.token $large
    $negatives+=@{name='oversized-message';frame=$bad;role='c';code='request-oversized';system=0;dispatch=$false;reverted=$false}
    foreach($case in $negatives){
        $before=Snapshot $leaf;$result=Client $server $case.name $case.frame $case.role;$after=Snapshot $leaf
        $sid=if($case.reverted){$sids[$case.role]}else{''};Decision $result $case.code $sid $case.reverted $case.dispatch $case.system
        $preserved=Same $before $after;$report.observations+=@{case=($case.name+'-preservation');before=$before;after=$after;preserved=$preserved};Save-Report
        Require ($result.status -ne 'accepted' -and $preserved) 'negative-request-accepted-or-mutated'
    }
    $aba=Client $server 'save-return-to-a' (Frame 2 $initial ('57'*16) $committed.token $a)
    Require ($aba.status -eq 'accepted' -and $aba.token -ne $initial.token -and $aba.token -ne $committed.token) 'aba-version'
    Decision $aba 'ok' $sids.c $true $true
    $before=Snapshot $leaf;$old=Client $server 'aba-old-token' (Frame 2 $initial ('58'*16) $initial.token $a)
    Require ($old.status -ne 'accepted' -and (Same $before (Snapshot $leaf))) 'aba-old-token-accepted'
    Decision $old 'request' $sids.c $true $true 10
    $missing=Client $server 'reconcile-absent-active-run' (Frame 3 $initial ('5b'*16) $aba.token $a)
    Require ($missing.reply_status -eq 4 -and $missing.status -ne 'accepted' -and (Same $before (Snapshot $leaf))) 'absent-receipt-overclaimed'
    Decision $missing 'not-observed' $sids.c $true $true
    Finish $server.child|Out-Null
    Correspondence $opened $leaf 1 $false;Correspondence $committed $leaf 2;Correspondence $reconciled $leaf 2;Correspondence $aba $leaf 3
    $afterFirst=Snapshot $leaf $false
    $report.stage='restart-reconciliation';Save-Report
    $restarted=Server $leaf 2 'restarted';Require ($restarted.state.run -ne $initial.run) 'run-not-rotated'
    $before=Snapshot $leaf;$old=Client $restarted 'old-run-after-restart' $save
    Require ($old.status -ne 'accepted' -and (Same $before (Snapshot $leaf))) 'old-run-accepted-or-mutated'
    Decision $old 'request-run' $sids.c $true $false
    $reconciled=Client $restarted 'older-receipt-after-restart' (Frame 3 $restarted.state $operation $initial.token $b)
    Require ($reconciled.status -eq 'accepted' -and $reconciled.token -eq $committed.token) 'older-receipt-after-restart'
    Decision $reconciled 'ok' $sids.c $true $true
    Finish $restarted.child|Out-Null
    Correspondence $reconciled $leaf 2
    $beforeLost=Snapshot $leaf;$report.stage='lost-reply-reconciliation';Save-Report
    $lostServer=Server $leaf 2 'lost-reply' 1;$lostOperation='5a'*16
    $lost=Client $lostServer 'save-lost-reply' (Frame 2 $lostServer.state $lostOperation $aba.token $b)
    Require ($lost.status -ne 'accepted' -and $lost.server_verified -and $lost.request_sent -and $lost.indeterminate -and $lost.decision.indeterminate) 'lost-reply-falsely-accepted'
    Decision $lost 'reply-dropped' $sids.c $true $true
    $reconciledLost=Client $lostServer 'reconcile-lost-reply' (Frame 3 $lostServer.state $lostOperation $aba.token $b)
    Require ($reconciledLost.status -eq 'accepted' -and $reconciledLost.token -ne $aba.token) 'lost-reply-not-reconciled'
    Decision $reconciledLost 'ok' $sids.c $true $true
    Finish $lostServer.child|Out-Null
    Correspondence $reconciledLost $leaf 4
    $final=Snapshot $leaf $false;$finalMeta=Snapshot $leaf
    Require ($final.Count -eq 4 -and (Same $baseline['00000001.commit'] $final['00000001.commit'])) 'final-records'
    foreach($name in $afterFirst.Keys){Require (Same $afterFirst[$name] $final[$name]) 'prior-record-bytes-changed'}
    foreach($name in $beforeLost.Keys){Require (Same $beforeLost[$name] $finalMeta[$name]) 'lost-reply-prior-record-changed'}
    $lastRecord=[IO.File]::ReadAllBytes("C:\$leaf\document\00000003.commit")
    $lastSidLength=[BitConverter]::ToUInt32($lastRecord,20);$lastContextLength=[BitConverter]::ToUInt32($lastRecord,24)
    Require ((Digest $firstRecord[(236+$sidLength+$storedContextLength)..($firstRecord.Length-33)]) -ceq
        (Digest $lastRecord[(236+$lastSidLength+$lastContextLength)..($lastRecord.Length-33)])) 'aba-exact-content'
    foreach($file in @(Get-ChildItem -LiteralPath "C:\$leaf\document" -File -Filter '*.commit')){Artifact $file.FullName $file.Name}
    $manifestAfter=@{sha256=(Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant();sddl=(Get-Acl -LiteralPath $manifestPath).Sddl;identity=[SNManagedRequestIdentity]::Read($manifestPath)}
    Require (Same $manifestBefore $manifestAfter) 'manifest-changed'
    $report.manifest=@{before=$manifestBefore;after=$manifestAfter;preserved=$true};Artifact $manifestPath 'generation.manifest'
    Require ((Get-FileHash -LiteralPath $project -Algorithm SHA256).Hash.ToLowerInvariant() -eq $originalHash) 'external-source-changed'
    $report.final=$final;$report.elapsed_ms=$clock.ElapsedMilliseconds;$report.stage='complete'
    $report.status='observed-authenticated-managed-request-candidate-only'
    $report.limits='Manual seeded one-document fixture; no context rebind, service, external overwrite, power-loss durability or real RC lifecycle composition. No executable project resource opened.'
}catch{
    $report.status='failed';$report.error_type=$_.Exception.GetType().FullName;$report.error=$_.Exception.Message;$report.error_line=$_.InvocationInfo.ScriptLineNumber
}finally{Save-Report}
