# Manual disposable-VM managed RC lifecycle experiment. No service or engine start.
param([Parameter(Mandatory=$true)][ValidateSet('Inventory','Probe')][string]$Mode,
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-f0-9]{12}$')][string]$Tag,
    [Parameter(Mandatory=$true)][string]$OutputPath,
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-f0-9]{64}$')][string]$ExpectedStoreSha256,
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-f0-9]{64}$')][string]$ExpectedRequestSha256,
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-f0-9]{64}$')][string]$ExpectedLifecycleSha256,
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-f0-9]{64}$')][string]$ExpectedProjectSha256,
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-f0-9]{64}$')][string]$ExpectedLicenseSha256,
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-f0-9]{64}$')][string]$ExpectedSvgSha256,
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-f0-9]{64}$')][string]$ExpectedCirSha256,
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-f0-9]{64}$')][string]$ExpectedDriveSha256)
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
        store_binary_sha256=(Get-FileHash -LiteralPath "C:\Windows\Temp\sn021-managed-lifecycle-$Tag.store.exe" -Algorithm SHA256).Hash.ToLowerInvariant()
        binary_sha256=(Get-FileHash -LiteralPath "C:\Windows\Temp\sn021-managed-lifecycle-$Tag.request.exe" -Algorithm SHA256).Hash.ToLowerInvariant()
        lifecycle_binary_sha256=(Get-FileHash -LiteralPath "C:\Windows\Temp\sn021-managed-lifecycle-$Tag.lifecycle.exe" -Algorithm SHA256).Hash.ToLowerInvariant()
        project_sha256=(Get-FileHash -LiteralPath "C:\Windows\Temp\sn021-managed-lifecycle-$Tag.project.json" -Algorithm SHA256).Hash.ToLowerInvariant()
        license_sha256=(Get-FileHash -LiteralPath "C:\Windows\Temp\sn021-managed-lifecycle-$Tag.license" -Algorithm SHA256).Hash.ToLowerInvariant()
        svg_sha256=(Get-FileHash -LiteralPath "C:\Windows\Temp\sn021-managed-lifecycle-$Tag.svg" -Algorithm SHA256).Hash.ToLowerInvariant()
        cir_sha256=(Get-FileHash -LiteralPath "C:\Windows\Temp\sn021-managed-lifecycle-$Tag.cir" -Algorithm SHA256).Hash.ToLowerInvariant()
        drive_sha256=(Get-FileHash -LiteralPath "C:\Windows\Temp\sn021-managed-lifecycle-$Tag.drive.csv" -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    Require ($report.staged.store_binary_sha256 -ceq $ExpectedStoreSha256 -and
        $report.staged.binary_sha256 -ceq $ExpectedRequestSha256 -and
        $report.staged.lifecycle_binary_sha256 -ceq $ExpectedLifecycleSha256 -and
        $report.staged.project_sha256 -ceq $ExpectedProjectSha256 -and
        $report.staged.license_sha256 -ceq $ExpectedLicenseSha256 -and
        $report.staged.svg_sha256 -ceq $ExpectedSvgSha256 -and
        $report.staged.cir_sha256 -ceq $ExpectedCirSha256 -and
        $report.staged.drive_sha256 -ceq $ExpectedDriveSha256) 'staged-input-identity'
    if($Mode -eq 'Inventory'){$report.status='observed';return}
    Require ($admin -and -not $report.reboot_pending) 'setup-requires-elevated-stable-guest'
    $clock=[Diagnostics.Stopwatch]::StartNew();$harness="C:\SN021LifecycleHarness-$Tag"
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
    $storeExe="$harness\store.exe";$requestExe="$harness\request.exe";$lifecycleExe="$harness\lifecycle.exe"
    $external="$harness\external"
    foreach($directory in @($external,"$external\tests","$external\tests\schema",
        "$external\tests\schema\fixtures","$external\tests\schema\fixtures\assets")){Protect $directory}
    $project="$external\source.json"
    Copy-Item -LiteralPath "C:\Windows\Temp\sn021-managed-lifecycle-$Tag.store.exe" -Destination $storeExe
    Copy-Item -LiteralPath "C:\Windows\Temp\sn021-managed-lifecycle-$Tag.request.exe" -Destination $requestExe
    Copy-Item -LiteralPath "C:\Windows\Temp\sn021-managed-lifecycle-$Tag.lifecycle.exe" -Destination $lifecycleExe
    Copy-Item -LiteralPath "C:\Windows\Temp\sn021-managed-lifecycle-$Tag.project.json" -Destination $project
    Copy-Item -LiteralPath "C:\Windows\Temp\sn021-managed-lifecycle-$Tag.license" -Destination "$external\LICENSE"
    Copy-Item -LiteralPath "C:\Windows\Temp\sn021-managed-lifecycle-$Tag.svg" -Destination "$external\tests\schema\fixtures\assets\passive.svg"
    Copy-Item -LiteralPath "C:\Windows\Temp\sn021-managed-lifecycle-$Tag.cir" -Destination "$external\tests\schema\fixtures\assets\passive.cir"
    Copy-Item -LiteralPath "C:\Windows\Temp\sn021-managed-lifecycle-$Tag.drive.csv" -Destination "$external\fixed-drive.csv"
    $report.store_binary_sha256=(Get-FileHash -LiteralPath $storeExe -Algorithm SHA256).Hash.ToLowerInvariant()
    $report.binary_sha256=(Get-FileHash -LiteralPath $requestExe -Algorithm SHA256).Hash.ToLowerInvariant()
    $report.lifecycle_binary_sha256=(Get-FileHash -LiteralPath $lifecycleExe -Algorithm SHA256).Hash.ToLowerInvariant()
    $report.project_sha256=(Get-FileHash -LiteralPath $project -Algorithm SHA256).Hash.ToLowerInvariant()
    Require ($report.store_binary_sha256 -ceq $ExpectedStoreSha256 -and
        $report.binary_sha256 -ceq $ExpectedRequestSha256 -and
        $report.lifecycle_binary_sha256 -ceq $ExpectedLifecycleSha256 -and
        $report.project_sha256 -ceq $ExpectedProjectSha256 -and
        (Get-FileHash -LiteralPath "$external\LICENSE" -Algorithm SHA256).Hash.ToLowerInvariant() -ceq $ExpectedLicenseSha256 -and
        (Get-FileHash -LiteralPath "$external\tests\schema\fixtures\assets\passive.svg" -Algorithm SHA256).Hash.ToLowerInvariant() -ceq $ExpectedSvgSha256 -and
        (Get-FileHash -LiteralPath "$external\tests\schema\fixtures\assets\passive.cir" -Algorithm SHA256).Hash.ToLowerInvariant() -ceq $ExpectedCirSha256 -and
        (Get-FileHash -LiteralPath "$external\fixed-drive.csv" -Algorithm SHA256).Hash.ToLowerInvariant() -ceq $ExpectedDriveSha256) 'private-input-identity'
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
        using(var h=CreateFileW(path,0x80,7,IntPtr.Zero,3,0x02200000,IntPtr.Zero)) {
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
    $report.stage='provision-exact-import';Save-Report
    $leaf='SN021Managed-'+[guid]::NewGuid().ToString('N').Substring(0,12)
    $report.roots+=@{case='managed-rc-lifecycle';leaf=$leaf;external=$external}
    $sourceHash=$report.project_sha256
    $v=Finish (Start-Child 'a' 'provision' $storeExe @('provision',$leaf,$sids.w,$sids.c))
    Require ($v.status -eq 'provisioned') 'provision'
    $v=Finish (Start-Child 'w' 'import-exact' $storeExe @('import-exact',$leaf,$project))
    Require ($v.status -eq 'imported-exact' -and $v.revision -eq 1 -and
        $v.bytes -eq [IO.File]::ReadAllBytes($project).Length -and $v.project_equal -and $v.context_equal) 'import-exact'
    $rootIdentity=[SNManagedRequestIdentity]::Read($external)
    Require ($v.root_volume -ceq $rootIdentity[0] -and
        $v.root_file -ceq $rootIdentity[1]) 'import-root-file-id'
    $report.external_root_identity=$rootIdentity
    $initialRecord=Snapshot $leaf $false
    Require ($initialRecord.Count -eq 1) 'import-record-count'
    $manifestPath="C:\$leaf\generation.manifest"
    $report.manifest_before_sha256=(Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
    Copy-Item -LiteralPath "C:\$leaf\document\00000001.commit" -Destination "$harness\admin\import-record.bin"
    Artifact "$harness\admin\import-record.bin" 'import-record.bin'
    Copy-Item -LiteralPath $project -Destination "$harness\admin\original-source.json"
    Artifact "$harness\admin\original-source.json" 'original-source.json'
    $report.stage='authenticated-lifecycle';Save-Report
    $server=Server $leaf 3 'lifecycle'
    $initial=$server.state
    $opened=Client $server 'open-initial' (Frame 1 $initial)
    Require ($opened.status -eq 'accepted' -and $opened.token -ceq $initial.token) 'open-initial'
    Decision $opened 'ok' $sids.c $true $true

    function Open-Project([string]$Path){
        $raw=[IO.File]::ReadAllBytes($Path)
        Require ($raw.Length -ge 153 -and [BitConverter]::ToUInt32($raw,32) -eq 0) 'open-frame'
        $contextLength=[BitConverter]::ToUInt32($raw,144)
        Require ($contextLength -ge 52 -and $contextLength -le 16384 -and
            $raw.Length -ge 152+$contextLength) 'open-context'
        $projectLength=[BitConverter]::ToUInt32($raw,148+$contextLength)
        Require ($projectLength -gt 0 -and $projectLength -le 1048576 -and
            $raw.Length -eq 152+$contextLength+$projectLength) 'open-project'
        return ,$raw[(152+$contextLength)..($raw.Length-1)]
    }
    $initialRequest="$harness\c\open-initial.request.bin"
    $initialResponse="$harness\c\open-initial.response.bin"
    Require ((Digest (Open-Project $initialResponse)) -ceq $sourceHash) 'initial-open-not-exact-import'
    $edited="$harness\c\edited.json"
    $v=Finish (Start-Child 'c' 'edit-name' $lifecycleExe @('edit',$initialRequest,$initialResponse,$edited,'Managed RC copy'))
    Require ($v.status -eq 'edited' -and $v.revision -eq 1) 'name-edit'
    $editedBytes=[IO.File]::ReadAllBytes($edited)
    Require ((Digest $editedBytes) -cne $sourceHash -and
        (([Text.Encoding]::UTF8.GetString($editedBytes)|ConvertFrom-Json).name -ceq 'Managed RC copy')) 'name-edit-bytes'
    Artifact $edited 'edited.json'
    $operation=[guid]::NewGuid().ToString('N')
    $committed=Client $server 'save-edited' (Frame 2 $initial $operation $initial.token $editedBytes)
    Require ($committed.status -eq 'accepted' -and $committed.token -cne $initial.token) 'save-edited'
    Decision $committed 'ok' $sids.c $true $true
    $reopened=Client $server 'open-revised' (Frame 1 $initial)
    Require ($reopened.status -eq 'accepted' -and $reopened.token -ceq $committed.token) 'open-revised'
    Decision $reopened 'ok' $sids.c $true $true
    Finish $server.child|Out-Null
    Correspondence $opened $leaf 1 $false
    Correspondence $committed $leaf 2
    Correspondence $reopened $leaf 2 $false
    $reopenedRequest="$harness\c\open-revised.request.bin"
    $reopenedResponse="$harness\c\open-revised.response.bin"
    Require ((Digest (Open-Project $reopenedResponse)) -ceq (Digest $editedBytes)) 'reopen-project-mismatch'
    $recordsBefore=Snapshot $leaf $false
    Require ($recordsBefore.Count -eq 2 -and (Same $initialRecord['00000001.commit'] $recordsBefore['00000001.commit'])) 'prior-record-changed'
    Require ((Get-FileHash -LiteralPath $project -Algorithm SHA256).Hash.ToLowerInvariant() -ceq $sourceHash) 'external-original-changed'

    $report.stage='explicit-export-and-compile';Save-Report
    $v=Finish (Start-Child 'c' 'export-create' $lifecycleExe @('export',$reopenedRequest,$reopenedResponse,"$harness\c",'managed-export.json'))
    Require ($v.status -eq 'exported' -and $v.revision -eq 2) 'export'
    $exported="$harness\c\managed-export.json"
    Require ((Digest ([IO.File]::ReadAllBytes($exported))) -ceq (Digest $editedBytes)) 'export-bytes'
    Artifact $exported 'managed-export.json'
    $collision=Start-Child 'c' 'export-collision' $lifecycleExe @('export',$reopenedRequest,$reopenedResponse,"$harness\c",'managed-export.json')
    $remaining=[Math]::Min(30000,300000-$clock.ElapsedMilliseconds)
    Require ($remaining -gt 0 -and $collision.process.WaitForExit([int]$remaining) -and
        $collision.process.ExitCode -eq 2) 'export-collision-result'
    $collisionLines=@(Lines $collision)
    Require ($collisionLines.Count -gt 0 -and $collisionLines[-1].status -eq 'fixture-failed' -and
        (Get-Content -LiteralPath ($collision.output+'.stderr') -Raw) -match 'export-') 'export-collision-code'
    $report.observations+=@{case='export-collision-refusal';exit_code=$collision.process.ExitCode;
        destination_sha256=(Get-FileHash -LiteralPath $exported -Algorithm SHA256).Hash.ToLowerInvariant()};Save-Report
    Artifact $collision.output 'export-collision.c.jsonl'
    Artifact ($collision.output+'.stderr') 'export-collision.c.stderr'
    Require ((Digest ([IO.File]::ReadAllBytes($exported))) -ceq (Digest $editedBytes)) 'export-collision-mutated'
    $netlist="$harness\c\managed-rc.cir"
    $v=Finish (Start-Child 'c' 'compile-bound' $lifecycleExe @('compile',$reopenedRequest,$reopenedResponse,$netlist))
    Require ($v.status -eq 'compiled-bound' -and $v.revision -eq 2 -and -not $v.readiness -and
        $v.duration_ns -eq 5000000 -and $v.exchange_quantum_ns -eq 5000000) 'compile-bound'
    Artifact $netlist 'managed-rc.cir'
    $report.netlist_sha256=(Get-FileHash -LiteralPath $netlist -Algorithm SHA256).Hash.ToLowerInvariant()
    Require ((Get-FileHash -LiteralPath $project -Algorithm SHA256).Hash.ToLowerInvariant() -ceq $sourceHash) 'external-original-changed'

    function Refused-Compile([string]$Label,[string]$ExpectedCode){
        $output="$harness\c\$Label.cir"
        $refusal=Start-Child 'c' $Label $lifecycleExe @('compile',$reopenedRequest,$reopenedResponse,$output)
        $remaining=[Math]::Min(30000,300000-$clock.ElapsedMilliseconds)
        Require ($remaining -gt 0 -and $refusal.process.WaitForExit([int]$remaining) -and
            $refusal.process.ExitCode -eq 2) ($Label+'-exit')
        $lines=@(Lines $refusal)
        Require ($lines.Count -gt 0 -and $lines[-1].status -eq 'fixture-failed' -and
            (Get-Content -LiteralPath ($refusal.output+'.stderr') -Raw).Trim() -ceq $ExpectedCode -and
            -not (Test-Path -LiteralPath $output)) ($Label+'-refusal')
        $report.observations+=@{case=($Label+'-refusal');exit_code=$refusal.process.ExitCode;
            expected_code=$ExpectedCode;output_absent=$true};Save-Report
        Artifact $refusal.output ($Label+'.c.jsonl')
        Artifact ($refusal.output+'.stderr') ($Label+'.c.stderr')
    }
    $recordsAtCompile=Snapshot $leaf $false
    $drive="$external\fixed-drive.csv"
    [IO.File]::WriteAllBytes($drive,[Text.Encoding]::ASCII.GetBytes("time_ns,drive_uv`n0,3200000`n"))
    Copy-Item -LiteralPath $drive -Destination "$harness\admin\hash-divergent-drive.csv"
    Artifact "$harness\admin\hash-divergent-drive.csv" 'hash-divergent-drive.csv'
    Refused-Compile 'changed-schedule-compile' 'compile-hash'
    Copy-Item -LiteralPath "C:\Windows\Temp\sn021-managed-lifecycle-$Tag.drive.csv" -Destination $drive -Force
    Require ((Get-FileHash -LiteralPath $drive -Algorithm SHA256).Hash.ToLowerInvariant() -ceq $ExpectedDriveSha256) 'schedule-restored'
    $passive="$external\tests\schema\fixtures\assets\passive.cir"
    Move-Item -LiteralPath $passive -Destination "$harness\admin\missing-passive.cir"
    Artifact "$harness\admin\missing-passive.cir" 'held-missing-passive.cir'
    Refused-Compile 'missing-passive-compile' 'compile-filesystem'
    Copy-Item -LiteralPath "C:\Windows\Temp\sn021-managed-lifecycle-$Tag.cir" -Destination $passive
    Require ((Get-FileHash -LiteralPath $passive -Algorithm SHA256).Hash.ToLowerInvariant() -ceq $ExpectedCirSha256) 'passive-restored'
    Require (Same $recordsAtCompile (Snapshot $leaf $false)) 'managed-record-changed-after-resource-refusal'
    Require ((Get-FileHash -LiteralPath $project -Algorithm SHA256).Hash.ToLowerInvariant() -ceq $sourceHash) 'original-changed-after-resource-refusal'

    $report.stage='root-substitution-refusal';Save-Report
    $former="$harness\external-former"
    Require ([IO.Path]::GetFullPath($external) -ceq "$harness\external" -and
        [IO.Path]::GetFullPath($former) -ceq "$harness\external-former" -and
        -not (Test-Path -LiteralPath $former)) 'owned-root-move-target'
    Rename-Item -LiteralPath $external -NewName 'external-former'
    foreach($directory in @($external,"$external\tests","$external\tests\schema",
        "$external\tests\schema\fixtures","$external\tests\schema\fixtures\assets")){Protect $directory}
    Copy-Item -LiteralPath "$former\source.json" -Destination $project
    Copy-Item -LiteralPath "$former\LICENSE" -Destination "$external\LICENSE"
    Copy-Item -LiteralPath "$former\tests\schema\fixtures\assets\passive.svg" -Destination "$external\tests\schema\fixtures\assets\passive.svg"
    Copy-Item -LiteralPath "$former\tests\schema\fixtures\assets\passive.cir" -Destination "$external\tests\schema\fixtures\assets\passive.cir"
    Copy-Item -LiteralPath "$former\fixed-drive.csv" -Destination "$external\fixed-drive.csv"
    foreach($path in @('source.json','LICENSE','tests\schema\fixtures\assets\passive.svg',
        'tests\schema\fixtures\assets\passive.cir','fixed-drive.csv')){
        Require ((Get-FileHash -LiteralPath "$external\$path" -Algorithm SHA256).Hash.ToLowerInvariant() -ceq
            (Get-FileHash -LiteralPath "$former\$path" -Algorithm SHA256).Hash.ToLowerInvariant()) 'root-replacement-bytes'
    }
    $formerIdentity=[SNManagedRequestIdentity]::Read($former)
    $replacementIdentity=[SNManagedRequestIdentity]::Read($external)
    Require ((Same $formerIdentity $rootIdentity) -and -not (Same $replacementIdentity $rootIdentity)) 'replacement-file-id'
    $report.root_substitution=@{former=$formerIdentity;replacement=$replacementIdentity;
        source_sha256=(Get-FileHash -LiteralPath "$former\source.json" -Algorithm SHA256).Hash.ToLowerInvariant()};Save-Report
    Refused-Compile 'replaced-root-compile' 'compile-root_identity'
    Require (Same $recordsBefore (Snapshot $leaf $false)) 'managed-record-changed-after-root-substitution'
    Require ((Get-FileHash -LiteralPath "$former\source.json" -Algorithm SHA256).Hash.ToLowerInvariant() -ceq $sourceHash) 'original-source-changed'
    foreach($file in @(Get-ChildItem -LiteralPath "C:\$leaf\document" -File -Filter '*.commit')){Artifact $file.FullName $file.Name}
    Artifact $manifestPath 'generation.manifest'
    $report.records_final=Snapshot $leaf $false
    Require ((Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant() -ceq
        $report.manifest_before_sha256) 'manifest-changed'
    $report.elapsed_ms=$clock.ElapsedMilliseconds
    $report.stage='complete'
    $report.status='observed-managed-rc-lifecycle-candidate-only'
    $report.limits='Manual exact-import fixture and authenticated Open/Save; no service, power-loss durability, external overwrite, or general runtime readiness. Real ngspice consumption requires separate host audit.'
}catch{
    $report.status='failed';$report.error_type=$_.Exception.GetType().FullName;$report.error=$_.Exception.Message;$report.error_line=$_.InvocationInfo.ScriptLineNumber
}finally{Save-Report}
