;
; inno setup installation script for WIN32 Orange C package
;
[Setup]
PrivilegesRequired=admin
AppName=Orange C
AppVerName=Orange C Version 5.0.10.1
OutputBaseFileName=setup
AppPublisher=LADSoft
AppPublisherURL=http:\\members.tripod.com\~ladsoft
AppSupportURL=http:\\members.tripod.com\~ladsoft
AppUpdatesURL=http:\\members.tripod.com\~ladsoft
DefaultDirName={pf}\Orange C 386
DefaultGroupName=Orange C 386
SetupIconFile=c:\orangec\src\ocide\res\prj.ico
;AlwaysCreateUninstallIcon=yes
LicenseFile=License.txt
;BackColor=clRed
;BackColor2=clAqua
;WindowVisible= yes
AppCopyright=Copyright(C) LADSoft 1994-2014, All Rights Reserved.
WizardSmallImageFile=ladsoft1.bmp
WizardImageFile=ladsoftl.bmp
WizardImageBackColor=clAqua
DisableStartupPrompt=yes
InfoAfterFile=relnotes.doc
Compression=lzma
UninstallDisplayIcon={app}\bin\ocide.exe,1
uninstallable = not IsComponentSelected('main\memstick')
ChangesAssociations=yes
ChangesEnvironment=yes

; uncomment the following line if you want your installation to run on NT 3.51 too.
; MinVersion=4,3.51

[Messages]
BeveledLabel=Orange C, Copyright (C) LADSoft, 1994-2013

[Types]
Name: "desktop"; Description: "Desktop Installation"
Name: "memstick"; Description: "Memory Stick Installation"

[Components]
Name: "main"; Description: "Main Files"; types: desktop memstick; flags: fixed
Name: "main\desktop"; Description: "Desktop"; types: desktop; flags: exclusive
Name: "main\memstick"; Description: "Memory Stick"; types: memstick; flags: exclusive

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:"; MinVersion: 4,4; Flags: unchecked; Components: main\desktop
Name: "projectassociation"; Description: "Add file association for workspace files (.cwa, .cpj)"; GroupDescription: "General:";  Components: main\desktop
Name: "fileassociation"; Description: "Add file association for .C, .CPP, .H files"; GroupDescription: "General:"; Flags: unchecked;  Components: main\desktop
Name: "addtopath"; Description: "Add Orange C to the path"; GroupDescription: "General:"; Flags: checkedonce;  Components: main\desktop

[Dirs]
Name: "{userdocs}\Orange C Projects"; Components: main\desktop
Name: "{app}\appdata"; Components: main\memstick

[Files]
Source: "C:\orangec\src\ocidehld.bat"; DestDir: "{app}"; DestName: "ocide.bat"; Flags: IgnoreVersion; Components: main\memstick
Source: "C:\orangec\license\*.*"; DestDir: "{app}\license\"; Flags: IgnoreVersion; Components: main
Source: "C:\orangec\bin\*.*"; DestDir: "{app}\bin\"; Flags: IgnoreVersion;Components: main
Source: "C:\orangec\help\*.*"; DestDir: "{app}\help\"; Flags: IgnoreVersion; Components: main
Source: "C:\orangec\rule\*.rul"; DestDir: "{app}\rule\"; Flags: IgnoreVersion; Components: main
Source: "C:\orangec\include\*.*"; DestDir: "{app}\include\"; Flags: IgnoreVersion; Components: main
Source: "C:\orangec\include\sys\*.*"; DestDir: "{app}\include\sys"; Flags: IgnoreVersion; Components: main
Source: "C:\orangec\include\win32\*.*"; DestDir: "{app}\include\win32\"; Flags: IgnoreVersion; Components: main
Source: "C:\orangec\lib\*.*"; DestDir: "{app}\lib\"; Flags: IgnoreVersion; Components: main
Source: "C:\orangec\lib\startup\*.*"; DestDir: "{app}\lib\startup\"; Flags: IgnoreVersion; Components: main
Source: "C:\orangec\doc\*.*"; DestDir: "{app}\doc\"; Flags: IgnoreVersion; Components: main
Source: "C:\orangec\doc\general\*.*"; DestDir: "{app}\doc\general\"; Flags: IgnoreVersion; Components: main
Source: "C:\orangec\doc\oasm\*.*"; DestDir: "{app}\doc\oasm\"; Flags: IgnoreVersion; Components: main

Source: "C:\orangec\doc\occ\*.*"; DestDir: "{app}\doc\occ\"; Flags: IgnoreVersion; Components: main
Source: "C:\orangec\doc\ogrep\*.*"; DestDir: "{app}\doc\ogrep\"; Flags: IgnoreVersion; Components: main
Source: "C:\orangec\doc\olink\*.*"; DestDir: "{app}\doc\olink\"; Flags: IgnoreVersion; Components: main
Source: "C:\orangec\doc\omake\*.*"; DestDir: "{app}\doc\omake\"; Flags: IgnoreVersion; Components: main

;Source: "C:\orangec\bin\lscrtl.dll"; DestDir: "{sys}"; Components: main\desktop

Source: "C:\orangec\examples\*.*"; DestDir: "{userdocs}\Orange C Projects\examples\"; Flags: IgnoreVersion; Components: main\desktop
Source: "C:\orangec\examples\msdos\*.*"; DestDir: "{userdocs}\Orange C Projects\examples\msdos\"; Flags: IgnoreVersion; Components: main\desktop
Source: "C:\orangec\examples\win32\*.*"; DestDir: "{userdocs}\Orange C Projects\examples\windows examples\"; Flags: IgnoreVersion; Components: main\desktop
Source: "C:\orangec\examples\win32\listview\*.*"; DestDir: "{userdocs}\Orange C Projects\examples\windows examples\listview"; Flags: IgnoreVersion; Components: main\desktop
Source: "C:\orangec\examples\win32\xmlview\*.*"; DestDir: "{userdocs}\Orange C Projects\examples\windows examples\xmlview"; Flags: IgnoreVersion; Components: main\desktop
Source: "C:\orangec\examples\win32\RCDemo\*.*"; DestDir: "{userdocs}\Orange C Projects\examples\windows examples\RCDemo"; Flags: IgnoreVersion; Components: main\desktop

Source: "C:\orangec\examples\*.*"; DestDir: "{app}\Orange C Projects\examples\"; Flags: IgnoreVersion; Components: main\memstick
Source: "C:\orangec\examples\msdos\*.*"; DestDir: "{app}\Orange C Projects\examples\msdos\"; Flags: IgnoreVersion; Components: main\memstick
Source: "C:\orangec\examples\win32\*.*"; DestDir: "{app}\Orange C Projects\examples\windows examples\"; Flags: IgnoreVersion; Components: main\memstick
Source: "C:\orangec\examples\win32\listview\*.*"; DestDir: "{app}\Orange C Projects\examples\windows examples\listview"; Flags: IgnoreVersion; Components: main\memstick
Source: "C:\orangec\examples\win32\xmlview\*.*"; DestDir: "{app}\Orange C Projects\examples\windows examples\xmlview"; Flags: IgnoreVersion; Components: main\memstick
Source: "C:\orangec\examples\win32\RCDemo\*.*"; DestDir: "{app}\Orange C Projects\examples\windows examples\RCDemo"; Flags: IgnoreVersion; Components: main\memstick

[Icons]
Name: "{group}\Orange C IDE"; Filename: "{app}\bin\ocide.exe"; Components: main\desktop;
Name: "{group}\Orange C IDE Help"; Filename: "{app}\help\ccide.chm"; Components: main\desktop;
Name: "{group}\C Language Help"; Filename: "{app}\help\chelp.chm"; Components: main\desktop;
Name: "{group}\Runtime Help"; Filename: "{app}\help\crtl.chm"; Components: main\desktop;
Name: "{group}\Tools Help"; Filename: "{app}\help\tools.chm"; Components: main\desktop;
Name: "{userdesktop}\Orange C IDE"; Filename: "{app}\bin\ocide.exe"; MinVersion: 4,4; Components: main\desktop; Tasks: desktopicon

[Run]
Filename: "{app}\bin\ocide.exe"; Parameters: " "; Description: "Launch Orange C IDE"; Flags: nowait postinstall skipifsilent unchecked; Components: main\desktop;

[Registry]
Root: HKLM; Subkey: "Software\LADSoft"; Flags: uninsdeletekeyifempty; Components: main\desktop;
Root: HKLM; SubKey: "Software\LADSoft\ORANGEC"; Valuetype: string; ValueName: "InstallPath"; ValueData: "{app}"; Components: main\desktop;
Root: HKLM; SubKey: "Software\LADSoft\ORANGEC\OCIDE"; ValueType: string; ValueName: "InstallPath"; ValueData: "{app}"; Components: main\desktop;

Root: HKCR; Subkey: ".cwa"; Flags: uninsdeletekeyifempty; Components: main\desktop;
Root: HKCR; Subkey: "cwafile.cwafile"; Flags: uninsdeletekey; Components: main\desktop;
Root: HKCR; Subkey: ".cwa"; Valuetype: string; ValueData: "cwafile.cwafile"; Flags: uninsdeletevalue; tasks: projectassociation; Components: main\desktop;
Root: HKCR; Subkey: "cwafile.cwafile"; Valuetype: string; Valuedata: "CCIDE Workspace File"; Flags: uninsdeletevalue; tasks: projectassociation; Components: main\desktop;
Root: HKCR; Subkey: "cwafile.cwafile\DefaultIcon"; Valuetype: string; ValueData: "{app}\bin\ocide.exe,1"; tasks: projectassociation; Components: main\desktop;
Root: HKCR; Subkey: "cwafile.cwafile\Shell"; tasks: projectassociation; Components: main\desktop;
Root: HKCR; Subkey: "cwafile.cwafile\Shell\Open"; tasks: projectassociation; Components: main\desktop;
Root: HKCR; Subkey: "cwafile.cwafile\Shell\Open\command"; valuetype: string; ValueData: "{app}\bin\ocide ""/w%1"""; tasks: projectassociation; Components: main\desktop;

Root: HKCR; Subkey: ".cpj"; Flags: uninsdeletekeyifempty; Components: main\desktop;
Root: HKCR; Subkey: "cpjfile.cpjfile"; Flags: uninsdeletekey; Components: main\desktop;
Root: HKCR; Subkey: ".cpj"; Valuetype: string; ValueData: "cpjfile.cpjfile"; Flags: uninsdeletevalue; tasks: projectassociation; Components: main\desktop;
Root: HKCR; Subkey: "cpjfile.cpjfile"; Valuetype: string; Valuedata: "CCIDE Target File"; Flags: uninsdeletevalue; tasks: projectassociation; Components: main\desktop;
Root: HKCR; Subkey: "cpjfile.cpjfile\DefaultIcon"; Valuetype: string; ValueData: "{app}\bin\ocide.exe,2"; tasks: projectassociation; Components: main\desktop;
Root: HKCR; Subkey: "cpjfile.cpjfile\Shell"; tasks: projectassociation; Components: main\desktop;
Root: HKCR; Subkey: "cpjfile.cpjfile\Shell\Open"; tasks: projectassociation; Components: main\desktop;
Root: HKCR; Subkey: "cpjfile.cpjfile\Shell\Open\command"; valuetype: string; ValueData: "{app}\bin\ocide ""/p%1"""; tasks: projectassociation; Components: main\desktop;

Root: HKCR; Subkey: ".c"; Flags: uninsdeletekeyifempty; Components: main\desktop;
Root: HKCR; Subkey: "cfile.cfile"; Flags: uninsdeletekey; Components: main\desktop;
Root: HKCR; Subkey: ".c"; Valuetype: string; ValueData: "cfile.cfile"; Flags: uninsdeletevalue; tasks: fileassociation; Components: main\desktop;
Root: HKCR; Subkey: "cfile.cfile"; Valuetype: string; Valuedata: "C language file"; Flags: uninsdeletevalue; tasks: fileassociation; Components: main\desktop;
Root: HKCR; Subkey: "cfile.cfile\DefaultIcon"; Valuetype: string; ValueData: "{app}\bin\ocide.exe,3"; tasks: fileassociation; Components: main\desktop;
Root: HKCR; Subkey: "cfile.cfile\Shell"; tasks: fileassociation; Components: main\desktop;
Root: HKCR; Subkey: "cfile.cfile\Shell\Open"; tasks: fileassociation; Components: main\desktop;
Root: HKCR; Subkey: "cfile.cfile\Shell\Open\command"; valuetype: string; ValueData: "{app}\bin\ocide ""%1"""; tasks: fileassociation; Components: main\desktop;

Root: HKCR; Subkey: ".h"; Flags: uninsdeletekeyifempty; Components: main\desktop;
Root: HKCR; Subkey: "hfile.hfile"; Flags: uninsdeletekey; Components: main\desktop;
Root: HKCR; Subkey: ".h"; Valuetype: string; ValueData: "hfile.hfile"; Flags: uninsdeletevalue; tasks: fileassociation; Components: main\desktop;
Root: HKCR; Subkey: "hfile.hfile"; Valuetype: string; Valuedata: "C language header file"; Flags: uninsdeletevalue; tasks: fileassociation; Components: main\desktop;
Root: HKCR; Subkey: "hfile.hfile\DefaultIcon"; Valuetype: string; ValueData: "{app}\bin\ocide.exe,5"; tasks: fileassociation; Components: main\desktop;
Root: HKCR; Subkey: "hfile.hfile\Shell"; tasks: fileassociation; Components: main\desktop;
Root: HKCR; Subkey: "hfile.hfile\Shell\Open"; tasks: fileassociation; Components: main\desktop;
Root: HKCR; Subkey: "hfile.hfile\Shell\Open\command"; valuetype: string; ValueData: "{app}\bin\ocide ""%1"""; tasks: fileassociation; Components: main\desktop;

Root: HKCR; Subkey: ".hxx"; Flags: uninsdeletekeyifempty; Components: main\desktop;
Root: HKCR; Subkey: "hxxfile.hxxfile"; Flags: uninsdeletekey; Components: main\desktop;
Root: HKCR; Subkey: ".hxx"; Valuetype: string; ValueData: "hxxfile.hxxfile"; Flags: uninsdeletevalue; tasks: fileassociation; Components: main\desktop;
Root: HKCR; Subkey: "hxxfile.hxxfile"; Valuetype: string; Valuedata: "C++ language header file"; Flags: uninsdeletevalue; tasks: fileassociation; Components: main\desktop;
Root: HKCR; Subkey: "hxxfile.hxxfile\DefaultIcon"; Valuetype: string; ValueData: "{app}\bin\ocide.exe,5"; tasks: fileassociation; Components: main\desktop;
Root: HKCR; Subkey: "hxxfile.hxxfile\Shell"; tasks: fileassociation; Components: main\desktop;
Root: HKCR; Subkey: "hxxfile.hxxfile\Shell\Open"; tasks: fileassociation; Components: main\desktop;
Root: HKCR; Subkey: "hxxfile.hxxfile\Shell\Open\command"; valuetype: string; ValueData: "{app}\bin\ocide ""%1"""; tasks: fileassociation; Components: main\desktop;

Root: HKCR; Subkey: ".inl"; Flags: uninsdeletekeyifempty; Components: main\desktop;
Root: HKCR; Subkey: "inlfile.inlfile"; Flags: uninsdeletekey; Components: main\desktop;
Root: HKCR; Subkey: ".inl"; Valuetype: string; ValueData: "inlfile.inlfile"; Flags: uninsdeletevalue; tasks: fileassociation; Components: main\desktop;
Root: HKCR; Subkey: "inlfile.inlfile"; Valuetype: string; Valuedata: "C++ language inline file"; Flags: uninsdeletevalue; tasks: fileassociation; Components: main\desktop;
Root: HKCR; Subkey: "inlfile.inlfile\DefaultIcon"; Valuetype: string; ValueData: "{app}\bin\ocide.exe,5"; tasks: fileassociation; Components: main\desktop;
Root: HKCR; Subkey: "inlfile.inlfile\Shell"; tasks: fileassociation; Components: main\desktop;
Root: HKCR; Subkey: "inlfile.inlfile\Shell\Open"; tasks: fileassociation; Components: main\desktop;
Root: HKCR; Subkey: "inlfile.inlfile\Shell\Open\command"; valuetype: string; ValueData: "{app}\bin\ocide ""%1"""; tasks: fileassociation; Components: main\desktop;

Root: HKCR; Subkey: ".cpp"; Flags: uninsdeletekeyifempty; Components: main\desktop;
Root: HKCR; Subkey: ".cxx"; Flags: uninsdeletekeyifempty; Components: main\desktop;
Root: HKCR; Subkey: ".cc"; Flags: uninsdeletekeyifempty; Components: main\desktop;
Root: HKCR; Subkey: "cppfile.cppfile"; Flags: uninsdeletekey; Components: main\desktop;
Root: HKCR; Subkey: ".cpp"; Valuetype: string; ValueData: "cppfile.cppfile"; Flags: uninsdeletevalue; tasks: fileassociation; Components: main\desktop;
Root: HKCR; Subkey: ".cxx"; Valuetype: string; ValueData: "cppfile.cppfile"; Flags: uninsdeletevalue; tasks: fileassociation; Components: main\desktop;
Root: HKCR; Subkey: ".cc"; Valuetype: string; ValueData: "cppfile.cppfile"; Flags: uninsdeletevalue; tasks: fileassociation; Components: main\desktop;
Root: HKCR; Subkey: "cppfile.cppfile"; Valuetype: string; Valuedata: "C++ language file"; Flags: uninsdeletevalue; tasks: fileassociation; Components: main\desktop;
Root: HKCR; Subkey: "cppfile.cppfile\DefaultIcon"; Valuetype: string; ValueData: "{app}\bin\ocide.exe,4"; tasks: fileassociation; Components: main\desktop;
Root: HKCR; Subkey: "cppfile.cppfile\Shell"; tasks: fileassociation; Components: main\desktop;
Root: HKCR; Subkey: "cppfile.cppfile\Shell\Open"; tasks: fileassociation; Components: main\desktop;
Root: HKCR; Subkey: "cppfile.cppfile\Shell\Open\command"; valuetype: string; ValueData: "{app}\bin\ocide ""%1"""; tasks: fileassociation; Components: main\desktop;


[UninstallDelete]
type: filesandordirs; Name: "{userdocs}\Orange C Projects\default.cwa"; Components: main\desktop;
type: filesandordirs; Name: "{userdocs}\Orange C Projects\default.ods"; Components: main\desktop;
type: filesandordirs; Name: "{userappdata}\Orange C\ocide.ini"; Components: main\desktop;
type: filesandordirs; Name: "{userappdata}\Orange C"; Components: main\desktop;

;[UninstallRun]
;Filename: "{app}\bin\defs.exe";  Parameters: "/U ""{app}"" "; Flags: runminimized

[Code]
function InitializeSetup(): Boolean;
begin
  Result := True;
end;

procedure DeinitializeSetup();
var
  FileName: String;
  ResultCode: Integer;
begin
end;
function InitializeUninstall(): Boolean;
begin
  Result := True;
end;

procedure DeinitializeUninstall();
begin
end;

procedure AddToPath(Path : String);
var
  WorkingPath : String;
begin
  RegQueryStringValue(HKEY_LOCAL_MACHINE, 'System\CurrentControlSet\Control\Session Manager\Environment', 'Path', WorkingPath);
  if Pos(Path, workingPath) = 0 then
    begin
      Insert(Path, WorkingPath, 1);
      RegWriteExpandStringValue(HKEY_LOCAL_MACHINE, 'System\CurrentControlSet\Control\Session Manager\Environment', 'Path', WorkingPath);
    end;
end;
procedure RemoveFromPath(Path : String);
var
  WorkingPath : String;
begin
  RegQueryStringValue(HKEY_LOCAL_MACHINE, 'System\CurrentControlSet\Control\Session Manager\Environment', 'Path', WorkingPath);
  if Pos(Path, workingPath) <> 0 then
    begin
      Delete(WorkingPath, Pos(Path, workingPath), Length(Path));
      RegWriteExpandStringValue(HKEY_LOCAL_MACHINE, 'System\CurrentControlSet\Control\Session Manager\Environment', 'Path', WorkingPath);
    end;
end;
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
    begin
      DeleteFile(ExpandConstant('{app}\help\ocide.chm:Zone.Identifier'));
      DeleteFile(ExpandConstant('{app}\help\tools.chm:Zone.Identifier'));
      DeleteFile(ExpandConstant('{app}\help\crtl.chm:Zone.Identifier'));
      DeleteFile(ExpandConstant('{app}\help\chelp.chm:Zone.Identifier'));
      if Not IsComponentSelected('main\memstick') Then
        Begin
          RegWriteExpandStringValue(HKEY_LOCAL_MACHINE, 'System\CurrentControlSet\Control\Session Manager\Environment', 'ORANGEC',ExpandConstant('{app}'));
          if IsTaskSelected('addtopath') Then
            Begin
              AddToPath(ExpandConstant('{app}\bin;'));
            end;
        end;
   end;
end;
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
    begin
      Begin
        RegDeleteValue(HKEY_LOCAL_MACHINE, 'System\CurrentControlSet\Control\Session Manager\Environment', 'ORANGEC');
        RemoveFromPath(ExpandConstant('{app}\bin;'));
      End
    end;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  if IsComponentSelected('main\memstick') Then
    Begin
       if PageID = wpSelectProgramGroup then
         Begin
           result := true;
         End
       Else
         Begin
          result := false;
         End
    end
  else
    Begin
      result := false;
    End;
end;
function UpdateReadyMemo(Space, NewLine, MemoUserInfoInfo, MemoDirInfo, MemoTypeInfo, MemoComponentsInfo, MemoGroupInfo, MemoTasksInfo: String): String;
var
  retval : string;
begin
  retval := MemoDirInfo + NewLine + NewLine + MemoTypeInfo ;
  if IsComponentSelected('main\desktop') Then
    Begin
      retval := retval + NewLine + NewLine + MemoTasksInfo + NewLine + NewLine + MemoGroupInfo;
    End
    result := retval;
end;


procedure CurPageChanged(CurPageID: Integer);
begin
end;

