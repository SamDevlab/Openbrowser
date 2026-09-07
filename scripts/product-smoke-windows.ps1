param(
    [Parameter(Mandatory = $true)]
    [string]$ArtifactDir,

    [Parameter(Mandatory = $true)]
    [string]$Version
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Wait-Until {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Description,

        [Parameter(Mandatory = $true)]
        [scriptblock]$Condition,

        [int]$TimeoutSeconds = 20
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        if (& $Condition) {
            return
        }
        Start-Sleep -Milliseconds 250
    }

    throw "Timed out waiting for $Description."
}

function Get-RequestCount {
    param(
        [Parameter(Mandatory = $true)]
        [string]$LogPath,

        [Parameter(Mandatory = $true)]
        [string]$RequestTarget
    )

    if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) {
        return 0
    }

    return @(Select-String -LiteralPath $LogPath -SimpleMatch "GET $RequestTarget ").Count
}

function Close-BrowserGracefully {
    param(
        [Parameter(Mandatory = $true)]
        [System.Diagnostics.Process]$Process
    )

    Wait-Until -Description 'Openbrowser main window' -TimeoutSeconds 15 -Condition {
        if ($Process.HasExited) {
            throw "Openbrowser exited before its main window became available (code $($Process.ExitCode))."
        }
        $Process.Refresh()
        return $Process.MainWindowHandle -ne [IntPtr]::Zero
    }

    if (-not $Process.CloseMainWindow()) {
        throw 'Openbrowser rejected the graceful main-window close request.'
    }

    if (-not $Process.WaitForExit(20000)) {
        throw 'Openbrowser did not exit after a graceful window close.'
    }

    return $Process.ExitCode
}

function Write-ShutdownDiagnostics {
    param(
        [Parameter(Mandatory = $true)]
        [int]$ExitCode,

        [Parameter(Mandatory = $true)]
        [string]$RuntimeRoot,

        [Parameter(Mandatory = $true)]
        [string]$StateRoot
    )

    $unsignedExitCode = [BitConverter]::ToUInt32([BitConverter]::GetBytes([int]$ExitCode), 0)
    Write-Host ("Unexpected graceful-close exit code: {0} (0x{1:X8})" -f $ExitCode, $unsignedExitCode)

    $debugCandidates = @(
        (Join-Path $RuntimeRoot 'debug.log'),
        (Join-Path $StateRoot 'debug.log'),
        (Join-Path $StateRoot 'default/debug.log')
    )
    foreach ($debugLog in $debugCandidates) {
        if (Test-Path -LiteralPath $debugLog -PathType Leaf) {
            Write-Host "CEF/Openbrowser log: $debugLog"
            Get-Content -LiteralPath $debugLog -Tail 200
        }
    }

    Start-Sleep -Seconds 1
    try {
        $since = (Get-Date).AddMinutes(-3)
        $events = @(Get-WinEvent -FilterHashtable @{ LogName = 'Application'; StartTime = $since } -ErrorAction Stop |
            Where-Object {
                $_.Message -match 'openbrowser|libcef|chrome_elf'
            } |
            Select-Object -First 8)
        if ($events.Count -gt 0) {
            Write-Host 'Recent Windows Application events related to Openbrowser/CEF:'
            foreach ($event in $events) {
                Write-Host ("Event {0} Provider={1}: {2}" -f $event.Id, $event.ProviderName, $event.Message)
            }
        } else {
            Write-Host 'No matching Windows Application crash event was available yet.'
        }
    } catch {
        Write-Host "Windows event-log diagnostics unavailable: $($_.Exception.Message)"
    }
}

function Start-Browser {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Exe,

        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory,

        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    return Start-Process `
        -FilePath $Exe `
        -ArgumentList $Arguments `
        -WorkingDirectory $WorkingDirectory `
        -PassThru
}

$artifactRoot = (Resolve-Path -LiteralPath $ArtifactDir).Path
$packageName = "Openbrowser-$Version-windows-x64"
$zipPath = Join-Path $artifactRoot "$packageName.zip"
$shaPath = Join-Path $artifactRoot "$packageName.zip.sha256"

if (-not (Test-Path -LiteralPath $zipPath -PathType Leaf)) {
    throw "Product smoke package missing: $zipPath"
}
if (-not (Test-Path -LiteralPath $shaPath -PathType Leaf)) {
    throw "Product smoke checksum missing: $shaPath"
}

$expectedSha = ((Get-Content -LiteralPath $shaPath -Raw) -split '\s+')[0].Trim().ToLowerInvariant()
$actualSha = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualSha -ne $expectedSha) {
    throw "Product smoke ZIP checksum mismatch. Expected=$expectedSha Actual=$actualSha"
}

$runRoot = Join-Path $env:RUNNER_TEMP "openbrowser-product-smoke-$([guid]::NewGuid().ToString('N'))"
$extractRoot = Join-Path $runRoot 'package'
$stateRoot = Join-Path $runRoot 'state'
$siteRoot = Join-Path $runRoot 'site'
$serverOut = Join-Path $runRoot 'http.stdout.log'
$serverErr = Join-Path $runRoot 'http.stderr.log'

New-Item -ItemType Directory -Force -Path $extractRoot, $stateRoot, $siteRoot | Out-Null
Set-Content -LiteralPath (Join-Path $siteRoot 'smoke.html') -Encoding UTF8 -Value @'
<!doctype html>
<html>
<head><meta charset="utf-8"><title>Openbrowser Product Smoke</title></head>
<body><h1>Openbrowser packaged product smoke</h1></body>
</html>
'@

Expand-Archive -LiteralPath $zipPath -DestinationPath $extractRoot
$runtimeRoot = Join-Path $extractRoot $packageName
$exe = Join-Path $runtimeRoot 'openbrowser.exe'
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
    throw "Extracted Openbrowser executable missing: $exe"
}

$listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, 0)
$listener.Start()
$port = ([System.Net.IPEndPoint]$listener.LocalEndpoint).Port
$listener.Stop()

$python = (Get-Command python.exe -ErrorAction Stop).Source
$server = Start-Process `
    -FilePath $python `
    -ArgumentList @('-u', '-m', 'http.server', "$port", '--bind', '127.0.0.1', '--directory', $siteRoot) `
    -WorkingDirectory $siteRoot `
    -RedirectStandardOutput $serverOut `
    -RedirectStandardError $serverErr `
    -PassThru

$browser = $null
$restoredBrowser = $null
$env:OPENBROWSER_NONINTERACTIVE = '1'

try {
    Wait-Until -Description 'local HTTP smoke server' -TimeoutSeconds 10 -Condition {
        if ($server.HasExited) {
            throw "Local HTTP server exited early with code $($server.ExitCode)."
        }
        try {
            $probe = Invoke-WebRequest -Uri "http://127.0.0.1:$port/smoke.html?probe=ready" -TimeoutSec 2
            return $probe.StatusCode -eq 200
        } catch {
            return $false
        }
    }

    $requestTarget = '/smoke.html?run=first'
    $smokeUrl = "http://127.0.0.1:$port$requestTarget"
    $sessionFile = Join-Path $stateRoot 'session.json'

    $prereq = Start-Process `
        -FilePath $exe `
        -ArgumentList '--openbrowser-sandbox-prereq-check' `
        -WorkingDirectory $runtimeRoot `
        -PassThru `
        -Wait
    if ($prereq.ExitCode -ne 0) {
        throw "Packaged sandbox prerequisite probe failed with code $($prereq.ExitCode)."
    }

    $browser = Start-Browser `
        -Exe $exe `
        -WorkingDirectory $runtimeRoot `
        -Arguments @(
            "--url=$smokeUrl",
            '--disable-gpu',
            "--storage-dir=$stateRoot",
            "--session-file=$sessionFile"
        )

    Wait-Until -Description 'first packaged navigation' -TimeoutSeconds 25 -Condition {
        if ($browser.HasExited) {
            throw "Openbrowser exited during first navigation (code $($browser.ExitCode))."
        }
        return (Get-RequestCount -LogPath $serverErr -RequestTarget $requestTarget) -ge 1
    }

    $firstExitCode = Close-BrowserGracefully -Process $browser
    $browser = $null

    if (-not (Test-Path -LiteralPath $sessionFile -PathType Leaf)) {
        if ($firstExitCode -ne 0) {
            Write-ShutdownDiagnostics -ExitCode $firstExitCode -RuntimeRoot $runtimeRoot -StateRoot $stateRoot
        }
        throw 'Session file was not written after graceful shutdown.'
    }

    $sessionRaw = Get-Content -LiteralPath $sessionFile -Raw
    $session = $sessionRaw | ConvertFrom-Json
    if ($firstExitCode -ne 0) {
        Write-Host "Persisted session after abnormal close:"
        Write-Host $sessionRaw
        Write-ShutdownDiagnostics -ExitCode $firstExitCode -RuntimeRoot $runtimeRoot -StateRoot $stateRoot
    }

    if ($session.clean_shutdown -ne $true) {
        throw 'Session did not record a clean shutdown.'
    }
    if (@($session.tabs | Where-Object { $_.url -eq $smokeUrl }).Count -ne 1) {
        throw 'Committed smoke URL was not persisted in the session.'
    }

    $historyFile = Join-Path $stateRoot 'history.json'
    if (-not (Test-Path -LiteralPath $historyFile -PathType Leaf)) {
        throw 'History file was not persisted after real navigation.'
    }
    $history = Get-Content -LiteralPath $historyFile -Raw | ConvertFrom-Json
    if (@($history.entries | Where-Object { $_.url -eq $smokeUrl }).Count -lt 1) {
        throw 'Committed smoke URL was not persisted in history.'
    }

    if ($firstExitCode -ne 0) {
        throw "Openbrowser returned a non-zero status after window close."
    }

    $firstRequestCount = Get-RequestCount -LogPath $serverErr -RequestTarget $requestTarget

    # Relaunch without --url. The default restore_session_on_startup=true must
    # restore the persisted tab and cause a second real request to the same URL.
    $restoredBrowser = Start-Browser `
        -Exe $exe `
        -WorkingDirectory $runtimeRoot `
        -Arguments @(
            '--disable-gpu',
            "--storage-dir=$stateRoot",
            "--session-file=$sessionFile"
        )

    Wait-Until -Description 'restored packaged navigation' -TimeoutSeconds 25 -Condition {
        if ($restoredBrowser.HasExited) {
            throw "Openbrowser exited during restored navigation (code $($restoredBrowser.ExitCode))."
        }
        return (Get-RequestCount -LogPath $serverErr -RequestTarget $requestTarget) -gt $firstRequestCount
    }

    $secondExitCode = Close-BrowserGracefully -Process $restoredBrowser
    $restoredBrowser = $null

    $restoredSession = Get-Content -LiteralPath $sessionFile -Raw | ConvertFrom-Json
    if ($secondExitCode -ne 0) {
        Write-ShutdownDiagnostics -ExitCode $secondExitCode -RuntimeRoot $runtimeRoot -StateRoot $stateRoot
    }
    if ($restoredSession.clean_shutdown -ne $true) {
        throw 'Relaunched product did not finish with a clean persisted shutdown.'
    }
    if (@($restoredSession.tabs | Where-Object { $_.url -eq $smokeUrl }).Count -ne 1) {
        throw 'Restored session lost the smoke tab after the second shutdown.'
    }

    if ($secondExitCode -ne 0) {
        throw "Relaunched Openbrowser returned a non-zero status after window close."
    }

    $finalRequestCount = Get-RequestCount -LogPath $serverErr -RequestTarget $requestTarget
    Write-Host "Packaged product lifecycle smoke passed."
    Write-Host "  URL: $smokeUrl"
    Write-Host "  Observed page requests: $finalRequestCount"
    Write-Host "  Session restore: verified"
    Write-Host "  History persistence: verified"
    Write-Host "  Clean shutdown x2: verified"
    Write-Host "  ZIP SHA-256: $actualSha"
} finally {
    foreach ($candidate in @($browser, $restoredBrowser)) {
        if ($null -ne $candidate -and -not $candidate.HasExited) {
            & taskkill.exe /PID $candidate.Id /T /F | Out-Null
        }
    }
    if (-not $server.HasExited) {
        & taskkill.exe /PID $server.Id /T /F | Out-Null
    }
    Remove-Item Env:OPENBROWSER_NONINTERACTIVE -ErrorAction SilentlyContinue
    Remove-Item -Recurse -Force $runRoot -ErrorAction SilentlyContinue
}
