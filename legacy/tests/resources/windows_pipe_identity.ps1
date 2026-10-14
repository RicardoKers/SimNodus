# Disposable VMware guest identity preflight. No document/store requests.
param(
    [ValidateSet('Inventory','Probe','Server','Client')][string]$Mode,
    [ValidatePattern('^[a-f0-9]{12}$')][string]$Tag,
    [string]$OutputPath,
    [string]$WriterSid,[string]$ClientSid,[string]$OtherSid='none',
    [ValidatePattern('^[a-z]+$')][string]$Case='inventory',
    [ValidateSet('yes','no')][string]$Anonymous='no'
)
$ErrorActionPreference='Stop'
$root="C:\SN021PipeIdentity-$Tag"
$utf8=New-Object Text.UTF8Encoding($false)
$report=[ordered]@{status='started';stage='environment';mode=$Mode;tag=$Tag;observations=@()}
function Save-Report { [IO.File]::WriteAllText($OutputPath,($report|ConvertTo-Json -Depth 14)+"`n",$utf8) }
function Require($Value,[string]$Code) {if(-not $Value){throw "PROBE:$Code"}}
function Protect([string]$Path,[string]$WriteSid,[string]$ReadSid) {
    $acl=New-Object Security.AccessControl.DirectorySecurity
    $acl.SetAccessRuleProtection($true,$false)
    foreach($sid in @('S-1-5-18','S-1-5-32-544')) {
        $acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule(([Security.Principal.SecurityIdentifier]$sid),'FullControl','ContainerInherit,ObjectInherit','None','Allow')))
    }
    if($WriteSid){$acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule(([Security.Principal.SecurityIdentifier]$WriteSid),'FullControl','ContainerInherit,ObjectInherit','None','Allow')))}
    if($ReadSid){$acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule(([Security.Principal.SecurityIdentifier]$ReadSid),'ReadAndExecute','ContainerInherit,ObjectInherit','None','Allow')))}
    [IO.Directory]::CreateDirectory($Path,$acl)|Out-Null
}
function Launch($Credential,[string]$ChildMode,[string]$Destination,[string]$AllowedOther,[string]$Anon) {
    $args=@('-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',('"'+(Join-Path $root 'probe.ps1')+'"'),'-Mode',$ChildMode,'-Tag',$Tag,'-OutputPath',('"'+$Destination+'"'),'-WriterSid',$WriterSid,'-ClientSid',$ClientSid,'-OtherSid',$AllowedOther,'-Case',$Case,'-Anonymous',$Anon)
    return Start-Process -FilePath "$env:windir\System32\WindowsPowerShell\v1.0\powershell.exe" -ArgumentList $args -Credential $Credential -WorkingDirectory $root -LoadUserProfile -WindowStyle Hidden -PassThru
}
try {
    Require ((Get-CimInstance Win32_ComputerSystem).Manufacturer -match 'VMware') 'not-vmware'
    $identity=[Security.Principal.WindowsIdentity]::GetCurrent()
    $admin=(New-Object Security.Principal.WindowsPrincipal($identity)).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    $report.token=[ordered]@{sid=$identity.User.Value;administrator=$admin;groups=@($identity.Groups|ForEach-Object {$_.Value})}
    $os=Get-CimInstance Win32_OperatingSystem
    $report.os=[ordered]@{version=$os.Version;build=$os.BuildNumber;caption=$os.Caption}
    $report.filesystem=[IO.DriveInfo]::new('C:\').DriveFormat
    Require ($report.filesystem -eq 'NTFS') 'not-ntfs'
    $report.reboot_pending=(Test-Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Component Based Servicing\RebootPending') -or (Test-Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\WindowsUpdate\Auto Update\RebootRequired')
    if($Mode -eq 'Inventory'){$report.status='observed';return}
    if($Mode -in @('Server','Client')) {
        Require (-not $admin) 'elevated-child'
        $privileges=@(& "$env:windir\System32\whoami.exe" /priv /fo csv /nh | ConvertFrom-Csv -Header Name,Description,State)
        Require ($LASTEXITCODE -eq 0 -and $privileges.Count -gt 0) 'privilege-inventory-failed'
        $report.privileges=@($privileges|Select-Object Name,State)
        foreach($p in $privileges){Require ($p.Name -notmatch '^Se(TakeOwnership|Restore|Backup|Debug|Tcb|Impersonate|AssignPrimaryToken|CreateToken)Privilege$') 'privileged-child'}
        Add-Type -TypeDefinition @'
// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
// Manual disposable-VM identity preflight, not the document request protocol.
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.AccessControl;
using System.Security.Principal;
using Microsoft.Win32.SafeHandles;

public static class Sn021PipeIdentity {
    const uint ClientRights=0x120083, FullRights=0x1f01ff;
    [StructLayout(LayoutKind.Sequential)] struct SA { public int Length; public IntPtr Descriptor; public int Inherit; }
    [StructLayout(LayoutKind.Sequential)] struct OV { public IntPtr Internal,High; public uint Offset,OffsetHigh; public IntPtr Event; }
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern SafeFileHandle CreateNamedPipeW(string n,uint o,uint m,uint count,uint ob,uint ib,uint timeout,ref SA sa);
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern SafeFileHandle CreateFileW(string n,uint access,uint share,IntPtr sa,uint mode,uint flags,IntPtr template);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool ConnectNamedPipe(SafeFileHandle h,IntPtr ov);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool ReadFile(SafeFileHandle h,IntPtr b,uint size,out uint n,IntPtr ov);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool WriteFile(SafeFileHandle h,IntPtr b,uint size,out uint n,IntPtr ov);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetOverlappedResult(SafeFileHandle h,IntPtr ov,out uint n,bool wait);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool CancelIoEx(SafeFileHandle h,IntPtr ov);
    [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr CreateEventW(IntPtr sa,bool manual,bool initial,IntPtr name);
    [DllImport("kernel32.dll",SetLastError=true)] static extern uint WaitForSingleObject(IntPtr h,uint ms);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
    [DllImport("kernel32.dll")] static extern IntPtr GetCurrentThread();
    [DllImport("kernel32.dll")] static extern IntPtr LocalFree(IntPtr p);
    [DllImport("advapi32.dll",SetLastError=true)] static extern bool ImpersonateNamedPipeClient(SafeFileHandle h);
    [DllImport("advapi32.dll",SetLastError=true)] static extern bool OpenThreadToken(IntPtr thread,uint access,bool asSelf,out IntPtr token);
    [DllImport("advapi32.dll",SetLastError=true)] static extern bool GetTokenInformation(IntPtr token,int kind,out int value,int size,out int needed);
    [DllImport("advapi32.dll",SetLastError=true)] static extern bool RevertToSelf();
    [DllImport("advapi32.dll")] static extern uint GetSecurityInfo(SafeFileHandle h,int kind,uint requested,out IntPtr owner,out IntPtr group,out IntPtr dacl,out IntPtr sacl,out IntPtr descriptor);
    [DllImport("advapi32.dll")] static extern uint GetSecurityDescriptorLength(IntPtr sd);

    public sealed class Observation {
        public string Status="started", Stage="start", ProcessSid, AuthenticatedSid, Descriptor, ErrorType;
        public int Pid=Process.GetCurrentProcess().Id, NativeError, IdentificationLevel=-1;
        public bool Authorized, Reverted, ReplyReceived;
        public long ElapsedMs;
    }
    static void Require(bool value,string message) { if(!value) throw new InvalidOperationException(message); }
    static void Native(bool ok) { if(!ok) throw new Win32Exception(Marshal.GetLastWin32Error()); }
    static string Sid() { using(var id=WindowsIdentity.GetCurrent()) return id.User.Value; }
    static string Sddl(string writer,string client,string other) {
        string s="O:"+writer+"G:"+writer+"D:P(A;;0x1f01ff;;;"+writer+")(A;;0x120083;;;"+client+")";
        return other==""?s:s+"(A;;0x120083;;;"+other+")";
    }
    static string Verify(SafeFileHandle h,string writer,string client,string other) {
        IntPtr owner,group,dacl,sacl,sd;
        uint error=GetSecurityInfo(h,1,5,out owner,out group,out dacl,out sacl,out sd);
        if(error!=0) throw new Win32Exception((int)error);
        try {
            byte[] b=new byte[GetSecurityDescriptorLength(sd)];Marshal.Copy(sd,b,0,b.Length);
            var descriptor=new RawSecurityDescriptor(b,0);
            Require(descriptor.Owner.Value==writer,"wrong-pipe-owner");
            var expected=new Dictionary<string,int>();expected.Add(writer,(int)FullRights);expected.Add(client,(int)ClientRights);
            if(other!="")expected.Add(other,(int)ClientRights);
            Require(descriptor.DiscretionaryAcl!=null && descriptor.DiscretionaryAcl.Count==expected.Count,"wrong-pipe-ace-count");
            foreach(GenericAce entry in descriptor.DiscretionaryAcl) {
                var ace=entry as CommonAce;int mask;
                Require(ace!=null && ace.AceQualifier==AceQualifier.AccessAllowed && ace.AceFlags==AceFlags.None && !ace.IsCallback,"unexpected-pipe-ace");
                Require(expected.TryGetValue(ace.SecurityIdentifier.Value,out mask) && ace.AccessMask==mask,"wrong-pipe-rights");
                expected.Remove(ace.SecurityIdentifier.Value);
            }
            Require(expected.Count==0,"missing-pipe-rights");
            return descriptor.GetSddlForm(AccessControlSections.Owner|AccessControlSections.Access);
        } finally { LocalFree(sd); }
    }
    // Buffers and OVERLAPPED survive cancellation until completion is observed.
    static uint Io(SafeFileHandle h,int operation,byte[] bytes,Stopwatch clock,int deadline) {
        IntPtr ev=CreateEventW(IntPtr.Zero,true,false,IntPtr.Zero);
        if(ev==IntPtr.Zero)throw new Win32Exception(Marshal.GetLastWin32Error());
        IntPtr ov=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(OV))),buffer=Marshal.AllocHGlobal(bytes.Length);
        try {
            Marshal.StructureToPtr(new OV{Event=ev},ov,false);
            Marshal.Copy(bytes,0,buffer,bytes.Length);uint n=0;
            bool ok=operation==0?ConnectNamedPipe(h,ov):operation==1?ReadFile(h,buffer,(uint)bytes.Length,out n,ov):WriteFile(h,buffer,(uint)bytes.Length,out n,ov);
            int error=ok?0:Marshal.GetLastWin32Error();
            if(operation==0 && error==535)return 0; // Client connected before ConnectNamedPipe.
            if(!ok && error!=997)throw new Win32Exception(error);
            if(!ok) {
                uint remaining=(uint)Math.Max(0,deadline-clock.ElapsedMilliseconds);
                uint wait=WaitForSingleObject(ev,remaining);
                if(wait!=0) {
                    bool cancelled=CancelIoEx(h,ov);int cancelError=cancelled?0:Marshal.GetLastWin32Error();
                    uint drained=WaitForSingleObject(ev,5000);
                    if(drained!=0) Environment.FailFast("SN021 pending I/O did not drain; process ends without freeing buffers");
                    bool completed=GetOverlappedResult(h,ov,out n,false);int completionError=completed?0:Marshal.GetLastWin32Error();
                    if(completionError==996)Environment.FailFast("SN021 cancellation completion uncertain");
                    throw new TimeoutException("io-deadline;cancel="+cancelError+";completion="+completionError);
                }
                Native(GetOverlappedResult(h,ov,out n,false));
            } else if(operation!=0) Native(GetOverlappedResult(h,ov,out n,false));
            if(operation==1)Marshal.Copy(buffer,bytes,0,bytes.Length);
            return n;
        } finally { Marshal.FreeHGlobal(buffer);Marshal.FreeHGlobal(ov);CloseHandle(ev); }
    }
    static SafeFileHandle Create(string name,string writer,string client,string other) {
        var sd=new RawSecurityDescriptor(Sddl(writer,client,other));byte[] bytes=new byte[sd.BinaryLength];sd.GetBinaryForm(bytes,0);
        var pin=GCHandle.Alloc(bytes,GCHandleType.Pinned);
        try {
            var sa=new SA{Length=Marshal.SizeOf(typeof(SA)),Descriptor=pin.AddrOfPinnedObject(),Inherit=0};
            var h=CreateNamedPipeW(name,0x40080003,14,1,128,128,5000,ref sa);
            if(h.IsInvalid){int error=Marshal.GetLastWin32Error();h.Dispose();throw new Win32Exception(error);}return h;
        } finally {pin.Free();}
    }
    static void Identify(SafeFileHandle h,Observation o,string allowed) {
        Native(ImpersonateNamedPipeClient(h));
        try {
            IntPtr token;Native(OpenThreadToken(GetCurrentThread(),8,true,out token));
            try {
                int level,needed;Native(GetTokenInformation(token,9,out level,4,out needed));o.IdentificationLevel=level;
                Require(level==1,"not-identification-level");
                using(var id=new WindowsIdentity(token))o.AuthenticatedSid=id.User.Value;
            } finally {CloseHandle(token);}
        } finally {
            if(!RevertToSelf())Environment.FailFast("SN021 RevertToSelf failed");
            IntPtr remaining;
            if(OpenThreadToken(GetCurrentThread(),8,true,out remaining)){CloseHandle(remaining);Environment.FailFast("SN021 thread token remains");}
            if(Marshal.GetLastWin32Error()!=1008)Environment.FailFast("SN021 cannot verify thread reversion");
            o.Reverted=true;
        }
        Require(Sid()==o.ProcessSid,"writer-context-changed");
        o.Authorized=o.AuthenticatedSid==allowed;
    }
    public static Observation Run(string role,string pipe,string writer,string client,string other,string ready,bool anonymous) {
        var o=new Observation();var clock=Stopwatch.StartNew();o.ProcessSid=Sid();
        try {
            using(var identity=WindowsIdentity.GetCurrent()) {
                Require(!new WindowsPrincipal(identity).IsInRole(WindowsBuiltInRole.Administrator),"elevated-process");
                foreach(var group in identity.Groups)Require(group.Value!="S-1-5-32-544","administrator-member");
            }
            if(role=="server") {
                Require(o.ProcessSid==writer,"wrong-writer-token");o.Stage="create-pipe";
                using(var h=Create(pipe,writer,client,other)) {
                    o.Descriptor=Verify(h,writer,client,other);File.WriteAllText(ready,"ready");
                    o.Stage="connect";Io(h,0,new byte[1],clock,15000);
                    clock.Restart();o.Stage="read";var request=new byte[2];uint n=Io(h,1,request,clock,5000);
                    Require(n==1 && request[0]==0x51,"bad-preflight-marker");
                    o.Stage="identify";Identify(h,o,client);
                    o.Stage="reply";Io(h,2,new byte[]{o.Authorized?(byte)0x41:(byte)0x44},clock,5000);
                    // Client acknowledgement avoids discarding an unread response on close.
                    o.Stage="ack";var ack=new byte[2];Require(Io(h,1,ack,clock,5000)==1 && ack[0]==0x4b,"bad-ack");
                    o.Status=o.Authorized?"authenticated-allowed":"authenticated-denied";
                }
            } else {
                o.Stage="open-pipe";
                using(var h=CreateFileW(pipe,ClientRights,0,IntPtr.Zero,3,anonymous?0x40100000u:0x40110000u,IntPtr.Zero)) {
                    if(h.IsInvalid)throw new Win32Exception(Marshal.GetLastWin32Error());
                    o.Stage="verify-server";o.Descriptor=Verify(h,writer,client,other);
                    clock.Restart();o.Stage="send";Io(h,2,new byte[]{0x51},clock,5000);
                    o.Stage="receive";var reply=new byte[2];uint n=Io(h,1,reply,clock,5000);
                    Require(n==1 && (reply[0]==0x41 || reply[0]==0x44),"bad-reply");o.ReplyReceived=true;o.Authorized=reply[0]==0x41;
                    Io(h,2,new byte[]{0x4b},clock,5000);o.Status=o.Authorized?"reply-allowed":"reply-denied";
                }
            }
        } catch(Exception e) { o.Status="failed";o.ErrorType=e.GetType().FullName;var w=e as Win32Exception;if(w!=null)o.NativeError=w.NativeErrorCode; }
        o.ElapsedMs=clock.ElapsedMilliseconds;return o;
    }
}

'@
        $pipe="\\.\pipe\SN021Identity-$Tag-$Case"
        $ready=Join-Path $root "writer\$Case.ready"
        $other=if($OtherSid -eq 'none'){''}else{$OtherSid}
        $report.result=[Sn021PipeIdentity]::Run($Mode.ToLowerInvariant(),$pipe,$WriterSid,$ClientSid,$other,$ready,($Anonymous -eq 'yes'))
        $report.status='observed';return
    }
    Require ($Mode -eq 'Probe' -and $admin) 'setup-requires-administrator'
    Require (-not $report.reboot_pending) 'restart-pending'
    Require (-not (Test-Path -LiteralPath $root)) 'fixture-exists'
    $report.stage='fresh-accounts'
    $credentials=@{};$sids=@{}
    foreach($role in @('w','c','u')) {
        $name='snp'+$role+$Tag.Substring(0,10)
        Require (-not (Get-LocalUser -Name $name -ErrorAction SilentlyContinue)) 'account-exists'
        $password=ConvertTo-SecureString ('Sn!9'+[guid]::NewGuid().ToString('N')) -AsPlainText -Force
        New-LocalUser -Name $name -Password $password -Description 'Disposable SN-021 pipe identity fixture'|Out-Null
        Add-LocalGroupMember -Group (Get-LocalGroup -SID 'S-1-5-32-545') -Member $name
        $credentials[$role]=New-Object Management.Automation.PSCredential("$env:COMPUTERNAME\$name",$password)
        $sids[$role]=(Get-LocalUser -Name $name).SID.Value
    }
    $WriterSid=$sids.w;$ClientSid=$sids.c
    $report.principals=$sids
    Protect $root '' 'S-1-5-32-545'
    Protect (Join-Path $root 'writer') $sids.w 'S-1-5-32-545'
    Protect (Join-Path $root 'client') $sids.c ''
    Protect (Join-Path $root 'unauthorized') $sids.u ''
    Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $root 'probe.ps1')
    foreach($Case in @('allowedone','allowedtwo','handlerdeny','anonymous','pipedeny')) {
        $report.stage=$Case;Save-Report
        $other=if($Case -eq 'handlerdeny'){$sids.u}else{'none'}
        $role=if($Case -in @('handlerdeny','pipedeny')){'u'}else{'c'}
        $anon=if($Case -eq 'anonymous'){'yes'}else{'no'}
        $serverOutput=Join-Path $root "writer\$Case.json"
        $directory=if($role -eq 'u'){'unauthorized'}else{'client'}
        $clientOutput=Join-Path $root "$directory\$Case.json"
        $server=Launch $credentials.w 'Server' $serverOutput $other 'no'
        $ready=Join-Path $root "writer\$Case.ready"
        $wait=[Diagnostics.Stopwatch]::StartNew()
        while(-not (Test-Path -LiteralPath $ready) -and -not $server.HasExited -and $wait.ElapsedMilliseconds -lt 20000){Start-Sleep -Milliseconds 100}
        if(-not (Test-Path -LiteralPath $ready)) {
            if(Test-Path -LiteralPath $serverOutput){$report.observations+=Get-Content $serverOutput -Raw|ConvertFrom-Json;Save-Report}
            throw 'PROBE:server-not-ready'
        }
        $client=Launch $credentials[$role] 'Client' $clientOutput $other $anon
        Require ($client.WaitForExit(30000)) 'client-timeout-left-for-inspection'
        Require ($server.WaitForExit(30000)) 'server-timeout-left-for-inspection'
        Require ((Test-Path $serverOutput) -and (Test-Path $clientOutput)) 'missing-child-result'
        $sv=Get-Content $serverOutput -Raw|ConvertFrom-Json
        $cl=Get-Content $clientOutput -Raw|ConvertFrom-Json
        $report.observations+=[ordered]@{case=$Case;server=$sv;client=$cl};Save-Report
        Require ($sv.status -eq 'observed' -and $cl.status -eq 'observed') 'child-harness-failed'
        Require ($sv.token.sid -eq $sids.w -and $cl.token.sid -eq $sids[$role]) 'wrong-child-sid'
        if($Case -in @('allowedone','allowedtwo')) {
            Require ($sv.result.Status -eq 'authenticated-allowed' -and $cl.result.Status -eq 'reply-allowed' -and $sv.result.AuthenticatedSid -eq $sids.c -and $sv.result.Reverted) 'allowed-authentication-failed'
        } elseif($Case -eq 'handlerdeny') {
            Require ($sv.result.Status -eq 'authenticated-denied' -and $cl.result.Status -eq 'reply-denied' -and $sv.result.AuthenticatedSid -eq $sids.u -and $sv.result.Reverted) 'handler-denial-failed'
        } elseif($Case -eq 'anonymous') {
            Require ($sv.result.Status -eq 'failed' -and $sv.result.Stage -eq 'identify' -and $sv.result.NativeError -eq 1347 -and $sv.result.Reverted -and -not $cl.result.ReplyReceived) 'anonymous-result-inconclusive'
        } else {
            Require ($cl.result.Status -eq 'failed' -and $cl.result.Stage -eq 'open-pipe' -and $cl.result.NativeError -eq 5 -and $sv.result.Stage -eq 'connect' -and $sv.result.ErrorType -eq 'System.TimeoutException') 'pipe-denial-inconclusive'
        }
    }
    $report.status='observed-pipe-identity-preflight-only';$report.stage='complete'
    $report.limits=@('No 64-byte document protocol','No protected store startup validation','No service','No save or version/ABA protocol','No full IPC acceptance')
} catch {
    $report.status='failed';$report.error_type=$_.Exception.GetType().FullName
    $report.error_line=$_.InvocationInfo.ScriptLineNumber
    $e=$_.Exception;while($e.InnerException){$e=$e.InnerException}
    if($e -is [ComponentModel.Win32Exception]){$report.native_error=$e.NativeErrorCode}
    if($_.Exception.Message.StartsWith('PROBE:')){$report.failure=$_.Exception.Message}
} finally {Save-Report}
