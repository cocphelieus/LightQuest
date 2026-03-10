[Setup]
AppId={{9A66A4D5-2C27-4F7D-8D14-09D6D56D53A9}
AppName=LightQuest
AppVersion=1.0.0
DefaultDirName={localappdata}\Programs\LightQuest
DisableProgramGroupPage=yes
OutputDir=C:\Users\acer\OneDrive\Desktop\LightQuest\dist
OutputBaseFilename=LightQuest-Setup
Compression=lzma
SolidCompression=yes
WizardStyle=modern
SetupIconFile=C:\Users\acer\OneDrive\Desktop\LightQuest\dist\LightQuest-Player\LightQuest.ico
PrivilegesRequired=lowest
UninstallDisplayIcon={app}\LightQuest.ico

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create desktop shortcut"; GroupDescription: "Additional icons:"; Flags: unchecked

[Files]
Source: "C:\Users\acer\OneDrive\Desktop\LightQuest\dist\LightQuest-Player\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\LightQuest"; Filename: "{app}\LightQuest.exe"; IconFilename: "{app}\LightQuest.ico"
Name: "{autodesktop}\LightQuest"; Filename: "{app}\LightQuest.exe"; IconFilename: "{app}\LightQuest.ico"; Tasks: desktopicon

[Run]
Filename: "{app}\LightQuest.exe"; Description: "Launch LightQuest"; Flags: nowait postinstall skipifsilent
