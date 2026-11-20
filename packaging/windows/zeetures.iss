; Zeetures's Windows installer (owner, 2026-11-20: players install and
; start it without a command line). Built by the nightly workflow with Inno
; Setup 6:
;
;   iscc /DX64Dir=<64-bit install folder> [/DX86Dir=<32-bit install folder>]
;        /DAppVersion=0.18.0 /DOutDir=<folder> packaging\windows\zeetures.iss
;
; One installer holds both builds and installs the 64-bit one on 64-bit
; Windows (and on Windows on Arm, which runs it emulated), the 32-bit one
; otherwise. It installs for the current user, with no administrator
; prompt, unless the player chooses all users. It carries only the engine
; and the project's own art: no part of Creatures 2.

#ifndef X64Dir
  #error Give the 64-bit build's folder: /DX64Dir=...
#endif
#ifndef AppVersion
  #define AppVersion "0"
#endif
#ifndef OutDir
  #define OutDir "."
#endif

[Setup]
AppId={{5B0B6E2C-7C1D-4E43-9B7E-2A9C2E1F0D35}
AppName=Zeetures
AppVersion={#AppVersion}
AppVerName=Zeetures {#AppVersion}
AppPublisher=Zeetures (a fan fork of openc2e)
AppPublisherURL=https://github.com/DotairZee/zeetures
AppSupportURL=https://github.com/DotairZee/zeetures/issues
AppUpdatesURL=https://github.com/DotairZee/zeetures/releases/tag/nightly
DefaultDirName={autopf}\Zeetures
DefaultGroupName=Zeetures
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesInstallIn64BitMode=x64compatible
#ifndef X86Dir
ArchitecturesAllowed=x64compatible
#endif
InfoBeforeFile=install-notice.txt
SetupIconFile=..\icons\zeetures.ico
UninstallDisplayIcon={app}\Zeetures.exe
OutputDir={#OutDir}
OutputBaseFilename=Zeetures-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#X64Dir}\*"; DestDir: "{app}"; Check: Is64BitInstallMode; Flags: ignoreversion recursesubdirs createallsubdirs
#ifdef X86Dir
Source: "{#X86Dir}\*"; DestDir: "{app}"; Check: not Is64BitInstallMode; Flags: ignoreversion recursesubdirs createallsubdirs
#endif

[InstallDelete]
; (an older install's openc2e.exe, from before the program was Zeetures.exe)
Type: files; Name: "{app}\openc2e.exe"

[Icons]
Name: "{group}\Zeetures"; Filename: "{app}\Zeetures.exe"; WorkingDir: "{app}"
Name: "{group}\Zeetures on the web"; Filename: "https://github.com/DotairZee/zeetures"
Name: "{group}\{cm:UninstallProgram,Zeetures}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Zeetures"; Filename: "{app}\Zeetures.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\Zeetures.exe"; Description: "{cm:LaunchProgram,Zeetures}"; Flags: nowait postinstall skipifsilent

; Uninstalling removes only the program: the player's worlds, saves and
; settings in Documents\My Games stay.
