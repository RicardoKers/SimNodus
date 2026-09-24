# Disposable VMware startup prerequisite. No IPC endpoint or document loading.
param([ValidateSet('Inventory','Probe','Writer')][string]$Mode,
    [ValidatePattern('^[a-f0-9]{12}$')][string]$Tag,[string]$OutputPath,
    [string]$WriterSid,[ValidatePattern('^[a-z]+$')][string]$Case='inventory')
$ErrorActionPreference='Stop'
$utf8=New-Object Text.UTF8Encoding($false)
$harness="C:\SN021Startup-$Tag"
$report=[ordered]@{status='started';stage='environment';mode=$Mode;tag=$Tag;observations=@()}
function Save-Report { [IO.File]::WriteAllText($OutputPath,($report|ConvertTo-Json -Depth 12)+"`n",$utf8) }
function Require($Value,[string]$Code){if(-not $Value){throw "PROBE:$Code"}}
function Protect([string]$Path,[string]$Reader) {
    $acl=New-Object Security.AccessControl.DirectorySecurity
    $acl.SetAccessRuleProtection($true,$false)
    foreach($sid in @('S-1-5-18','S-1-5-32-544')){$acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule(([Security.Principal.SecurityIdentifier]$sid),'FullControl','ContainerInherit,ObjectInherit','None','Allow')))}
    $acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule(([Security.Principal.SecurityIdentifier]$Reader),'ReadAndExecute','ContainerInherit,ObjectInherit','None','Allow')))
    [IO.Directory]::CreateDirectory($Path,$acl)|Out-Null
}
try {
    Require ((Get-CimInstance Win32_ComputerSystem).Manufacturer -match 'VMware') 'not-vmware'
    $id=[Security.Principal.WindowsIdentity]::GetCurrent()
    $admin=(New-Object Security.Principal.WindowsPrincipal($id)).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    $report.token=[ordered]@{sid=$id.User.Value;administrator=$admin;groups=@($id.Groups|ForEach-Object {$_.Value})}
    $os=Get-CimInstance Win32_OperatingSystem;$report.os=[ordered]@{version=$os.Version;build=$os.BuildNumber;caption=$os.Caption}
    $report.filesystem=[IO.DriveInfo]::new('C:\').DriveFormat;Require ($report.filesystem -eq 'NTFS') 'not-ntfs'
    $report.reboot_pending=(Test-Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Component Based Servicing\RebootPending') -or (Test-Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\WindowsUpdate\Auto Update\RebootRequired')
    if($Mode -eq 'Inventory'){$report.status='observed';return}
    Add-Type -TypeDefinition @'
// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
// Disposable fixture startup probe. No production store or IPC implementation.
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.AccessControl;
using System.Security.Cryptography;
using System.Security.Principal;
using System.Text;
using System.Text.RegularExpressions;
using Microsoft.Win32.SafeHandles;
public sealed class Sn021StoreStartup : IDisposable {
    [StructLayout(LayoutKind.Sequential)] struct US {public ushort Length,Maximum;public IntPtr Buffer;}
    [StructLayout(LayoutKind.Sequential)] struct OA {public uint Length;public IntPtr Root,Name;public uint Attributes;public IntPtr Security,Quality;}
    [StructLayout(LayoutKind.Sequential)] struct IOS {public IntPtr Status,Information;}
    [StructLayout(LayoutKind.Sequential)] struct Info {public uint Attr,C0,C1,A0,A1,W0,W1,Volume,High,Low,Links,IndexHigh,IndexLow;}
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern uint QueryDosDeviceW(string n,StringBuilder b,int size);
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode)] static extern uint GetDriveTypeW(string p);
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern SafeFileHandle CreateFileW(string p,uint access,uint share,IntPtr sa,uint mode,uint flags,IntPtr template);
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool GetVolumeInformationByHandleW(SafeFileHandle h,IntPtr name,uint length,IntPtr serial,IntPtr maximum,IntPtr flags,StringBuilder fs,uint size);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetFileInformationByHandle(SafeFileHandle h,out Info i);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetFileInformationByHandleEx(SafeFileHandle h,int kind,byte[] data,uint size);
    [DllImport("kernel32.dll")] static extern uint GetFileType(SafeFileHandle h);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool ReadFile(SafeFileHandle h,byte[] data,uint size,out uint read,IntPtr ov);
    [DllImport("ntdll.dll")] static extern int NtCreateFile(out IntPtr h,uint access,ref OA attributes,out IOS ios,IntPtr allocation,uint attr,uint share,uint disposition,uint options,IntPtr ea,uint length);
    [DllImport("advapi32.dll")] static extern uint GetSecurityInfo(SafeFileHandle h,int kind,uint requested,out IntPtr owner,out IntPtr group,out IntPtr dacl,out IntPtr sacl,out IntPtr sd);
    [DllImport("advapi32.dll")] static extern uint GetSecurityDescriptorLength(IntPtr sd);
    [DllImport("kernel32.dll")] static extern IntPtr LocalFree(IntPtr p);
    readonly List<SafeFileHandle> handles=new List<SafeFileHandle>();
    public readonly List<string> Security=new List<string>();
    public string Stage="volume",Failure;
    public byte[] RootIdentity,FileIdentity,Digest;
    const string BA="S-1-5-32-544",SY="S-1-5-18",BU="S-1-5-32-545";
    static void Need(bool ok,string why){if(!ok)throw new InvalidOperationException(why);}
    static void Native(bool ok){if(!ok)throw new Win32Exception(Marshal.GetLastWin32Error());}
    SafeFileHandle Own(SafeFileHandle h){if(h.IsInvalid){int e=Marshal.GetLastWin32Error();h.Dispose();throw new Win32Exception(e);}handles.Add(h);return h;}
    static byte[] Id(Info i){var b=new byte[12];Buffer.BlockCopy(BitConverter.GetBytes(i.Volume),0,b,0,4);Buffer.BlockCopy(BitConverter.GetBytes(i.IndexHigh),0,b,4,4);Buffer.BlockCopy(BitConverter.GetBytes(i.IndexLow),0,b,8,4);return b;}
    static bool Same(byte[] a,byte[] b){if(a.Length!=b.Length)return false;for(int i=0;i<a.Length;i++)if(a[i]!=b[i])return false;return true;}
    static byte[] Part(byte[] b,int offset,int length){var x=new byte[length];Buffer.BlockCopy(b,offset,x,0,length);return x;}
    Info Inspect(SafeFileHandle h,bool directory){
        Info i;Native(GetFileInformationByHandle(h,out i));Need(GetFileType(h)==1,"not-disk");
        Need((i.Attr&0x1400)==0,"reparse-or-offline");Need(((i.Attr&16)!=0)==directory,"object-type");
        if(directory){byte[] policy=new byte[4];Native(GetFileInformationByHandleEx(h,23,policy,4));Need(BitConverter.ToUInt32(policy,0)==0,"case-sensitive-directory");}
        else Need(i.Links==1,"hardlink-alias");return i;
    }
    RawSecurityDescriptor Descriptor(SafeFileHandle h){
        IntPtr o,g,d,s,sd;uint error=GetSecurityInfo(h,1,5,out o,out g,out d,out s,out sd);if(error!=0)throw new Win32Exception((int)error);
        try{var b=new byte[GetSecurityDescriptorLength(sd)];Marshal.Copy(sd,b,0,b.Length);var value=new RawSecurityDescriptor(b,0);Security.Add(value.GetSddlForm(AccessControlSections.Owner|AccessControlSections.Access));return value;}finally{LocalFree(sd);}
    }
    void ExactAcl(SafeFileHandle h,string reader,bool directory){
        var sd=Descriptor(h);Need(sd.Owner.Value==BA,"wrong-owner");
        var expected=new Dictionary<string,int>{{SY,0x1f01ff},{BA,0x1f01ff},{reader,0x1200a9}};
        Need(sd.DiscretionaryAcl!=null && sd.DiscretionaryAcl.Count==3,"wrong-ace-count");
        Need(!directory || (sd.ControlFlags&ControlFlags.DiscretionaryAclProtected)!=0,"unprotected-directory-dacl");
        foreach(GenericAce a in sd.DiscretionaryAcl){var ace=a as CommonAce;int rights;
            Need(ace!=null && !ace.IsCallback && ace.AceQualifier==AceQualifier.AccessAllowed,"unexpected-ace");
            Need(ace.AceFlags==(directory?(AceFlags.ContainerInherit|AceFlags.ObjectInherit):AceFlags.Inherited),"wrong-inheritance");
            Need(expected.TryGetValue(ace.SecurityIdentifier.Value,out rights)&&rights==ace.AccessMask,"wrong-rights");expected.Remove(ace.SecurityIdentifier.Value);
        }Need(expected.Count==0,"missing-rights");
    }
    SafeFileHandle Volume(){
        Need(GetDriveTypeW("C:\\")==3,"not-fixed-drive");var target=new StringBuilder(1024);Native(QueryDosDeviceW("C:",target,1024)!=0);
        Need(Regex.IsMatch(target.ToString(),@"^\\Device\\HarddiskVolume[0-9]+$"),"drive-alias");
        var h=Own(CreateFileW(@"\\?\GLOBALROOT"+target+"\\",0x120080,1,IntPtr.Zero,3,0x02300000,IntPtr.Zero));Inspect(h,true);
        var fs=new StringBuilder(32);Native(GetVolumeInformationByHandleW(h,IntPtr.Zero,0,IntPtr.Zero,IntPtr.Zero,IntPtr.Zero,fs,32));Need(fs.ToString()=="NTFS","not-ntfs");
        var sd=Descriptor(h);string ti="S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464";
        Need(sd.Owner.Value==BA||sd.Owner.Value==SY||sd.Owner.Value==ti,"untrusted-volume-owner");
        Need(sd.DiscretionaryAcl!=null,"null-volume-dacl");
        foreach(GenericAce a in sd.DiscretionaryAcl){var ace=a as CommonAce;Need(ace!=null&&!ace.IsCallback,"unhandled-volume-ace");
            if(ace.AceQualifier!=AceQualifier.AccessAllowed||(ace.AceFlags&AceFlags.InheritOnly)!=0)continue;
            string sid=ace.SecurityIdentifier.Value;
            if(sid!=BA&&sid!=SY&&sid!=ti)Need((ace.AccessMask&0x500d0040)==0,"unsafe-volume-ancestor-rights");
        }return h;
    }
    SafeFileHandle Child(SafeFileHandle parent,string name,bool directory){
        Need(Regex.IsMatch(name,@"^[a-zA-Z0-9.-]+$"),"invalid-single-component");
        IntPtr text=Marshal.StringToHGlobalUni(name),us=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(US)));
        try{Marshal.StructureToPtr(new US{Length=(ushort)(name.Length*2),Maximum=(ushort)(name.Length*2),Buffer=text},us,false);
            var oa=new OA{Length=(uint)Marshal.SizeOf(typeof(OA)),Root=parent.DangerousGetHandle(),Name=us,Attributes=0x40};IOS ios;IntPtr raw;
            int status=NtCreateFile(out raw,0x120080u|(directory?0u:1u),ref oa,out ios,IntPtr.Zero,0,1,1,0x00600020,IntPtr.Zero,0);
            if(status<0)throw new InvalidOperationException("nt-open:"+status.ToString("x8"));var h=Own(new SafeFileHandle(raw,true));Inspect(h,directory);
            var b=new byte[65536];Native(GetFileInformationByHandleEx(h,2,b,(uint)b.Length));uint n=BitConverter.ToUInt32(b,0);Need(n>0&&n<=65532&&n%2==0,"bad-filename-info");
            var actual=Encoding.Unicode.GetString(b,4,(int)n);actual=actual.Substring(actual.LastIndexOf('\\')+1);Need(String.Equals(actual,name,StringComparison.OrdinalIgnoreCase),"filename-alias");return h;
        }finally{Marshal.FreeHGlobal(us);Marshal.FreeHGlobal(text);}
    }
    byte[] Read(SafeFileHandle h,int limit){
        Info before=Inspect(h,false);Need(before.High==0&&before.Low<=limit,"byte-limit");
        var output=new List<byte>();for(;;){byte[] b=new byte[limit+1-output.Count];uint n;Native(ReadFile(h,b,(uint)b.Length,out n,IntPtr.Zero));if(n==0)break;for(int j=0;j<n;j++)output.Add(b[j]);Need(output.Count<=limit,"byte-limit");}
        Info after=Inspect(h,false);Need(Same(Id(before),Id(after))&&before.Low==after.Low&&output.Count==before.Low,"changed-during-read");return output.ToArray();
    }
    void Open(string leaf,string writer,out SafeFileHandle store){var volume=Volume();Stage="root";var root=Child(volume,leaf,true);ExactAcl(root,BU,true);RootIdentity=Id(Inspect(root,true));Stage="store";store=Child(root,"store",true);ExactAcl(store,writer,true);}
    public static void Seal(string leaf,string writer,string path){
        using(var x=new Sn021StoreStartup()){SafeFileHandle store;x.Open(leaf,writer,out store);x.Stage="data";var data=x.Child(store,"data.bin",false);x.ExactAcl(data,writer,false);var content=x.Read(data,4096);
            var b=new byte[64];Buffer.BlockCopy(Encoding.ASCII.GetBytes("SN21ST01"),0,b,0,8);Buffer.BlockCopy(x.RootIdentity,0,b,8,12);Buffer.BlockCopy(Id(x.Inspect(data,false)),0,b,20,12);
            using(var sha=SHA256.Create())Buffer.BlockCopy(sha.ComputeHash(content),0,b,32,32);
            // Administrator-owned setup writes the fixed manifest before writer startup.
            File.WriteAllBytes(path,b);
        }
    }
    public void Validate(string leaf,string writer){
        try{using(var id=WindowsIdentity.GetCurrent()){Need(id.User.Value==writer,"wrong-writer");Need(!new WindowsPrincipal(id).IsInRole(WindowsBuiltInRole.Administrator),"elevated-writer");}
            SafeFileHandle store;Open(leaf,writer,out store);Stage="manifest";var m=Child(store,"manifest.bin",false);ExactAcl(m,writer,false);byte[] manifest=Read(m,64);
            Need(manifest.Length==64&&Same(Part(manifest,0,8),Encoding.ASCII.GetBytes("SN21ST01")),"manifest-format");Need(Same(RootIdentity,Part(manifest,8,12)),"root-identity");
            Stage="data";var d=Child(store,"data.bin",false);ExactAcl(d,writer,false);FileIdentity=Id(Inspect(d,false));Need(Same(FileIdentity,Part(manifest,20,12)),"file-identity");
            byte[] data=Read(d,4096);using(var sha=SHA256.Create())Digest=sha.ComputeHash(data);Need(Same(Digest,Part(manifest,32,32)),"file-bytes");Stage="validated-handles-retained";
        }catch(Exception e){Failure=e is InvalidOperationException?e.Message:e.GetType().FullName;throw;}
    }
    public void Dispose(){for(int i=handles.Count-1;i>=0;i--)handles[i].Dispose();handles.Clear();}
}

'@
    if($Mode -eq 'Writer') {
        Require (-not $admin -and $report.token.sid -eq $WriterSid -and $report.token.groups -notcontains 'S-1-5-32-544') 'wrong-writer-token'
        $privileges=@(& "$env:windir\System32\whoami.exe" /priv /fo csv /nh | ConvertFrom-Csv -Header Name,Description,State)
        Require ($LASTEXITCODE -eq 0 -and $privileges.Count -gt 0) 'privilege-inventory-failed'
        $report.privileges=@($privileges|Select-Object Name,State)
        foreach($p in $privileges){Require ($p.Name -notmatch '^Se(TakeOwnership|Restore|Backup|Debug|Tcb|Impersonate|AssignPrimaryToken|CreateToken)Privilege$') 'privileged-writer'}
        $state=New-Object Sn021StoreStartup
        try {
            $state.Validate("SN021Startup-$Tag-$Case",$WriterSid)
            $report.status='validated-startup-only'
            $report.root_identity=[Convert]::ToBase64String($state.RootIdentity)
            $report.file_identity=[Convert]::ToBase64String($state.FileIdentity)
            $report.sha256=([BitConverter]::ToString($state.Digest)).Replace('-','').ToLowerInvariant()
            $report.handles_retained_at_success=$true
        } catch {
            $report.status='refused';$report.failure=$state.Failure
            $exception=$_.Exception;while($exception.InnerException){$exception=$exception.InnerException}
            $report.error_type=$exception.GetType().FullName
            if($exception -is [ComponentModel.Win32Exception]){$report.native_error=$exception.NativeErrorCode}
        } finally {
            $report.stage=$state.Stage;$report.security=@($state.Security);$state.Dispose()
        }
        return
    }
    Require ($Mode -eq 'Probe' -and $admin) 'setup-requires-administrator'
    Require (-not $report.reboot_pending) 'restart-pending'
    Require (-not (Test-Path $harness)) 'harness-exists'
    $name='sns'+$Tag
    Require (-not (Get-LocalUser -Name $name -ErrorAction SilentlyContinue)) 'account-exists'
    $password=ConvertTo-SecureString ('Sn!9'+[guid]::NewGuid().ToString('N')) -AsPlainText -Force
    New-LocalUser -Name $name -Password $password -Description 'Disposable SN-021 startup fixture'|Out-Null
    Add-LocalGroupMember -Group (Get-LocalGroup -SID 'S-1-5-32-545') -Member $name
    $WriterSid=(Get-LocalUser -Name $name).SID.Value
    $credential=New-Object Management.Automation.PSCredential("$env:COMPUTERNAME\$name",$password)
    $report.writer_sid=$WriterSid
    Protect $harness 'S-1-5-32-545'
    $output=Join-Path $harness 'output';Protect $output $WriterSid
    $acl=Get-Acl $output
    $acl.SetAccessRule((New-Object Security.AccessControl.FileSystemAccessRule(([Security.Principal.SecurityIdentifier]$WriterSid),'FullControl','ContainerInherit,ObjectInherit','None','Allow')))
    Set-Acl -LiteralPath $output -AclObject $acl
    Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $harness 'probe.ps1')
    $expected=[ordered]@{good='';bytes='file-bytes';identity='file-identity';missing='nt-open:';hardlink='hardlink-alias';owner='wrong-owner';rights='wrong-ace-count';manifest='byte-limit';junction='reparse-or-offline';rootrights='wrong-ace-count'}
    $expectedStage=@{bytes='data';identity='data';missing='data';hardlink='data';owner='data';rights='data';manifest='manifest';junction='store';rootrights='root'}
    foreach($Case in $expected.Keys) {
        $report.stage=$Case;Save-Report
        $leaf="SN021Startup-$Tag-$Case";$root="C:\$leaf";$store=Join-Path $root 'store'
        Require (-not (Test-Path $root)) 'fixture-exists'
        Protect $root 'S-1-5-32-545';Protect $store $WriterSid
        $data=Join-Path $store 'data.bin';$manifest=Join-Path $store 'manifest.bin'
        [IO.File]::WriteAllText($data,'query fixture',$utf8)
        [Sn021StoreStartup]::Seal($leaf,$WriterSid,$manifest)
        $before=[ordered]@{sha256=(Get-FileHash $data -Algorithm SHA256).Hash;manifest_sha256=(Get-FileHash $manifest -Algorithm SHA256).Hash}
        switch($Case) {
            'bytes' {[IO.File]::WriteAllText($data,'other fixture',$utf8)}
            'identity' {$replacement=Join-Path $store 'replacement.bin';[IO.File]::WriteAllText($replacement,'query fixture',$utf8);[IO.File]::Replace($replacement,$data,[System.Management.Automation.Language.NullString]::Value)}
            'missing' {[IO.File]::Move($data,(Join-Path $store 'preserved.bin'))}
            'hardlink' {New-Item -ItemType HardLink -Path (Join-Path $root 'alias.bin') -Target $data|Out-Null}
            'owner' {$acl=Get-Acl $data;$acl.SetOwner([Security.Principal.SecurityIdentifier]$WriterSid);Set-Acl $data $acl}
            'rights' {$acl=Get-Acl $data;$acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule(([Security.Principal.SecurityIdentifier]'S-1-5-32-545'),'Write','Allow')));Set-Acl $data $acl}
            'manifest' {$b=[IO.File]::ReadAllBytes($manifest);[IO.File]::WriteAllBytes($manifest,($b+[byte]0))}
            'junction' {$original=Join-Path $root 'original-store';[IO.Directory]::Move($store,$original);New-Item -ItemType Junction -Path $store -Target $original|Out-Null}
            'rootrights' {$acl=Get-Acl $root;$acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule(([Security.Principal.SecurityIdentifier]$WriterSid),'Write','Allow')));Set-Acl $root $acl}
        }
        $repeats=if($Case -eq 'good'){2}else{1}
        for($i=0;$i -lt $repeats;$i++) {
            $destination=Join-Path $output "$Case-$i.json"
            $args=@('-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',('"'+(Join-Path $harness 'probe.ps1')+'"'),'-Mode','Writer','-Tag',$Tag,'-OutputPath',('"'+$destination+'"'),'-WriterSid',$WriterSid,'-Case',$Case)
            $process=Start-Process -FilePath "$env:windir\System32\WindowsPowerShell\v1.0\powershell.exe" -ArgumentList $args -Credential $credential -WorkingDirectory $harness -LoadUserProfile -WindowStyle Hidden -PassThru
            Require ($process.WaitForExit(30000)) 'writer-timeout-left-for-inspection'
            Require (Test-Path $destination) 'missing-writer-report'
            $result=Get-Content $destination -Raw|ConvertFrom-Json
            $report.observations+=[ordered]@{case=$Case;iteration=$i;pid=$process.Id;before=$before;writer=$result};Save-Report
            if($Case -eq 'good') {
                Require ($result.status -eq 'validated-startup-only' -and $result.sha256 -eq $before.sha256.ToLowerInvariant()) 'good-startup-failed'
                Require ((Get-FileHash $data -Algorithm SHA256).Hash -eq $before.sha256 -and (Get-FileHash $manifest -Algorithm SHA256).Hash -eq $before.manifest_sha256) 'good-fixture-changed'
            } else {
                Require ($result.status -eq 'refused' -and $result.stage -eq $expectedStage[$Case] -and $result.failure -and $result.failure.StartsWith($expected[$Case])) 'negative-result-inconclusive'
            }
        }
    }
    $report.status='observed-startup-prerequisite-only';$report.stage='complete'
    $report.limits=@('No pipe endpoint or document protocol','No production manifest format','No concurrent hostile namespace changes','No save/version/ABA or recovery','No service')
} catch {
    $report.status='failed';$report.error_type=$_.Exception.GetType().FullName;$report.error_line=$_.InvocationInfo.ScriptLineNumber
    if($_.Exception.Message.StartsWith('PROBE:')){$report.failure=$_.Exception.Message}
    $e=$_.Exception;while($e.InnerException){$e=$e.InnerException}
    if($e -is [ComponentModel.Win32Exception]){$report.native_error=$e.NativeErrorCode}
    if($e -is [InvalidOperationException]){$report.detail=$e.Message}
} finally {Save-Report}
