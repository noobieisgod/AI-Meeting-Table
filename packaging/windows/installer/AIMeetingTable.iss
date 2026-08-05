#define MyAppId "{{0A1D4854-7B48-49D5-85ED-59826C26B35A}"
#define MyAppName "AI Meeting Table"
#define MyAppExeName "AIMeetingTable.exe"
#define MyAppPublisher "AI Meeting Table"

#ifndef AppVersion
  #error AppVersion define is required
#endif
#ifndef RepoRoot
  #error RepoRoot define is required
#endif
#ifndef ReleaseDir
  #error ReleaseDir define is required
#endif
#ifndef OutputDir
  #error OutputDir define is required
#endif
#ifndef SourceUrl
  #define SourceUrl "https://github.com/noobieisgod/AI-Meeting-Table"
#endif

[Setup]
AppId={#MyAppId}
AppName={#MyAppName}
AppVersion={#AppVersion}
AppVerName={#MyAppName} {#AppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#SourceUrl}
AppSupportURL={#SourceUrl}
AppUpdatesURL={#SourceUrl}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
DisableReadyPage=yes
LicenseFile={#RepoRoot}\LICENSE
OutputDir={#OutputDir}
OutputBaseFilename=AI-Meeting-Table-{#AppVersion}-setup
SetupIconFile={#RepoRoot}\apps\windows\app_icon.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
VersionInfoVersion={#AppVersion}
VersionInfoProductName={#MyAppName}
VersionInfoDescription={#MyAppName} Installer
VersionInfoCompany={#MyAppPublisher}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#ReleaseDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#RepoRoot}\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "{#RepoRoot}\packaging\windows\installer\LEGAL-NOTICES.txt"; DestDir: "{app}"; DestName: "LEGAL-NOTICES.txt"; Flags: ignoreversion

[Icons]
Name: "{group}\AI Meeting Table"; Filename: "{app}\{#MyAppExeName}"; Check: ShouldCreateStartMenuShortcut
Name: "{group}\License and Source Code"; Filename: "{app}\LEGAL-NOTICES.txt"; Check: ShouldCreateStartMenuShortcut
Name: "{group}\Uninstall AI Meeting Table"; Filename: "{uninstallexe}"; Check: ShouldCreateStartMenuShortcut
Name: "{autodesktop}\AI Meeting Table"; Filename: "{app}\{#MyAppExeName}"; Check: ShouldCreateDesktopShortcut

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent; Check: ShouldLaunchAfterInstall

[Code]
var
  DesktopShortcutCheck: TNewCheckBox;
  StartMenuShortcutCheck: TNewCheckBox;
  LaunchAfterInstallCheck: TNewCheckBox;
  RemoveLocalAppDataCheck: TNewCheckBox;

procedure InitializeWizard;
var
  CheckParent: TWinControl;
  CheckTop: Integer;
begin
  WizardForm.WelcomeLabel1.Caption := '{#MyAppName}';
  WizardForm.WelcomeLabel2.Caption := 'A native Windows app where multiple AI models collaborate in a structured, phase-driven workflow with defined roles, enforced authority on the software level, and a shared artifact to solve a user task.';
  WizardForm.LicenseLabel1.Caption := '{#MyAppName} is distributed under the MIT License. Please review the license before continuing.';
  WizardForm.SelectDirLabel.Caption := 'Choose where AI Meeting Table will be installed and which shortcuts should be created.';

  CheckParent := WizardForm.DirEdit.Parent;
  CheckTop := WizardForm.DiskSpaceLabel.Top + ScaleY(20);

  DesktopShortcutCheck := TNewCheckBox.Create(WizardForm);
  DesktopShortcutCheck.Parent := CheckParent;
  DesktopShortcutCheck.Left := WizardForm.DirEdit.Left;
  DesktopShortcutCheck.Top := CheckTop;
  DesktopShortcutCheck.Width := WizardForm.SelectDirLabel.Width;
  DesktopShortcutCheck.Caption := 'Create desktop shortcut';
  DesktopShortcutCheck.Checked := True;

  StartMenuShortcutCheck := TNewCheckBox.Create(WizardForm);
  StartMenuShortcutCheck.Parent := CheckParent;
  StartMenuShortcutCheck.Left := DesktopShortcutCheck.Left;
  StartMenuShortcutCheck.Top := DesktopShortcutCheck.Top + DesktopShortcutCheck.Height + ScaleY(6);
  StartMenuShortcutCheck.Width := DesktopShortcutCheck.Width;
  StartMenuShortcutCheck.Caption := 'Create Start menu shortcut';
  StartMenuShortcutCheck.Checked := True;

  LaunchAfterInstallCheck := TNewCheckBox.Create(WizardForm);
  LaunchAfterInstallCheck.Parent := CheckParent;
  LaunchAfterInstallCheck.Left := DesktopShortcutCheck.Left;
  LaunchAfterInstallCheck.Top := StartMenuShortcutCheck.Top + StartMenuShortcutCheck.Height + ScaleY(6);
  LaunchAfterInstallCheck.Width := DesktopShortcutCheck.Width;
  LaunchAfterInstallCheck.Caption := 'Launch AI Meeting Table after installation';
  LaunchAfterInstallCheck.Checked := True;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = wpSelectDir then
  begin
    WizardForm.PageNameLabel.Caption := 'Installation Options';
    WizardForm.PageDescriptionLabel.Caption := 'Choose the install location and default shortcuts.';
  end;
end;

function ShouldCreateDesktopShortcut: Boolean;
begin
  Result := (DesktopShortcutCheck = nil) or DesktopShortcutCheck.Checked;
end;

function ShouldCreateStartMenuShortcut: Boolean;
begin
  Result := (StartMenuShortcutCheck = nil) or StartMenuShortcutCheck.Checked;
end;

function ShouldLaunchAfterInstall: Boolean;
begin
  Result := (LaunchAfterInstallCheck = nil) or LaunchAfterInstallCheck.Checked;
end;

procedure InitializeUninstallProgressForm();
begin
  RemoveLocalAppDataCheck := TNewCheckBox.Create(UninstallProgressForm);
  RemoveLocalAppDataCheck.Parent := UninstallProgressForm.StatusLabel.Parent;
  RemoveLocalAppDataCheck.Left := UninstallProgressForm.ProgressBar.Left;
  RemoveLocalAppDataCheck.Top := UninstallProgressForm.ProgressBar.Top + UninstallProgressForm.ProgressBar.Height + ScaleY(16);
  RemoveLocalAppDataCheck.Width := UninstallProgressForm.ProgressBar.Width;
  RemoveLocalAppDataCheck.Caption := 'Also remove local app data (settings, saved tables, and artifacts)';
  RemoveLocalAppDataCheck.Checked := False;
end;

procedure RemoveLocalAppData();
var
  RoamingCompanyDir: string;
  RoamingAppDir: string;
  LocalCompanyDir: string;
  LocalAppDir: string;
  ResultCode: Integer;
begin
  RoamingCompanyDir := ExpandConstant('{userappdata}\AI Meeting Table');
  RoamingAppDir := RoamingCompanyDir + '\AI Meeting Table';
  LocalCompanyDir := ExpandConstant('{localappdata}\AI Meeting Table');
  LocalAppDir := LocalCompanyDir + '\AI Meeting Table';

  RegDeleteKeyIncludingSubkeys(HKCU, 'Software\AI Meeting Table\AI Meeting Table');
  Exec(ExpandConstant('{sys}\cmdkey.exe'), '/delete:AI_MEETING_TABLE_OPENAI', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  Exec(ExpandConstant('{sys}\cmdkey.exe'), '/delete:AI_MEETING_TABLE_GEMINI', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  Exec(ExpandConstant('{sys}\cmdkey.exe'), '/delete:AI_MEETING_TABLE_ANTHROPIC', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  DelTree(RoamingAppDir, True, True, True);
  DelTree(LocalAppDir, True, True, True);
  RemoveDir(RoamingCompanyDir);
  RemoveDir(LocalCompanyDir);
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if (CurUninstallStep = usUninstall) and
     (RemoveLocalAppDataCheck <> nil) and
     RemoveLocalAppDataCheck.Checked then
  begin
    RemoveLocalAppData();
  end;
end;
