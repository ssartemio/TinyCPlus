; Inno Setup 6 script. Built on Windows from tools/build_installers.py.
#ifndef MyAppVersion
  #define MyAppVersion "1.0.0-rc.1"
#endif
#ifndef PayloadDir
  #error PayloadDir is required
#endif
#ifndef OutputDir
  #error OutputDir is required
#endif
[Setup]
AppId={{91F3A49E-1BC8-4EFA-87A4-D9FEE20A7F04}
AppName=TinyC+
AppVersion={#MyAppVersion}
AppPublisher=TinyC+ contributors
AppPublisherURL=https://github.com/ssartemio/TinyCPlus
AppSupportURL=https://github.com/ssartemio/TinyCPlus/issues
DefaultDirName={localappdata}\Programs\TinyCPlus
DefaultGroupName=TinyC+
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ChangesEnvironment=yes
DisableProgramGroupPage=no
UninstallDisplayIcon={app}\bin\tiny.exe
OutputDir={#OutputDir}
OutputBaseFilename=TinyCPlus-{#MyAppVersion}-windows-x64
SetupLogging=yes
[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
[Files]
Source: "{#PayloadDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{group}\TinyC+ console"; Filename: "{cmd}"; Parameters: "/K ""{app}\bin\tiny.exe"" --version"
Name: "{group}\TinyC+ repository"; Filename: "https://github.com/ssartemio/TinyCPlus"
[Tasks]
Name: "addtopath"; Description: "Agregar tiny/tinyc/tinyedit al PATH del usuario"; GroupDescription: "Integración con la terminal:"; Flags: checkedonce
[Code]
const
  EnvironmentKey = 'Environment';
  ProductKey = 'Software\TinyCPlus';

function ContainsPath(const Current, Segment: String): Boolean;
begin
  Result := Pos(';' + Uppercase(Segment) + ';', ';' + Uppercase(Current) + ';') > 0;
end;

procedure AddUserPath(const Segment: String);
var
  Current: String;
begin
  if not RegQueryStringValue(HKCU, EnvironmentKey, 'Path', Current) then
    Current := '';
  if ContainsPath(Current, Segment) then
    Exit;
  if Current = '' then
    Current := Segment
  else
    Current := Current + ';' + Segment;
  if not RegWriteExpandStringValue(HKCU, EnvironmentKey, 'Path', Current) then
    RaiseException('Could not update user PATH');
  RegWriteDWordValue(HKCU, ProductKey, 'PathOwned', 1);
end;

procedure RemoveUserPath(const Segment: String);
var
  Current, Updated: String;
  Owned: Cardinal;
begin
  if not RegQueryDWordValue(HKCU, ProductKey, 'PathOwned', Owned) or (Owned <> 1) then
    Exit;
  if RegQueryStringValue(HKCU, EnvironmentKey, 'Path', Current) then
  begin
    Updated := ';' + Current + ';';
    StringChangeEx(Updated, ';' + Segment + ';', ';', True);
    if Length(Updated) >= 2 then
      Updated := Copy(Updated, 2, Length(Updated) - 2)
    else
      Updated := '';
    RegWriteExpandStringValue(HKCU, EnvironmentKey, 'Path', Updated);
  end;
  RegDeleteValue(HKCU, ProductKey, 'PathOwned');
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssPostInstall) and WizardIsTaskSelected('addtopath') then
    AddUserPath(ExpandConstant('{app}\bin'));
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
    RemoveUserPath(ExpandConstant('{app}\bin'));
end;
