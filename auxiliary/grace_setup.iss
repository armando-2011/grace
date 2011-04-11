; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!

#define MyAppName "Grace"
#define MyAppVersion "5.99.1 dev5"
#define MyAppPublisher "Grace Development Team"
#define MyAppURL "http://plasma-gate.weizmann.ac.il/Grace/"
#define MyAppExeName "rungrace.bat"

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{ED79C90E-DF0D-4867-9A4E-374559551A95}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
;AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={pf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
LicenseFile=C:\cygwin\home\img\grace\LICENSE
OutputBaseFilename=setup
SetupIconFile=C:\doc\grace\grace.ico
Compression=lzma
SolidCompression=yes
ChangesAssociations=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "C:\doc\grace\rungrace.bat"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\doc\grace\grace.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\doc\grace\grace\*"; DestDir: "{app}\grace"; Flags: ignoreversion recursesubdirs createallsubdirs
; NOTE: Don't use "Flags: ignoreversion" on any shared system files

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\grace.ico"
; Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"

[Registry]
Root: HKCR; Subkey: ".xgr"; ValueType: string; ValueName: ""; ValueData: "Grace.xgr"; Flags: uninsdeletevalue
Root: HKCR; Subkey: ".agr"; ValueType: string; ValueName: ""; ValueData: "Grace.xgr"; Flags: uninsdeletevalue
Root: HKCR; Subkey: "Grace.xgr"; ValueType: string; ValueName: ""; ValueData: "Grace Project File"; Flags: uninsdeletekey
Root: HKCR; Subkey: "Grace.xgr\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppName}.ico"
Root: HKCR; Subkey: "Grace.xgr\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""

