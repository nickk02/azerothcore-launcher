#define MyAppName "AzerothCore"
; Version comes from the release tag when one is building a tagged release
; (ISCC /DMyAppVersion=2026.08.06), and falls back to the build date otherwise
; so a local build still gets a distinct, meaningful version with no manual
; bump. The ifndef matters: an unconditional #define would silently overwrite
; the command-line value, and a rebuild of an old tag would be stamped with
; today's date instead of that tag's.
#ifndef MyAppVersion
  #define MyAppVersion GetDateTimeString('yyyy.mm.dd', '', '')
#endif
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
DefaultDirName={%USERPROFILE}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
SetupIconFile=..\assets\icon.ico
UninstallDisplayIcon={app}\App\{#MyAppExeName}
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
; The app is a single statically linked exe, so there is no runtime to bundle
; and no Visual C++ redistributable to install.
Source: "..\target\release\azerothcore.exe"; DestDir: "{app}\App"; Flags: ignoreversion

[Dirs]
; Empty client folder for the user's own, legally-obtained WotLK 3.3.5a install.
; Nothing from Blizzard's client is bundled or downloaded by this installer.
Name: "{app}\Client"

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\App\{#MyAppExeName}"
Name: "{group}\Client Folder"; Filename: "{app}\Client"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\App\{#MyAppExeName}"; Tasks: desktopicon

[Run]
; The Client folder opens first, deliberately. Installing this and then being
; dropped straight into the launcher with no client is the one confusing moment
; in the whole flow -- nothing on screen says a client is needed or where to
; put it. Opening the empty folder (which contains the README written in
; CurStepChanged) makes the next step self-evident. Both entries are ticked by
; default, so the normal finish is: folder opens, launcher opens behind it.
Filename: "{app}\Client"; Description: "Open the Client folder"; Flags: shellexec postinstall skipifsilent
Filename: "{app}\App\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent

[Code]
// Replaces Inno's stock "Setup has finished installing" text with the one
// instruction that actually matters.
procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = wpFinished then
    WizardForm.FinishedLabel.Caption :=
      ExpandConstant('{#MyAppName} is installed, but it has no game client yet.') + #13#10 + #13#10 +
      ExpandConstant('Copy your own, legally-obtained WotLK 3.3.5a client files into:') + #13#10 +
      ExpandConstant('    {app}\Client') + #13#10 + #13#10 +
      ExpandConstant('Then open {#MyAppName} and use Browse to point it at the Wow.exe in that folder.');
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  Lines: TArrayOfString;
begin
  if CurStep = ssPostInstall then
  begin
    SetArrayLength(Lines, 6);
    Lines[0] := 'This folder is where your own WotLK 3.3.5a client goes.';
    Lines[1] := '';
    Lines[2] := 'Copy your existing, legally-obtained 3.3.5a client files into this';
    Lines[3] := 'folder (the folder containing Wow.exe), then open AzerothCore and';
    Lines[4] := 'use the Browse button to point it at the Wow.exe in here.';
    Lines[5] := '';
    SaveStringsToFile(ExpandConstant('{app}\Client\README.txt'), Lines, False);
  end;
end;
