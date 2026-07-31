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
SetupIconFile=Assets\wotlk-icon.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
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
const
  // The x64 VC++ 2015-2022 runtime records itself in the true 64-bit
  // registry hive at this path (no "WOW6432Node" segment). "X86" is the
  // sibling key the 32-bit redist writes; we only care about X64 here.
  VCRedistRegPath = 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\X64';
  // Defensive fallback location, checked in case some install ever mirrors
  // the key under WOW6432Node; see IsVCRedistX64Installed for why this is
  // secondary, not primary.
  VCRedistRegPathWow6432 = 'SOFTWARE\WOW6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\X64';
  VCRedistDownloadUrl = 'https://aka.ms/vs/17/release/vc_redist.x64.exe';
  VCRedistFileName = 'vc_redist.x64.exe';

var
  DownloadPage: TDownloadWizardPage;
  VCRedistInstallRequired: Boolean;
  VCRedistFailed: Boolean;

// Detects the Microsoft Visual C++ 2015-2022 Redistributable (x64).
//
// Inno Setup's own setup.exe is compiled 32-bit by default. A 32-bit process
// reading HKLM\SOFTWARE\Microsoft\... gets transparently redirected by
// WOW64 to HKLM\SOFTWARE\WOW6432Node\Microsoft\..., which is where the
// *x86* redistributable's key lives, not the x64 one. Reading the native
// path via plain HKLM here is the classic false-negative trap: the key
// legitimately exists on the machine, but the redirected read never sees
// it. HKLM64 forces Inno Setup to use the real 64-bit registry view
// (bypasses WOW64 redirection), which is where the x64 redistributable
// actually installs to, so that's the primary check.
function IsVCRedistX64Installed(): Boolean;
var
  Installed: Cardinal;
begin
  Result := False;

  if RegQueryDWordValue(HKLM64, VCRedistRegPath, 'Installed', Installed) then
  begin
    Result := Installed = 1;
    Exit;
  end;

  // Fallback only: not the expected location for the x64 key, but checked
  // so an unusual layout doesn't cause an unnecessary download/install.
  if RegQueryDWordValue(HKLM, VCRedistRegPathWow6432, 'Installed', Installed) then
  begin
    Result := Installed = 1;
    Exit;
  end;
end;

procedure InitializeWizard;
begin
  VCRedistInstallRequired := not IsVCRedistX64Installed();
  if VCRedistInstallRequired then
    DownloadPage := CreateDownloadPage(SetupMessage(msgWizardPreparing),
      'AzerothCore needs the Microsoft Visual C++ Redistributable (x64) to run. Downloading it now...',
      nil);
end;

// Downloads vc_redist.x64.exe to {tmp} while the wizard is on the "Ready to
// Install" page, using Inno Setup's built-in download page so the user sees
// real progress instead of a frozen window. A failed/aborted download does
// not stop the installer -- VCRedistFailed is recorded and surfaced once,
// after the app files are in place, in CurStepChanged below.
function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = wpReady) and VCRedistInstallRequired and (not VCRedistFailed) then
  begin
    DownloadPage.Clear;
    DownloadPage.Add(VCRedistDownloadUrl, VCRedistFileName, '');
    DownloadPage.Show;
    try
      try
        DownloadPage.Download;
      except
        VCRedistFailed := True;
        if DownloadPage.AbortedByUser then
          Log('VC++ Redistributable download aborted by user.')
        else
          Log('VC++ Redistributable download failed: ' + GetExceptionMessage);
      end;
    finally
      DownloadPage.Hide;
    end;
  end;
end;

// Runs the downloaded redistributable installer silently, right before the
// app's own files are copied. Exit codes 0 (success), 3010 (success, reboot
// wanted -- suppressed by /norestart) and 1638 (a newer runtime is already
// present) all count as success.
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ResultCode: Integer;
  Ran: Boolean;
begin
  Result := '';
  if VCRedistInstallRequired and (not VCRedistFailed) then
  begin
    Ran := Exec(ExpandConstant('{tmp}\' + VCRedistFileName), '/install /quiet /norestart', '',
      SW_SHOW, ewWaitUntilTerminated, ResultCode);
    if (not Ran) then
    begin
      VCRedistFailed := True;
      Log('Failed to launch VC++ Redistributable installer.');
    end
    else if (ResultCode <> 0) and (ResultCode <> 3010) and (ResultCode <> 1638) then
    begin
      VCRedistFailed := True;
      Log('VC++ Redistributable installer exited with code ' + IntToStr(ResultCode));
    end;
  end;
end;

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

    if VCRedistInstallRequired and VCRedistFailed then
      SuppressibleMsgBox(
        'AzerothCore could not automatically install the Microsoft Visual C++ Redistributable (x64), which the app needs in order to run.' + #13#10 + #13#10 +
        'Please install it manually before launching AzerothCore, from:' + #13#10 +
        VCRedistDownloadUrl,
        mbInformation, MB_OK, IDOK);
  end;
end;
