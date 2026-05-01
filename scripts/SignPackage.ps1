# 1. 查找项目根目录
$projectRoot = Split-Path $PSScriptRoot -Parent
$packageDir = Join-Path $projectRoot "src\Calculator\AppPackages"
$latestBundle = Get-ChildItem -Path $packageDir -Filter "*.msixbundle" -Recurse | Sort-Object LastWriteTime -Descending | Select-Object -First 1

if (-not $latestBundle) {
    Write-Host "错误：找不到 .msixbundle 文件。" -ForegroundColor Red
    exit 1
}

$packagePath = $latestBundle.FullName
Write-Host "检测到构建产物：$packagePath" -ForegroundColor Cyan

# 2. 提取发布者
Write-Host "正在提取包内发布者信息..." -ForegroundColor Gray
$tempDir = Join-Path $env:TEMP ([Guid]::NewGuid().ToString())
New-Item -ItemType Directory -Path $tempDir | Out-Null
& "D:\Windows Kits\10\bin\10.0.26100.0\x64\MakeAppx.exe" unbundle /p $packagePath /d $tempDir /o | Out-Null
$manifestPath = Join-Path $tempDir "AppxMetadata\AppxBundleManifest.xml"
if (-not (Test-Path $manifestPath)) {
    & "D:\Windows Kits\10\bin\10.0.26100.0\x64\MakeAppx.exe" unpack /p $packagePath /d $tempDir /o | Out-Null
    $manifestPath = Join-Path $tempDir "AppxManifest.xml"
}

[xml]$manifest = Get-Content -Path $manifestPath
$publisher = if ($manifest.Bundle.Identity.Publisher) { $manifest.Bundle.Identity.Publisher } else { $manifest.Package.Identity.Publisher }
Remove-Item -Path $tempDir -Recurse -Force
$publisher = $publisher.Trim()

Write-Host "提取成功！发布者: $publisher" -ForegroundColor Green

# 3. 寻找 signtool (包括 D 盘)
$searchPaths = @("C:\Program Files (x86)\Windows Kits\10\bin", "D:\Windows Kits\10\bin")
$signtool = (Get-ChildItem -Path $searchPaths -Filter signtool.exe -Recurse -ErrorAction SilentlyContinue | Where-Object { $_.FullName -like "*x64*" } | Sort-Object FullName -Descending | Select-Object -First 1).FullName
if (-not $signtool) { $signtool = "C:\Program Files (x86)\Microsoft SDKs\ClickOnce\SignTool\signtool.exe" }
Write-Host "使用工具: $signtool" -ForegroundColor Cyan


# 4. 使用 ECDsa 算法生成证书 (参考 SignTestApp.ps1)
Write-Host "正在生成 ECDsa 证书..." -ForegroundColor Cyan
$codeSignOid = New-Object -TypeName "System.Security.Cryptography.Oid" -ArgumentList @("1.3.6.1.5.5.7.3.3")
$oidColl = New-Object -TypeName "System.Security.Cryptography.OidCollection"
$oidColl.Add($codeSignOid) > $null

$certReq = New-Object -TypeName "System.Security.Cryptography.X509Certificates.CertificateRequest" `
    -ArgumentList @($publisher, ([System.Security.Cryptography.ECDsa]::Create()), "SHA256") 
$certReq.CertificateExtensions.Add((New-Object -TypeName "System.Security.Cryptography.X509Certificates.X509EnhancedKeyUsageExtension" `
            -ArgumentList @($oidColl, $false)))
$now = Get-Date
$cert = $certReq.CreateSelfSigned($now, $now.AddYears(1))

$pfxFile = Join-Path $PSScriptRoot "TTSCalculator_SelfSigned.pfx"
[System.IO.File]::WriteAllBytes($pfxFile, $cert.Export("Pfx"))

# 5. 执行签名并导入
Write-Host "正在签名..." -ForegroundColor Cyan
& $signtool sign /v /fd SHA256 /a /f $pfxFile $packagePath

if ($LASTEXITCODE -eq 0) {
    Write-Host "恭喜！签名成功！" -ForegroundColor Green
    Write-Host "正在将证书导入 LocalMachine\TrustedPeople 以备 Inno Setup 使用..." -ForegroundColor Gray
    Import-PfxCertificate -CertStoreLocation 'Cert:\LocalMachine\TrustedPeople' -FilePath $pfxFile > $null
    
    # 导出 .cer 文件供 Inno Setup 使用
    $cerFile = Join-Path $PSScriptRoot "TTSCalculator_SelfSigned.cer"
    [System.IO.File]::WriteAllBytes($cerFile, $cert.Export("Cert"))
} else {
    Write-Host "签名依然失败。错误码: $LASTEXITCODE" -ForegroundColor Red
}
