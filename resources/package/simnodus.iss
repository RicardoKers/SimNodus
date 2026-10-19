; SimNodus Windows installer (SN-152), built by tools/package_windows.py --installer
; with Inno Setup 6 from the same staging folder as the portable zip.
; ISCC defines: AppVersion (0.5.0-dev), NumericVersion (0.5.0), SourceDir (the
; staged package), OutputDir and OutputBaseName.
;
; Installs for the current user without administrator rights by default; an
; administrator can choose "all users" in the first dialog (classroom PCs).
; Opening a project never runs code, so the .snx association is safe.

#ifndef AppVersion
  #error Pass /DAppVersion=..., /DNumericVersion=..., /DSourceDir=..., /DOutputDir=..., /DOutputBaseName=...
#endif

[Setup]
AppId={{6F1D2C7A-58B4-4C1E-9A3B-5E2D7C9F0A41}
AppName=SimNodus
AppVersion={#AppVersion}
AppVerName=SimNodus {#AppVersion}
AppPublisher=Ricardo Kerschbaumer
AppPublisherURL=https://github.com/RicardoKers/SimNodus
AppSupportURL=https://github.com/RicardoKers/SimNodus/issues
VersionInfoVersion={#NumericVersion}
DefaultDirName={autopf}\SimNodus
DefaultGroupName=SimNodus
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
LicenseFile={#SourceDir}\LICENSE
SetupIconFile=..\icons\simnodus.ico
UninstallDisplayIcon={app}\simnodus.exe
OutputDir={#OutputDir}
OutputBaseFilename={#OutputBaseName}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ChangesAssociations=yes
CloseApplications=yes

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "ptbr"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[CustomMessages]
en.AssociateSnx=Open SimNodus projects (.snx) with SimNodus
ptbr.AssociateSnx=Abrir projetos do SimNodus (.snx) com o SimNodus
en.ProjectType=SimNodus project
ptbr.ProjectType=Projeto do SimNodus

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "associate"; Description: "{cm:AssociateSnx}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\SimNodus"; Filename: "{app}\simnodus.exe"
Name: "{group}\{cm:UninstallProgram,SimNodus}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\SimNodus"; Filename: "{app}\simnodus.exe"; Tasks: desktopicon

[Registry]
; HKA is the current user's classes for a per-user install, the machine's for all users.
Root: HKA; Subkey: "Software\Classes\.snx"; ValueType: string; ValueName: ""; ValueData: "SimNodus.Project"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\SimNodus.Project"; ValueType: string; ValueName: ""; ValueData: "{cm:ProjectType}"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\SimNodus.Project\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\simnodus.exe,0"; Tasks: associate
Root: HKA; Subkey: "Software\Classes\SimNodus.Project\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\simnodus.exe"" ""%1"""; Tasks: associate

[Run]
Filename: "{app}\simnodus.exe"; Description: "{cm:LaunchProgram,SimNodus}"; Flags: nowait postinstall skipifsilent
