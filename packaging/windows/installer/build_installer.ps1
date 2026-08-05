param(
    [string]$InnoCompiler = 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe',
    [string]$SourceUrl = 'https://github.com/noobieisgod/AI-Meeting-Table',
    [string]$ReleaseRoot,
    [string]$ReleaseDir,
    [string]$OutputDir
)

$ErrorActionPreference = 'Stop'

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptRoot '..\..\..')).Path

if (-not $ReleaseRoot) {
    $ReleaseRoot = Join-Path $repoRoot 'build\release'
}

$cmakeText = Get-Content -LiteralPath (Join-Path $repoRoot 'CMakeLists.txt') -Raw
$match = [regex]::Match($cmakeText, 'project\s*\(\s*AIMeetingTable\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)')
if (-not $match.Success) {
    throw 'Unable to determine app version from CMakeLists.txt.'
}

$appVersion = $match.Groups[1].Value

if (-not $ReleaseDir) {
    $ReleaseDir = Join-Path $ReleaseRoot "AIMeetingTable-$appVersion"
}

if (-not $OutputDir) {
    $OutputDir = Join-Path $ReleaseRoot 'Installer'
}

$licenseFile = Join-Path $repoRoot 'LICENSE'
$legalNoticesFile = Join-Path $repoRoot 'packaging\windows\installer\LEGAL-NOTICES.txt'
$issFile = Join-Path $scriptRoot 'AIMeetingTable.iss'
$appExe = Join-Path $ReleaseDir 'AIMeetingTable.exe'

if (-not (Test-Path -LiteralPath $InnoCompiler)) {
    throw "Inno Setup compiler not found: $InnoCompiler"
}
if (-not (Test-Path -LiteralPath $issFile)) {
    throw "Installer script not found: $issFile"
}
if (-not (Test-Path -LiteralPath $licenseFile)) {
    throw "LICENSE not found: $licenseFile"
}
if (-not (Test-Path -LiteralPath $legalNoticesFile)) {
    throw "LEGAL-NOTICES.txt not found: $legalNoticesFile"
}
if (-not (Test-Path -LiteralPath $ReleaseDir)) {
    throw "Packaged GUI release folder not found: $ReleaseDir"
}
if (-not (Test-Path -LiteralPath $appExe)) {
    throw "Packaged GUI release executable not found: $appExe"
}

$debugRuntimePattern = '^(Qt6.*d|q.*d|msvcp.*d|vcruntime.*d|ucrtbased|concrt.*d)\.dll$'
$debugRuntimeFiles = Get-ChildItem -LiteralPath $ReleaseDir -Recurse -File |
    Where-Object { $_.Name -match $debugRuntimePattern }
if ($debugRuntimeFiles) {
    $names = ($debugRuntimeFiles.Name | Sort-Object -Unique) -join ', '
    throw "Release folder contains Debug Runtime files: $names"
}

New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

$arguments = @(
    "/DAppVersion=$appVersion",
    "/DRepoRoot=$repoRoot",
    "/DReleaseDir=$ReleaseDir",
    "/DOutputDir=$OutputDir",
    "/DSourceUrl=$SourceUrl",
    $issFile
)

Write-Host "Building installer for AI Meeting Table $appVersion"
Write-Host "Release input: $ReleaseDir"
Write-Host "Installer output: $OutputDir"

& $InnoCompiler @arguments

if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup compiler failed with exit code $LASTEXITCODE."
}

$installerPath = Join-Path $OutputDir "AI-Meeting-Table-$appVersion-setup.exe"
if (-not (Test-Path -LiteralPath $installerPath)) {
    throw "Expected installer output not found: $installerPath"
}

Write-Host "Installer created: $installerPath"
$checksum = (Get-FileHash -LiteralPath $installerPath -Algorithm SHA256).Hash.ToLowerInvariant()
$checksumPath = "$installerPath.sha256"
Set-Content -LiteralPath $checksumPath -Value "$checksum  $([IO.Path]::GetFileName($installerPath))" -Encoding ascii
Write-Host "SHA-256 checksum: $checksumPath"
