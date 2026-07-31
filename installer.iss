#define MyAppName "AzerothCore"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "AzerothCore"
#define MyAppExeName "azerothcore.exe"

[Setup]
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppCopyright=Copyright (C) 2026 {#MyAppPublisher}
VersionInfoVersion={#MyAppVersion}
VersionInfoCompany={#MyAppPublisher}
VersionInfoProductName={#MyAppName}
VersionInfoDescription={#MyAppName} Setup
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir=dist
OutputBaseFilename=AzerothCoreSetup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
; The project is built with WindowsAppSDKSelfContained=true, so the Release
; output directory is a full self-contained WinUI3 deployment: the app exe,
; its own compiled XAML (.xbf) and WinRT metadata (.winmd), every
; WindowsAppRuntime/WinUI DLL and .winmd, WebView2's loader, and one
; MUI resource subfolder per locale (~250 files, ~120 MB). There is no
; "azerothcore.pri" (the brief's assumed name) -- the real resource-index
; files are Microsoft.UI.pri, Microsoft.UI.Xaml.Controls.pri, and
; Microsoft.WindowsAppRuntime.pri. Rather than hand-list an itemized set
; that would silently go stale or omit a required runtime DLL/locale folder,
; copy the whole output tree and exclude only the dev-only build byproducts
; (debug symbols and the import lib/exports from linking the .exe).
Source: "x64\Release\azerothcore\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "*.pdb,*.lib,*.exp"

[Dirs]
; Empty client folder for the user's own, legally-obtained WotLK 3.3.5a install.
; Nothing from Blizzard's client is bundled or downloaded by this installer.
Name: "{app}\Client"

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Client Folder"; Filename: "{app}\Client"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent

[Code]
procedure CurStepChanged(CurStep: TSetupStep);
var
  ReadmePath: string;
  Lines: TArrayOfString;
begin
  if CurStep = ssPostInstall then
  begin
    ReadmePath := ExpandConstant('{app}\Client\README.txt');
    SetArrayLength(Lines, 6);
    Lines[0] := 'This folder is where your own WotLK 3.3.5a client goes.';
    Lines[1] := '';
    Lines[2] := 'Copy your existing, legally-obtained 3.3.5a client files into this';
    Lines[3] := 'folder (the folder containing Wow.exe), then open AzerothCore and';
    Lines[4] := 'use Settings to browse to Wow.exe here.';
    Lines[5] := '';
    SaveStringsToFile(ReadmePath, Lines, False);
  end;
end;
