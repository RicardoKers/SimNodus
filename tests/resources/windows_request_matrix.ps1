# Disposable VMware remaining request matrix experiment.
param([ValidateSet('Inventory','Probe','Server','Client','Listener')][string]$Mode,
    [ValidatePattern('^[a-f0-9]{12}$')][string]$Tag,[string]$OutputPath,
    [string]$WriterSid,[string]$ClientSid,[string]$OtherSid='none',
    [ValidatePattern('^[a-z]+$')][string]$Case='inventory',
    [ValidatePattern('^[a-f0-9]{32}$')][string]$DocumentHex,
    [ValidatePattern('^[a-f0-9]{32}$')][string]$RunHex,
    [ValidatePattern('^[a-f0-9]{64}$')][string]$DigestHex,
    [ValidatePattern('^[a-z]+$')][string]$Instance='inventory',
    [ValidatePattern('^(none|[a-f0-9]{128})$')][string]$RequestHex='none')
$ErrorActionPreference='Stop';$utf8=New-Object Text.UTF8Encoding($false)
$harness="C:\SN021Matrix-$Tag"
$report=[ordered]@{status='started';stage='environment';mode=$Mode;tag=$Tag;observations=@()}
function Save-Report{[IO.File]::WriteAllText($OutputPath,($report|ConvertTo-Json -Depth 16)+"`n",$utf8)}
function Require($Value,[string]$Code){if(-not $Value){throw "PROBE:$Code"}}
function Bytes([string]$Hex){$b=New-Object byte[] ($Hex.Length/2);for($i=0;$i -lt $b.Length;$i++){$b[$i]=[Convert]::ToByte($Hex.Substring($i*2,2),16)};return ,$b}
function Protect([string]$Path,[string]$Reader,[string]$Writer=''){
    $acl=New-Object Security.AccessControl.DirectorySecurity;$acl.SetAccessRuleProtection($true,$false)
    foreach($sid in @('S-1-5-18','S-1-5-32-544')){$acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule(([Security.Principal.SecurityIdentifier]$sid),'FullControl','ContainerInherit,ObjectInherit','None','Allow')))}
    if($Reader){$acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule(([Security.Principal.SecurityIdentifier]$Reader),'ReadAndExecute','ContainerInherit,ObjectInherit','None','Allow')))}
    if($Writer){$acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule(([Security.Principal.SecurityIdentifier]$Writer),'FullControl','ContainerInherit,ObjectInherit','None','Allow')))}
    [IO.Directory]::CreateDirectory($Path,$acl)|Out-Null
}
function Launch($Credential,[string]$ChildMode,[string]$Destination,[string]$Other){
    $args=@('-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',('"'+(Join-Path $harness 'probe.ps1')+'"'),'-Mode',$ChildMode,'-Tag',$Tag,'-OutputPath',('"'+$Destination+'"'),'-WriterSid',$WriterSid,'-ClientSid',$ClientSid,'-OtherSid',$Other,'-Case',$Case,'-DocumentHex',$DocumentHex,'-RunHex',$RunHex,'-DigestHex',$DigestHex,'-Instance',$Instance,'-RequestHex',$RequestHex)
    return Start-Process -FilePath "$env:windir\System32\WindowsPowerShell\v1.0\powershell.exe" -ArgumentList $args -Credential $Credential -WorkingDirectory $harness -LoadUserProfile -WindowStyle Hidden -PassThru
}
try{
    Require ((Get-CimInstance Win32_ComputerSystem).Manufacturer -match 'VMware') 'not-vmware'
    $id=[Security.Principal.WindowsIdentity]::GetCurrent();$admin=(New-Object Security.Principal.WindowsPrincipal($id)).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    $report.token=[ordered]@{sid=$id.User.Value;administrator=$admin;groups=@($id.Groups|ForEach-Object {$_.Value})}
    $os=Get-CimInstance Win32_OperatingSystem;$report.os=[ordered]@{version=$os.Version;build=$os.BuildNumber;caption=$os.Caption}
    $report.filesystem=[IO.DriveInfo]::new('C:\').DriveFormat;Require ($report.filesystem -eq 'NTFS') 'not-ntfs'
    $report.reboot_pending=(Test-Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Component Based Servicing\RebootPending') -or (Test-Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\WindowsUpdate\Auto Update\RebootRequired')
    if($Mode -eq 'Inventory'){$report.status='observed';return}
    Add-Type -TypeDefinition @'
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
using System.Diagnostics;
// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
// Disposable fixture startup probe. No production store or IPC implementation.
public sealed class Sn021MatrixStore : IDisposable {
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
    public byte[] RootIdentity,FileIdentity,Digest,DocumentId,RunId;
    public string AllowedSid;
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
    public static void Seal(string leaf,string writer,string client,byte[] document,byte[] run,string path){
        using(var x=new Sn021MatrixStore()){SafeFileHandle store;x.Open(leaf,writer,out store);x.Stage="data";var data=x.Child(store,"data.bin",false);x.ExactAcl(data,writer,false);var content=x.Read(data,4096);
            var b=new byte[224];Buffer.BlockCopy(Encoding.ASCII.GetBytes("SN21ST02"),0,b,0,8);Buffer.BlockCopy(x.RootIdentity,0,b,8,12);Buffer.BlockCopy(Id(x.Inspect(data,false)),0,b,20,12);
            using(var sha=SHA256.Create())Buffer.BlockCopy(sha.ComputeHash(content),0,b,32,32);
            // Administrator-owned setup writes the fixed manifest before writer startup.
            Need(document.Length==16 && run.Length==16,"id-length");
            Buffer.BlockCopy(document,0,b,64,16);Buffer.BlockCopy(run,0,b,80,16);PutSid(b,96,writer);PutSid(b,160,client);
            File.WriteAllBytes(path,b);
        }
    }
    public void Validate(string leaf,string writer){
        try{using(var id=WindowsIdentity.GetCurrent()){Need(id.User.Value==writer,"wrong-writer");Need(!new WindowsPrincipal(id).IsInRole(WindowsBuiltInRole.Administrator),"elevated-writer");}
            SafeFileHandle store;Open(leaf,writer,out store);Stage="manifest";var m=Child(store,"manifest.bin",false);ExactAcl(m,writer,false);byte[] manifest=Read(m,224);
            Need(manifest.Length==224&&Same(Part(manifest,0,8),Encoding.ASCII.GetBytes("SN21ST02")),"manifest-format");Need(Same(RootIdentity,Part(manifest,8,12)),"root-identity");
            Need(ReadSid(manifest,96)==writer,"manifest-writer");AllowedSid=ReadSid(manifest,160);DocumentId=Part(manifest,64,16);RunId=Part(manifest,80,16);
            Stage="data";var d=Child(store,"data.bin",false);ExactAcl(d,writer,false);FileIdentity=Id(Inspect(d,false));Need(Same(FileIdentity,Part(manifest,20,12)),"file-identity");
            byte[] data=Read(d,4096);using(var sha=SHA256.Create())Digest=sha.ComputeHash(data);Need(Same(Digest,Part(manifest,32,32)),"file-bytes");Stage="validated-handles-retained";
        }catch(Exception e){Failure=e is InvalidOperationException?e.Message:e.GetType().FullName;throw;}
    }
    static void PutSid(byte[] b,int offset,string text){var sid=new SecurityIdentifier(text);Need(sid.BinaryLength<=60,"sid-size");b[offset]=(byte)sid.BinaryLength;sid.GetBinaryForm(b,offset+1);}
    static string ReadSid(byte[] b,int offset){int n=b[offset];Need(n>=8&&n<=60,"sid-size");var sid=new SecurityIdentifier(b,offset+1);Need(sid.BinaryLength==n,"sid-encoding");for(int j=offset+1+n;j<offset+64;j++)Need(b[j]==0,"sid-padding");return sid.Value;}
    public void Dispose(){for(int i=handles.Count-1;i>=0;i--)handles[i].Dispose();handles.Clear();}
}
// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
// Manual disposable-VM identity preflight, not the document request protocol.

public static class Sn021MatrixPipe {
    const uint ClientRights=0x120183, FullRights=0x1f01ff;
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

    [DllImport("kernel32.dll",SetLastError=true)] static extern bool SetNamedPipeHandleState(SafeFileHandle h,ref uint mode,IntPtr maximum,IntPtr timeout);
    static string LastCancellation;
    public sealed class Observation {
        public string Failure,Cancellation,RootIdentity,FileIdentity,RequestHex;
        public string[] StoreSecurity;
        public bool HandlesRetained;
        public int RequestBytes,ResponseBytes;
        public long TotalMs;
        public string Status="started", Stage="start", ProcessSid, AuthenticatedSid, Descriptor, ErrorType;
        public int Pid=Process.GetCurrentProcess().Id, NativeError, IdentificationLevel=-1;
        public bool Authorized, Reverted, ReplyReceived;
        public long ElapsedMs;
    }
    static void Require(bool value,string message) { if(!value) throw new InvalidOperationException(message); }
    static void Native(bool ok) { if(!ok) throw new Win32Exception(Marshal.GetLastWin32Error()); }
    static string Sid() { using(var id=WindowsIdentity.GetCurrent()) return id.User.Value; }
    static string Sddl(string writer,string client,string other) {
        string s="O:"+writer+"G:"+writer+"D:P(A;;0x1f01ff;;;"+writer+")(A;;0x120183;;;"+client+")";
        return other==""?s:s+"(A;;0x120183;;;"+other+")";
    }
    static string Verify(SafeFileHandle h,string writer,string client,string other,Observation observation) {
        IntPtr owner,group,dacl,sacl,sd;
        uint error=GetSecurityInfo(h,1,5,out owner,out group,out dacl,out sacl,out sd);
        if(error!=0) throw new Win32Exception((int)error);
        try {
            byte[] b=new byte[GetSecurityDescriptorLength(sd)];Marshal.Copy(sd,b,0,b.Length);
            var descriptor=new RawSecurityDescriptor(b,0);observation.Descriptor=descriptor.GetSddlForm(AccessControlSections.Owner|AccessControlSections.Access);
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
        if(clock.ElapsedMilliseconds>=deadline)throw new TimeoutException("io-deadline-before-issue");
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
                    LastCancellation="cancel="+cancelError+";completion="+completionError;
                    throw new TimeoutException("io-deadline;"+LastCancellation);
                }
                Native(GetOverlappedResult(h,ov,out n,false));
            } else if(operation!=0) Native(GetOverlappedResult(h,ov,out n,false));
            if(operation==1)Marshal.Copy(buffer,bytes,0,bytes.Length);
            return n;
        } finally { Marshal.FreeHGlobal(buffer);Marshal.FreeHGlobal(ov);CloseHandle(ev); }
    }
    static SafeFileHandle Create(string name,string writer,string client,string other,bool extraRights=false) {
        var sd=new RawSecurityDescriptor(extraRights?Sddl(writer,client,other).Replace("0x120183","0x12018b"):Sddl(writer,client,other));byte[] bytes=new byte[sd.BinaryLength];sd.GetBinaryForm(bytes,0);
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
    public static byte[] Frame(byte[] document,byte[] run) {
        Require(document.Length==16 && run.Length==16,"id-length");
        byte[] b=new byte[64];Buffer.BlockCopy(System.Text.Encoding.ASCII.GetBytes("SN21IPC1"),0,b,0,8);
        b[8]=2;b[10]=1;b[12]=32;Buffer.BlockCopy(Guid.NewGuid().ToByteArray(),0,b,16,16);
        Buffer.BlockCopy(document,0,b,32,16);Buffer.BlockCopy(run,0,b,48,16);return b;
    }
    static bool Match(byte[] a,int offset,byte[] b) {if(offset+b.Length>a.Length)return false;for(int i=0;i<b.Length;i++)if(a[offset+i]!=b[i])return false;return true;}
    public static string FrameError(byte[] b,int length,byte[] document,byte[] run) {
        if(length!=64 || b.Length<length)return "request-size";
        if(!Match(b,0,System.Text.Encoding.ASCII.GetBytes("SN21IPC1")))return "request-magic";
        if(b[8]!=2 || b[9]!=0)return "request-version";
        if(b[10]!=1 || b[11]!=0)return "request-operation";
        if(b[12]!=32 || b[13]!=0 || b[14]!=0 || b[15]!=0)return "request-body-length";
        if(!Match(b,32,document))return "request-document";
        if(!Match(b,48,run))return "request-run";
        return "";
    }
    static byte[] Request(string test,byte[] document,byte[] run) {
        byte[] b=Frame(document,run);
        switch(test) {
            case "magic":b[0]^=1;break;
            case "version":b[8]=3;break;
            case "length":b[12]=255;b[13]=255;b[14]=255;b[15]=255;break;
            case "save":b[10]=2;break;
            case "document":b[32]^=1;break;
            case "run":b[48]^=1;break;
            case "path":Buffer.BlockCopy(System.Text.Encoding.ASCII.GetBytes("C:\\Windows\\file"),0,b,32,15);break;
            case "short":Array.Resize(ref b,63);break;
            case "oversized":Array.Resize(ref b,66);break;
        }return b;
    }
    static void ClientAcknowledged(SafeFileHandle h,Stopwatch clock,byte marker=0x7e) {
        var b=new byte[65];uint n=Io(h,1,b,clock,5000);
        Require(n==1 && b[0]==marker,"extra-request-or-invalid-ack");
    }
    static void ServerClosed(SafeFileHandle h,Stopwatch clock) {
        try {Io(h,1,new byte[65],clock,5000);throw new InvalidOperationException("extra-response");}
        catch(Win32Exception e){if(e.NativeErrorCode!=109 && e.NativeErrorCode!=232)throw;}
    }
    public static Observation RunQuery(string role,string pipe,string writer,string client,string other,string ready,string leaf,string test,byte[] document,byte[] run,byte[] expectedDigest,byte[] replay) {
        var o=new Observation();var total=Stopwatch.StartNew();var clock=Stopwatch.StartNew();o.ProcessSid=Sid();LastCancellation=null;
        try {
            using(var identity=WindowsIdentity.GetCurrent()) {
                Require(!new WindowsPrincipal(identity).IsInRole(WindowsBuiltInRole.Administrator),"elevated-process");
                foreach(var group in identity.Groups)Require(group.Value!="S-1-5-32-544","administrator-member");
            }
            if(role=="server") {
                Require(o.ProcessSid==writer,"wrong-writer-token");o.Stage="startup";
                using(var store=new Sn021MatrixStore()) {
                    store.Validate(leaf,writer);o.StoreSecurity=store.Security.ToArray();
                    o.RootIdentity=Convert.ToBase64String(store.RootIdentity);o.FileIdentity=Convert.ToBase64String(store.FileIdentity);
                    Require(store.AllowedSid==client,"setup-client-mismatch");
                    o.HandlesRetained=true;o.Stage="create-pipe";
                    using(var h=Create(pipe,writer,client,other)) {
                        o.Descriptor=Verify(h,writer,client,other,o);File.WriteAllText(ready,"ready");
                        o.Stage="connect";Io(h,0,new byte[1],clock,15000);
                        clock.Restart();o.Stage="read";var request=new byte[65];uint n=Io(h,1,request,clock,5000);o.RequestBytes=(int)n;o.RequestHex=BitConverter.ToString(request,0,(int)n).Replace("-","").ToLowerInvariant();
                        o.Stage="identify";Identify(h,o,store.AllowedSid);bool identityAllowed=o.Authorized;o.Authorized=false;
                        o.Stage="authorize";Require(identityAllowed,"unauthorized-document-principal");
                        o.Stage="frame";string failure=FrameError(request,(int)n,store.DocumentId,store.RunId);Require(failure=="",failure);
                        var reply=new byte[64];Buffer.BlockCopy(request,0,reply,0,32);reply[10]=1;reply[11]=128;Buffer.BlockCopy(store.Digest,0,reply,32,32);
                        o.Stage="reply";
                        switch(test) {
                            case "rmagic":reply[0]^=1;break;
                            case "rversion":reply[8]=3;break;
                            case "rlength":reply[12]=255;break;
                            case "rcorrelation":reply[16]^=1;break;
                            case "rdigest":reply[32]^=1;break;
                            case "rshort":Array.Resize(ref reply,63);break;
                            case "roversized":Array.Resize(ref reply,66);break;
                        }
                        if(test=="rdribble") {
                            for(int i=0;i<6;i++){Io(h,2,new byte[]{reply[i]},clock,5000);o.ResponseBytes++;System.Threading.Thread.Sleep(1000);}
                        } else {Require(Io(h,2,reply,clock,5000)==reply.Length,"short-reply-write");o.ResponseBytes=reply.Length;}
                        if(test=="rduplicate")Io(h,2,reply,clock,5000);
                        o.Stage="ack";ClientAcknowledged(h,clock);
                        if(test=="rduplicate")ServerClosed(h,clock);
                        if(test=="rstall")System.Threading.Thread.Sleep(6000);
                        if(test=="rclose"){o.Status="injected-close";return o;}
                        o.Stage="terminal";Require(Io(h,2,new byte[]{test=="rterminal"?(byte)0x7c:(byte)0x7f},clock,5000)==1,"short-terminal-write");
                        o.Stage="terminal-ack";ClientAcknowledged(h,clock,0x7f);
                        if(test=="rafterterminal"){Io(h,2,new byte[]{42},clock,5000);ServerClosed(h,clock);}
                        o.Authorized=true;o.Status="query-served";
                    }
                    // Startup handles are still held at this point, including on request rejection.
                }
            } else {
                o.Stage="open-pipe";
                using(var h=CreateFileW(pipe,ClientRights,0,IntPtr.Zero,3,test=="anonymous"?0x40100000u:0x40110000u,IntPtr.Zero)) {
                    if(h.IsInvalid)throw new Win32Exception(Marshal.GetLastWin32Error());
                    o.Stage="verify-server";o.Descriptor=Verify(h,writer,client,other,o);
                    uint mode=2;Native(SetNamedPipeHandleState(h,ref mode,IntPtr.Zero,IntPtr.Zero));
                    clock.Restart();o.Stage="send";
                    if(test=="disconnect"){o.Status="client-disconnected";return o;}
                    if(test=="slow")System.Threading.Thread.Sleep(6000);
                    byte[] request=replay.Length==0?Request(test,document,run):replay;Require(request.Length==64,"replay-size");o.RequestHex=BitConverter.ToString(request).Replace("-","").ToLowerInvariant();if(test=="dribble") {
                        for(int i=0;i<6;i++){Io(h,2,new byte[]{request[i]},clock,5000);o.RequestBytes++;System.Threading.Thread.Sleep(1000);}
                    } else o.RequestBytes=(int)Io(h,2,request,clock,5000);
                    if(test=="extra")Io(h,2,new byte[]{1},clock,5000);
                    o.Stage="receive";var reply=new byte[65];uint n=Io(h,1,reply,clock,5000);o.ResponseBytes=(int)n;
                    Require(n==64,"response-size");
                    Require(Match(reply,0,System.Text.Encoding.ASCII.GetBytes("SN21IPC1")) && reply[8]==2 && reply[9]==0 && reply[10]==1 && reply[11]==128 && reply[12]==32 && reply[13]==0 && reply[14]==0 && reply[15]==0,"response-header");
                    var correlation=new byte[16];Buffer.BlockCopy(request,16,correlation,0,16);Require(Match(reply,16,correlation),"response-correlation");Require(Match(reply,32,expectedDigest),"response-digest");
                    o.Stage="ack";Require(Io(h,2,new byte[]{0x7e},clock,5000)==1,"short-ack-write");
                    o.Stage="terminal";var terminal=new byte[65];uint count=Io(h,1,terminal,clock,5000);
                    Require(count==1 && terminal[0]==0x7f,"extra-response-or-invalid-terminal");
                    o.Stage="terminal-ack";Require(Io(h,2,new byte[]{0x7f},clock,5000)==1,"short-terminal-ack-write");
                    o.Stage="response-end";ServerClosed(h,clock);
                    o.ReplyReceived=true;o.Authorized=true;o.Status="query-received";
                    // Accept only after the acknowledgement and observed server closure; no unbounded flush.
                }
            }
        } catch(Exception e) {o.Status="failed";o.ErrorType=e.GetType().FullName;var w=e as Win32Exception;if(w!=null)o.NativeError=w.NativeErrorCode;if(e is InvalidOperationException || e is TimeoutException)o.Failure=e.Message;}
        finally{o.ElapsedMs=clock.ElapsedMilliseconds;o.TotalMs=total.ElapsedMilliseconds;o.Cancellation=LastCancellation;}
        return o;
    }

}

'@
    if($Mode -in @('Server','Client','Listener')){
        Require (-not $admin -and $report.token.groups -notcontains 'S-1-5-32-544') 'elevated-child'
        $privileges=@(& "$env:windir\System32\whoami.exe" /priv /fo csv /nh | ConvertFrom-Csv -Header Name,Description,State)
        Require ($LASTEXITCODE -eq 0 -and $privileges.Count -gt 0) 'privilege-inventory-failed'
        $report.privileges=@($privileges|Select-Object Name,State)
        foreach($p in $privileges){Require ($p.Name -notmatch '^Se(TakeOwnership|Restore|Backup|Debug|Tcb|Impersonate|AssignPrimaryToken|CreateToken)Privilege$') 'privileged-child'}
        $pipe="\\.\pipe\SN021Matrix-$Tag-$Case";$ready=Join-Path $harness "writer\$Instance.ready"
        [byte[]]$replay=@()
        $report.result=[Sn021MatrixPipe]::RunQuery($Mode.ToLowerInvariant(),$pipe,$WriterSid,$ClientSid,'',$ready,"SN021Matrix-$Tag-$Case",$Case,(Bytes $DocumentHex),(Bytes $RunHex),(Bytes $DigestHex),$replay)
        $report.status='observed';return
    }
    $probeClock=[Diagnostics.Stopwatch]::StartNew()
    Require ($Mode -eq 'Probe' -and $admin) 'setup-requires-administrator';Require (-not $report.reboot_pending) 'restart-pending'
    Require (-not (Test-Path $harness)) 'harness-exists'
    $credentials=@{};$sids=@{}
    foreach($role in @('w','c','u')){
        $name='snm'+$role+$Tag.Substring(0,10);Require (-not (Get-LocalUser -Name $name -ErrorAction SilentlyContinue)) 'account-exists'
        $password=ConvertTo-SecureString ('Sn!9'+[guid]::NewGuid().ToString('N')) -AsPlainText -Force
        New-LocalUser -Name $name -Password $password -Description 'Disposable SN-021 endpoint fixture'|Out-Null
        Add-LocalGroupMember -Group (Get-LocalGroup -SID 'S-1-5-32-545') -Member $name
        $credentials[$role]=New-Object Management.Automation.PSCredential("$env:COMPUTERNAME\$name",$password);$sids[$role]=(Get-LocalUser -Name $name).SID.Value
    }
    $WriterSid=$sids.w;$ClientSid=$sids.c;$OtherSid=$sids.u;$report.principals=$sids
    Protect $harness 'S-1-5-32-545';Protect (Join-Path $harness 'writer') 'S-1-5-32-545' $sids.w
    Protect (Join-Path $harness 'client') '' $sids.c;Protect (Join-Path $harness 'unauthorized') 'S-1-5-32-545' $sids.u
    Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $harness 'probe.ps1')
    function Start-Child([string]$Role,[string]$ChildMode,[string]$Label){
        Require ($probeClock.ElapsedMilliseconds -lt 180000) 'run-deadline-left-for-inspection'
        $Instance=$Label;$directory=@{w='writer';c='client';u='unauthorized'}[$Role]
        $output=Join-Path $harness "$directory\$Label.json";$ready=Join-Path $harness "$directory\$Label.ready"
        $process=Launch $credentials[$Role] $ChildMode $output $OtherSid
        return @{process=$process;output=$output;ready=$ready;role=$Role;label=$Label}
    }
    function Await-Ready($Child){
        $wait=[Diagnostics.Stopwatch]::StartNew()
        while(-not (Test-Path $Child.ready) -and -not $Child.process.HasExited -and $wait.ElapsedMilliseconds -lt 20000 -and $probeClock.ElapsedMilliseconds -lt 180000){Start-Sleep -Milliseconds 100}
        if(-not (Test-Path $Child.ready)){
            if(Test-Path $Child.output){$report.observations+=[ordered]@{case=$Child.label;partial=(Get-Content $Child.output -Raw|ConvertFrom-Json)};Save-Report}
            throw 'PROBE:server-not-ready'
        }
    }
    function Finish($Child){
        $remaining=[Math]::Min(30000,180000-$probeClock.ElapsedMilliseconds)
        Require ($remaining -gt 0) 'run-deadline-left-for-inspection'
        Require ($Child.process.WaitForExit([int]$remaining)) 'child-timeout-left-for-inspection'
        Require (Test-Path $Child.output) 'missing-child-report'
        $v=Get-Content $Child.output -Raw|ConvertFrom-Json
        Require ($v.status -eq 'observed' -and $v.token.sid -eq $sids[$Child.role]) 'child-harness-or-sid'
        return $v
    }
    function Snapshot {
        $items=[ordered]@{}
        foreach($path in @($root,$store,$data,$manifest,(Join-Path $store 'preserved.bin'),(Join-Path $root 'alias.bin'))){
            if(Test-Path -LiteralPath $path){
                $item=Get-Item -LiteralPath $path
                $items[$path]=[ordered]@{attributes=[int]$item.Attributes;security=(Get-Acl -LiteralPath $path).Sddl}
                if(-not $item.PSIsContainer){$items[$path].hash=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
            }else{$items[$path]='absent'}
        }
        return ($items|ConvertTo-Json -Depth 6 -Compress)
    }
    $startup=[ordered]@{bytes='file-bytes';identity='file-identity';missing='nt-open:c0000034';hardlink='hardlink-alias';owner='wrong-owner';rights='wrong-ace-count';manifest='byte-limit';junction='reparse-or-offline';rootrights='wrong-ace-count'}
    $replyFailures=@{rmagic='response-header';rversion='response-header';rlength='response-header';rcorrelation='response-correlation';rdigest='response-digest';rshort='response-size';rdribble='response-size';rduplicate='extra-response-or-invalid-terminal';rterminal='extra-response-or-invalid-terminal';rafterterminal='extra-response'}
    $cases=@('goodone','goodtwo','rmagic','rversion','rlength','rcorrelation','rdigest','rshort','roversized','rduplicate','rterminal','rafterterminal','rclose','rstall','dribble','rdribble','extra')+@($startup.Keys)
    foreach($Case in $cases){
        $report.stage=$Case;Save-Report
        $leaf="SN021Matrix-$Tag-$Case";$root="C:\$leaf";$store=Join-Path $root 'store'
        Require (-not (Test-Path $root)) 'fixture-exists';Protect $root 'S-1-5-32-545';Protect $store $WriterSid
        $data=Join-Path $store 'data.bin';$manifest=Join-Path $store 'manifest.bin';[IO.File]::WriteAllText($data,'query fixture',$utf8)
        $DocumentHex=[guid]::NewGuid().ToString('N');$RunHex=[guid]::NewGuid().ToString('N');$DigestHex=(Get-FileHash $data -Algorithm SHA256).Hash.ToLowerInvariant()
        [Sn021MatrixStore]::Seal($leaf,$WriterSid,$ClientSid,(Bytes $DocumentHex),(Bytes $RunHex),$manifest)
        $original=Snapshot
        switch($Case){
            'bytes' {[IO.File]::WriteAllText($data,'other fixture',$utf8)}
            'identity' {$replacement=Join-Path $store 'replacement.bin';[IO.File]::WriteAllText($replacement,'query fixture',$utf8);[IO.File]::Replace($replacement,$data,[System.Management.Automation.Language.NullString]::Value)}
            'missing' {[IO.File]::Move($data,(Join-Path $store 'preserved.bin'))}
            'hardlink' {New-Item -ItemType HardLink -Path (Join-Path $root 'alias.bin') -Target $data|Out-Null}
            'owner' {$acl=Get-Acl $data;$acl.SetOwner([Security.Principal.SecurityIdentifier]$WriterSid);Set-Acl $data $acl}
            'rights' {$acl=Get-Acl $data;$acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule(([Security.Principal.SecurityIdentifier]'S-1-5-32-545'),'Write','Allow')));Set-Acl $data $acl}
            'manifest' {$b=[IO.File]::ReadAllBytes($manifest);[IO.File]::WriteAllBytes($manifest,($b+[byte]0))}
            'junction' {$originalStore=Join-Path $root 'original-store';[IO.Directory]::Move($store,$originalStore);New-Item -ItemType Junction -Path $store -Target $originalStore|Out-Null}
            'rootrights' {$acl=Get-Acl $root;$acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule(([Security.Principal.SecurityIdentifier]$WriterSid),'Write','Allow')));Set-Acl $root $acl}
        }
        $before=Snapshot;$RequestHex='none';$clientReport=$null
        $server=Start-Child 'w' 'Server' ($Case+'server')
        if(-not $startup.Contains($Case)){
            Await-Ready $server;$client=Start-Child 'c' 'Client' ($Case+'client');$clientReport=Finish $client
        }
        $sv=Finish $server;$after=Snapshot
        $report.observations+=[ordered]@{case=$Case;original=$original;before=$before;after=$after;preserved=($before -eq $after);endpoint_ready=(Test-Path $server.ready);server=$sv;client=$clientReport};Save-Report
        Require ($before -eq $after) 'fixture-changed-during-case'
        $s=$sv.result;$c=if($clientReport){$clientReport.result}else{$null}
        if($startup.Contains($Case)){
            Require (-not (Test-Path $server.ready) -and $s.Stage -eq 'startup' -and $s.Failure -eq $startup[$Case]) 'startup-rejection-inconclusive'
        }elseif($Case -in @('goodone','goodtwo')){
            Require ($s.Status -eq 'query-served' -and $c.Status -eq 'query-received' -and $c.Stage -eq 'response-end' -and $s.HandlesRetained -and $s.Reverted) 'good-v2-query-failed'
        }elseif($replyFailures.ContainsKey($Case)){
            $stage=if($Case -in @('rduplicate','rterminal')){'terminal'}elseif($Case -eq 'rafterterminal'){'response-end'}else{'receive'}
            Require ($c.Stage -eq $stage -and $c.Failure -eq $replyFailures[$Case] -and -not $c.ReplyReceived) 'response-refusal-inconclusive'
        }elseif($Case -eq 'roversized'){
            Require ($c.Stage -eq 'receive' -and $c.NativeError -eq 234 -and -not $c.ReplyReceived) 'oversize-response-inconclusive'
        }elseif($Case -eq 'rclose'){
            Require ($c.Stage -eq 'terminal' -and $c.NativeError -in @(109,232) -and -not $c.ReplyReceived -and $s.Status -eq 'injected-close') 'premature-close-inconclusive'
        }elseif($Case -eq 'rstall'){
            Require ($c.Stage -eq 'terminal' -and $c.ErrorType -eq 'System.TimeoutException' -and $c.Cancellation -and $c.ElapsedMs -ge 4500 -and $c.ElapsedMs -lt 10000 -and -not $c.ReplyReceived) 'terminal-deadline-inconclusive'
        }elseif($Case -eq 'dribble'){
            Require ($s.Stage -eq 'frame' -and $s.Failure -eq 'request-size' -and $s.RequestBytes -eq 1 -and -not $c.ReplyReceived) 'dribble-request-inconclusive'
        }elseif($Case -eq 'extra'){
            Require ($s.Stage -eq 'ack' -and $s.Failure -eq 'extra-request-or-invalid-ack' -and -not $c.ReplyReceived) 'extra-request-inconclusive'
        }else{throw 'PROBE:unhandled-case'}
    }
    $report.elapsed_ms=$probeClock.ElapsedMilliseconds
    Require ($report.elapsed_ms -lt 180000) 'run-deadline-left-for-inspection'
    $report.status='observed-remaining-request-matrix-only';$report.stage='complete'
    $report.limits=@('Private v2 acknowledgement and terminal closure, no production protocol','No crash recovery, version/ABA or saving','Wrong responses are controlled injections under the trusted writer SID','Small messages are refused immediately, not accumulated as a byte stream','Historical v1 evidence is not automatically v2 regression coverage')
}catch{
    $report.status='failed';$report.error_type=$_.Exception.GetType().FullName;$report.error_line=$_.InvocationInfo.ScriptLineNumber
    if($_.Exception.Message.StartsWith('PROBE:')){$report.failure=$_.Exception.Message}
    $e=$_.Exception;while($e.InnerException){$e=$e.InnerException};if($e -is [ComponentModel.Win32Exception]){$report.native_error=$e.NativeErrorCode};if($e -is [InvalidOperationException]){$report.detail=$e.Message}
}finally{Save-Report}
