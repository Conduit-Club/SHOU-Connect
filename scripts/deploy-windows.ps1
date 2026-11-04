# Windows deployment script
# Usage: .\deploy-windows.ps1 -TargetName "SHOUConnect" -DisplayName "SHOUConnect" -BuildDir "build" -Architecture "amd64" -Nightly "false"

param(
    [string]$TargetName = "SHOUConnect",
    [string]$DisplayName = "SHOUConnect",
    [string]$BuildDir = "build",
    [string]$Architecture = "amd64",
    [string]$Nightly = "false"
)

$ErrorActionPreference = 'Stop'
$workspacePath = (Get-Location).Path
$deploymentPath = [IO.Path]::GetFullPath((Join-Path $workspacePath $DisplayName))
if (-not $deploymentPath.StartsWith($workspacePath + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Deployment directory must be inside the workspace.'
}
if (Test-Path -LiteralPath $deploymentPath) {
    throw "Deployment directory already exists; use a fresh workspace: $deploymentPath"
}
New-Item -ItemType Directory -Path $deploymentPath | Out-Null
$toolsPath = Join-Path $PSScriptRoot 'package_tools.py'
Copy-Item -LiteralPath "$BuildDir/Release/$TargetName.exe" -Destination $deploymentPath
Push-Location -LiteralPath $deploymentPath
try {
    & windeployqt.exe --release --translations 'zh_CN,zh_TW,en' --skip-plugin-types qmltooling "$TargetName.exe"
    if ($LASTEXITCODE -ne 0) { throw 'windeployqt failed' }
    $coreOptions = @($toolsPath, 'fetch-core', '.', '--platform', 'windows', '--architecture', $Architecture)
    if ($Nightly -eq 'true') { $coreOptions += '--nightly' }
    & python @coreOptions
    if ($LASTEXITCODE -ne 0) { throw 'Core download failed' }
    Copy-Item -LiteralPath "$workspacePath/libs/wintun/bin/$Architecture/wintun.dll" -Destination .
    Copy-Item -LiteralPath "$workspacePath/resource/qt.conf" -Destination .
    # Portable archives retain the existing documented VC Redistributable prerequisite.
    $runtimeInstaller = if ($Architecture -eq 'amd64') { 'vc_redist.x64.exe' } else { 'vc_redist.arm64.exe' }
    if (Test-Path -LiteralPath $runtimeInstaller) { Remove-Item -LiteralPath $runtimeInstaller }
    & python $toolsPath prune . --output "$workspacePath/package-pruning.json"
    if ($LASTEXITCODE -ne 0) { throw 'Package pruning failed' }
} finally {
    Pop-Location
}
