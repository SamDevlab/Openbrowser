param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,

    [Parameter(Mandatory = $true)]
    [string]$CefRoot,

    [Parameter(Mandatory = $true)]
    [string]$OutputDir,

    [Parameter(Mandatory = $true)]
    [string]$Version
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

# Linker/debug outputs are useful to developers but are not part of the end-user
# runtime. Keeping the portable package runtime-only also reduces accidental
# coupling between distribution contents and the selected Visual Studio generator.
Get-ChildItem -Path $packageDir -Recurse -File | Where-Object {
    $_.Extension -in @('.lib', '.exp', '.pdb', '.ilk', '.obj')
} | Remove-Item -Force

Copy-Item -LiteralPath (Join-Path $repoRoot 'LICENSE') -Destination (Join-Path $packageDir 'OPENBROWSER-LICENSE.txt') -Force
Copy-Item -LiteralPath (Join-Path $repoRoot 'THIRD_PARTY.md') -Destination (Join-Path $packageDir 'THIRD_PARTY.md') -Force
Copy-Item -LiteralPath (Join-Path $repoRoot 'README.md') -Destination (Join-Path $packageDir 'README.md') -Force
Copy-Item -LiteralPath (Join-Path $cefRootResolved 'LICENSE.txt') -Destination (Join-Path $packageDir 'CEF-LICENSE.txt') -Force
Copy-Item -LiteralPath (Join-Path $cefRootResolved 'CREDITS.html') -Destination (Join-Path $packageDir 'CEF-CREDITS.html') -Force

$startHere = @"
Openbrowser $Version — Experimental Windows x64 MVP

1. Extract the entire ZIP to a normal NTFS folder that you own.
2. Keep all files and the locales directory together.
3. Run openbrowser.exe.

Openbrowser is pre-alpha software. This build is currently unsigned, so Windows
may show an Unknown Publisher / SmartScreen warning. Do not use this build for
banking or other sensitive browsing.

On first launch Openbrowser verifies and, when necessary, applies the LPAC read/
execute ACL required by Chromium's Network Service sandbox. If that permission
cannot be established, Openbrowser fails closed instead of silently weakening the
sandbox. Moving the extracted folder to a normal NTFS location owned by your user
should resolve permission failures.

To verify the downloaded ZIP before extracting it, keep the accompanying
Openbrowser-$Version-windows-x64.zip.sha256 file and run:

  Get-FileHash .\Openbrowser-$Version-windows-x64.zip -Algorithm SHA256

Compare that hash with the first value in the .sha256 file.

Licenses and third-party notices are included in this directory.
"@
Set-Content -LiteralPath (Join-Path $packageDir 'START-HERE.txt') -Value $startHere -Encoding utf8

$requiredFiles = @(
    'openbrowser.exe',
    'openbrowser.dll',
    'libcef.dll',
    'chrome_elf.dll',
    'icudtl.dat',
    'resources.pak',
    'v8_context_snapshot.bin',
    'START-HERE.txt',
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

$unexpectedDeveloperArtifacts = @(Get-ChildItem -Path $packageDir -Recurse -File | Where-Object {
    $_.Extension -in @('.lib', '.exp', '.pdb', '.ilk', '.obj')
})
if ($unexpectedDeveloperArtifacts.Count -ne 0) {
    throw "Developer-only linker/debug artifacts remain in the portable package."
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
