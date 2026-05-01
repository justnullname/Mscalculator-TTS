; Inno Setup Script for TTS Calculator
#define AppName "TTS Calculator"
#define AppVersion "0.0.1.0"
#define AppPublisher "Microsoft Corporation (Fork)"
#define AppId "{B58171C6-C70C-4266-A2E8-8F9C994F4456}"
#define PackageName "Calculator_0.0.1.0_x64.msixbundle"
#define CertName "TTSCalculator_SelfSigned.cer"

[Setup]
AppId={{#AppId}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
AllowNoIcons=yes
OutputDir=..\bin
OutputBaseFilename=TTSCalculator_Setup
Compression=lzma
SolidCompression=yes
WizardStyle=modern
; 要求管理员权限以导入证书
PrivilegesRequired=admin

[Languages]
Name: "chinesesimplified"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "..\src\Calculator\AppPackages\Calculator_0.0.1.0_Test\{#PackageName}"; DestDir: "{tmp}"; Flags: deleteafterinstall
Source: ".\{#CertName}"; DestDir: "{tmp}"; Flags: deleteafterinstall

[Run]
; 1. 导入证书
Filename: "certutil.exe"; Parameters: "-addstore ""TrustedPeople"" ""{tmp}\{#CertName}"""; StatusMsg: "正在安装安全证书..."
; 2. 安装 MSIX 包 (移除 runhidden 以便看到报错)
Filename: "powershell.exe"; Parameters: "-NoExit -ExecutionPolicy Bypass -Command ""Write-Host '正在通过 PowerShell 安装 MSIX 包...' -ForegroundColor Cyan; Add-AppxPackage -Path '{tmp}\{#PackageName}'; if ($?) {{ Write-Host '------------------------------------------' -ForegroundColor Green; Write-Host '安装成功！' -ForegroundColor Green; Write-Host '请在开始菜单搜索 [TTS Calculator] 运行' -ForegroundColor Green; Write-Host '你可以关闭此窗口。' -ForegroundColor Green } else {{ Write-Host '------------------------------------------' -ForegroundColor Red; Write-Host '安装失败！请查看上方的红字报错。' -ForegroundColor Red }"""; StatusMsg: "正在安装应用包..."




[UninstallRun]
; 卸载应用
Filename: "powershell.exe"; Parameters: "-Command ""Get-AppxPackage *Mscalculator-TTS-Local* | Remove-AppxPackage"""; Flags: runhidden
; 清理证书 (可选)
Filename: "certutil.exe"; Parameters: "-delstore ""TrustedPeople"" ""TTS Calculator Dev Cert"""; Flags: runhidden
