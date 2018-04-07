; Xi2p Installer for Windows
; Copyright (c) 2017, The Xi2p I2P Router Project
; See LICENSE

; This is the common script code for both the 64 bit and the 32 bit installer,
; configured with the help of the compile-time constant "Bitness"

#define RootDir "..\..\.."

[Setup]
AppName=Xi2p I2P Router
AppVersion=Latest
DefaultDirName={pf}\Xi2p
DefaultGroupName=Xi2p I2P Router
UninstallDisplayIcon={app}\Xi2p.ico
PrivilegesRequired=admin
#if Bitness == 64
  ArchitecturesInstallIn64BitMode=x64
  ArchitecturesAllowed=x64
#endif
WizardSmallImageFile=WizardSmallImage.bmp
WizardImageFile=WizardImage.bmp
DisableWelcomePage=no
LicenseFile=LICENSE
OutputBaseFilename=Xi2pSetup{#Bitness}
OutputDir={#RootDir}\build


[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"


[Files]
; The use of the flag "ignoreversion" for the following entries leads to the following behaviour:
; When updating / upgrading an existing installation ALL existing files are replaced with the files in this
; installer, regardless of file dates, version info within the files, or type of file (textual file or
; .exe/.dll file possibly with version info).
;
; This is far more robust than relying on version info or on file dates (flag "comparetimestamp").
;
; Note that it would be very dangerous to use "ignoreversion" on files that may be shared with other
; applications somehow. Luckily this is no issue here because ALL files are "private" to Xi2p.

Source: "{#RootDir}\build\xi2p.exe";      DestDir: "{app}"; Flags: ignoreversion
Source: "{#RootDir}\build\xi2p-util.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "Xi2p.ico";                          DestDir: "{app}"; Flags: ignoreversion
Source: "ReadMe.htm";                         DestDir: "{app}"; Flags: ignoreversion

; Install/overwrite "client" files: "hosts.txt" and "publishers" in "\client\address_book"
; plus certificates in "\client\certificates"
Source: "{#RootDir}\pkg\client\*"; DestDir: "{userappdata}\Xi2p\client"; Flags: recursesubdirs ignoreversion

; Backup any existing user config files, as we will overwrite "xi2p.conf" and "tunnels.conf" unconditionally
; Note that Inno Setup goes through the "[Files]" entries strictly in the order given here,
; therefore the old files are backed-up correctly BEFORE the new ones overwrite them
Source: "{userappdata}\Xi2p\config\xi2p.conf";   DestDir: "{userappdata}\Xi2p\config"; DestName: "xi2p.conf.bak";   Flags: external skipifsourcedoesntexist
Source: "{userappdata}\Xi2p\config\tunnels.conf"; DestDir: "{userappdata}\Xi2p\config"; DestName: "tunnels.conf.bak"; Flags: external skipifsourcedoesntexist

Source: "{#RootDir}\pkg\config\*"; DestDir: "{userappdata}\Xi2p\config"; Flags: recursesubdirs ignoreversion



[InstallDelete]
; Delete .exe files that the "old" install batch file copied directly to the desktop,
; as this installer works with another, user-selectable location for those files;
; optional with the help of the "Tasks" section
Type: files; Name: "{userdesktop}\xi2p.exe";      Tasks: deletedesktopexe
Type: files; Name: "{userdesktop}\xi2p-util.exe"; Tasks: deletedesktopexe

; For every update delete all files and directories with transient data;
; with the following statements, in the "client" sub-directory currently only "hosts.txt" and "publishers.txt" are not deleted
Type: filesandordirs; Name: "{userappdata}\Xi2p\client\address_book\addresses"
Type: files;          Name: "{userappdata}\Xi2p\client\address_book\addresses.csv"
Type: filesandordirs; Name: "{userappdata}\Xi2p\client\certificates"
Type: filesandordirs; Name: "{userappdata}\Xi2p\core"


[UninstallDelete]
; Per default the uninstaller will only delete files it installed; to get rid of the whole "Xi2p"
; directory with all the additional data that was created at runtime this special delete statement is needed
Type: filesandordirs; Name: "{userappdata}\Xi2p"


[Tasks]
Name: desktopicon; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:";
Name: deletedesktopexe; Description: "Delete any Xi2p .exe files on desktop"; GroupDescription: "Additional actions:";


[Run]
Filename: "{app}\ReadMe.htm"; Description: "Show ReadMe"; Flags: postinstall shellexec skipifsilent


[Code]
(* So far nothing to do programmatically by using Pascal procedures *)


[Icons]
; Icons in the "Xi2p I2P Router" program group
; Windows will almost always display icons in alphabetical order, per level, so specify the text accordingly
Name: "{group}\Xi2p Daemon";         Filename: "{app}\xi2p.exe";      IconFilename: "{app}\Xi2p.ico"
Name: "{group}\Xi2p Utility";        Filename: "{app}\xi2p-util.exe"; IconFilename: "{app}\Xi2p.ico"
Name: "{group}\Read Me";              Filename: "{app}\ReadMe.htm"
Name: "{group}\Show Config Folder";   Filename: "{win}\Explorer.exe";   Parameters: "{userappdata}\Xi2p\config"
Name: "{group}\Uninstall Xi2p";      Filename: "{uninstallexe}"

; Desktop icon, optional with the help of the "Tasks" section
Name: "{userdesktop}\Xi2p Daemon"; Filename: "{app}\xi2p.exe"; IconFilename: "{app}\Xi2p.ico"; Tasks: desktopicon


[Registry]
; So far Xi2p does not use the Windows registry
