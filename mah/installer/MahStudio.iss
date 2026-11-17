; Inno Setup skeleton — MAH Studio (fill paths after CI artifacts exist)
#define MyAppName "MAH Studio"
#define MyAppVersion "0.1.0"
#define MyAppPublisher "Mayank After Hours"
#define MyAppURL "https://github.com/msr157/mah-studio"

[Setup]
AppId={{A11ACE0D-4A4A-4A4A-4A4A-MAHSTUDIO0001}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
DefaultDirName={autopf}\MAH Studio
DefaultGroupName=MAH Studio
OutputBaseFilename=MAHStudio-Setup-{#MyAppVersion}
Compression=lzma
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
; SignTool=signtool (configure after you have a cert)

[Files]
; Source: "dist\MAHStudio\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs

[Icons]
Name: "{group}\MAH Studio"; Filename: "{app}\bin\64bit\obs64.exe"

[Run]
Filename: "{app}\bin\64bit\obs64.exe"; Description: "Launch MAH Studio"; Flags: nowait postinstall skipifsilent
