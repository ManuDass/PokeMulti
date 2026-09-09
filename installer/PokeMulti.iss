#ifndef AppVersion
  #define AppVersion "0.26.1"
#endif
#ifndef PackageDir
  #define PackageDir "..\dist\PokeMulti-" + AppVersion
#endif
#ifndef OutputDir
  #define OutputDir "..\dist"
#endif
[Setup]
AppId={{A8A74D98-5D72-4764-9D0E-B7B6B723D0E8}
AppName=PokéMulti
AppVersion={#AppVersion}
AppPublisher=ManuDass
AppPublisherURL=https://github.com/ManuDass/PokeMulti
AppSupportURL=https://github.com/ManuDass/PokeMulti/issues
AppUpdatesURL=https://github.com/ManuDass/PokeMulti/releases/latest
DefaultDirName={localappdata}\Programs\PokeMulti
DefaultGroupName=PokéMulti
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
DisableWelcomePage=no
DisableDirPage=no
DisableProgramGroupPage=yes
AllowNoIcons=yes
WizardStyle=modern
SetupIconFile=..\src\frontend\pokemulti.ico
UninstallDisplayIcon={app}\pokemulti.exe
AppMutex=Local\PokeMulti.AppRunning
CloseApplications=yes
RestartApplications=no
OutputDir={#OutputDir}
OutputBaseFilename=PokeMulti-Setup
Compression=lzma2
SolidCompression=yes
SetupLogging=yes
UninstallDisplayName=PokéMulti
[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"
[Files]
Source: "{#PackageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{group}\PokéMulti"; Filename: "{app}\pokemulti.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\PokéMulti"; Filename: "{app}\pokemulti.exe"; WorkingDir: "{app}"; Tasks: desktopicon
[Run]
Filename: "{app}\pokemulti.exe"; Description: "Launch PokéMulti"; Flags: nowait postinstall skipifsilent
[UninstallDelete]
Type: files; Name: "{app}\.pokemulti-update.lock"
[Code]
procedure InitializeWizard;
begin
  WizardForm.WelcomeLabel2.Caption :=
    'This wizard installs PokéMulti and creates shortcuts so you can play with friends.' + #13#10#13#10 +
    'Choose your install folder on the next page. When PokéMulti opens, select your own supported ROM and choose an online username.' + #13#10#13#10 +
    'No ROM is included. Existing worlds and trainer saves are kept separately and are preserved.';
end;
