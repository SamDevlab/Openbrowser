param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,

    [Parameter(Mandatory = $true)]
    [string]$CefRoot,

    [Parameter(Mandatory = $true)]
    [string]$OutputDir,

    [string]$Version = "0.1.0"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = (Resolve-Path $BuildDir).Path
$cefRootResolved = (Resolve-Path $CefRoot).Path

$exeCandidates = @(Get-ChildItem -Path $buildRoot -Recurse -File -Filter "openbrowser.exe")
if ($exeCandidates.Count -eq 0) {
    throw "openbrowser.exe was not found under '$buildRoot'."
}

$exe = $exeCandidates | Where-Object { $_.FullName -match '[\\/]Release[\\/]' } | Select-Object -First 1
if ($null -eq $exe) {
    $exe = $exeCandidates | Select-Object -First 1
}

$runtimeDir = $exe.Directory.FullName
$artifactRoot = [System.IO.Path]::GetFullPath($OutputDir)
$packageName = "Openbrowser-$Version-windows-x64"
$packageDir = Join-Path $artifactRoot $packageName
$zipPath = Join-Path $artifactRoot "$packageName.zip"
$shaPath = "$zipPath.sha256"

if (Test-Path $packageDir) {
    Remove-Item -Recurse -Force $packageDir
}
if (Test-Path $zipPath) {
    Remove-Item -Force $zipPath
}
if (Test-Path $shaPath) {
    Remove-Item -Force $shaPath
}

New-Item -ItemType Directory -Force -Path $artifactRoot | Out-Null
New-Item -ItemType Directory -Force -Path $packageDir | Out-Null
Copy-Item -Path (Join-Path $runtimeDir '*') -Destination $packageDir -Recurse -Force

Copy-Item -LiteralPath (Join-Path $repoRoot 'LICENSE') -Destination (Join-Path $packageDir 'OPENBROWSER-LICENSE.txt') -Force
Copy-Item -LiteralPath (Join-Path $repoRoot 'THIRD_PARTY.md') -Destination (Join-Path $packageDir 'THIRD_PARTY.md') -Force
Copy-Item -LiteralPath (Join-Path $repoRoot 'README.md') -Destination (Join-Path $packageDir 'README.md') -Force
Copy-Item -LiteralPath (Join-Path $cefRootResolved 'LICENSE.txt') -Destination (Join-Path $packageDir 'CEF-LICENSE.txt') -Force
Copy-Item -LiteralPath (Join-Path $cefRootResolved 'CREDITS.html') -Destination (Join-Path $packageDir 'CEF-CREDITS.html') -Force

$requiredFiles = @(
    'openbrowser.exe',
    'libcef.dll',
    'icudtl.dat',
    'resources.pak',
    'OPENBROWSER-LICENSE.txt',
    'THIRD_PARTY.md',
    'CEF-LICENSE.txt',
    'CEF-CREDITS.html'
)

foreach ($relativePath in $requiredFiles) {
    $candidate = Join-Path $packageDir $relativePath
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
        throw "Required Windows runtime file is missing from package: $relativePath"
    }
}

$clientDll = Join-Path $packageDir 'openbrowser.dll'
if (-not (Test-Path -LiteralPath $clientDll -PathType Leaf)) {
    throw "Sandboxed Windows build did not produce openbrowser.dll next to bootstrap openbrowser.exe."
}

$localesDir = Join-Path $packageDir 'locales'
if (-not (Test-Path -LiteralPath $localesDir -PathType Container)) {
    throw "CEF locales directory is missing from the package."
}
if (@(Get-ChildItem -Path $localesDir -File -Filter '*.pak').Count -eq 0) {
    throw "CEF locales directory does not contain any locale .pak files."
}

$cefCredits = Get-Item -LiteralPath (Join-Path $packageDir 'CEF-CREDITS.html')
if ($cefCredits.Length -eq 0) {
    throw "CEF-CREDITS.html is empty."
}

Compress-Archive -Path $packageDir -DestinationPath $zipPath -CompressionLevel Optimal

if (-not (Test-Path -LiteralPath $zipPath -PathType Leaf)) {
    throw "Portable ZIP was not created."
}

$zipHash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
"$zipHash  $([System.IO.Path]::GetFileName($zipPath))" | Set-Content -LiteralPath $shaPath -Encoding ascii -NoNewline

$zip = Get-Item -LiteralPath $zipPath
Write-Host "PACKAGE_DIR=$packageDir"
Write-Host "PACKAGE_ZIP=$zipPath"
Write-Host "PACKAGE_SHA256=$zipHash"
Write-Host "PACKAGE_SIZE_BYTES=$($zip.Length)"
