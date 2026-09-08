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

function Get-HistoryVisitCount {
    param(
        [Parameter(Mandatory = $true)]
        [string]$HistoryPath,

        [Parameter(Mandatory = $true)]
        [string]$Url
    )

    if (-not (Test-Path -LiteralPath $HistoryPath -PathType Leaf)) {
        return 0
    }

    try {
        $history = Get-Content -LiteralPath $HistoryPath -Raw | ConvertFrom-Json
        $entry = @($history.entries | Where-Object { $_.url -eq $Url } | Select-Object -First 1)
        if ($entry.Count -eq 0) {
            return 0
        }
        return [int]$entry[0].visit_count
    } catch {
        # The persistence layer writes atomically, but tolerate a polling read
        # that races the replacement and simply try again on the next interval.
        return 0
    }
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
        (Join-Path $StateRoot 'lifecycle-shutdown.log'),
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

function Export-ProductSmokeDiagnostics {
    param(
        [Parameter(Mandatory = $true)]
        [string]$DiagnosticsRoot,

        [Parameter(Mandatory = $true)]
        [string]$RuntimeRoot,

        [Parameter(Mandatory = $true)]
        [string]$StateRoot,

        [Parameter(Mandatory = $true)]
        [string]$ServerOut,

        [Parameter(Mandatory = $true)]
        [string]$ServerErr,

        [System.Diagnostics.Process]$Browser,
        [System.Diagnostics.Process]$RestoredBrowser
    )

    Remove-Item -Recurse -Force $DiagnosticsRoot -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $DiagnosticsRoot | Out-Null

    foreach ($name in @('session.json', 'history.json', 'bookmarks.json', 'workspaces.json', 'settings.json', 'lifecycle-shutdown.log')) {
        $source = Join-Path $StateRoot $name
        if (Test-Path -LiteralPath $source -PathType Leaf) {
            Copy-Item -LiteralPath $source -Destination (Join-Path $DiagnosticsRoot $name) -Force
        }
    }

    foreach ($candidate in @(
        @{ Source = $ServerOut; Name = 'http.stdout.log' },
        @{ Source = $ServerErr; Name = 'http.stderr.log' },
        @{ Source = (Join-Path $RuntimeRoot 'debug.log'); Name = 'debug.log' },
        @{ Source = (Join-Path $StateRoot 'debug.log'); Name = 'state-debug.log' },
        @{ Source = (Join-Path $StateRoot 'default/debug.log'); Name = 'default-debug.log' }
    )) {
        if (Test-Path -LiteralPath $candidate.Source -PathType Leaf) {
            Copy-Item -LiteralPath $candidate.Source -Destination (Join-Path $DiagnosticsRoot $candidate.Name) -Force
        }
    }

    $trace = @(
        "captured_utc=$([DateTime]::UtcNow.ToString('O'))",
        "runtime_root=$RuntimeRoot",
        "state_root=$StateRoot",
        "browser_present=$($null -ne $Browser)",
        "browser_exited=$($null -ne $Browser -and $Browser.HasExited)",
        "restored_browser_present=$($null -ne $RestoredBrowser)",
        "restored_browser_exited=$($null -ne $RestoredBrowser -and $RestoredBrowser.HasExited)"
    )
    Set-Content -LiteralPath (Join-Path $DiagnosticsRoot 'shutdown-trace.log') -Encoding UTF8 -Value $trace

    try {
        $since = (Get-Date).AddMinutes(-5)
        Get-WinEvent -FilterHashtable @{ LogName = 'Application'; StartTime = $since } -ErrorAction Stop |
            Where-Object { $_.Message -match 'openbrowser|libcef|chrome_elf' } |
            Select-Object -First 20 TimeCreated, Id, ProviderName, LevelDisplayName, Message |
            Format-List |
            Out-File -LiteralPath (Join-Path $DiagnosticsRoot 'windows-event-log.txt') -Encoding utf8
    } catch {
        Set-Content -LiteralPath (Join-Path $DiagnosticsRoot 'windows-event-log.txt') -Encoding UTF8 -Value "Unavailable: $($_.Exception.Message)"
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
$diagnosticsRoot = Join-Path $PWD 'product-smoke-diagnostics'

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
    $firstHistoryVisitCount = Get-HistoryVisitCount -HistoryPath $historyFile -Url $smokeUrl
    if ($firstHistoryVisitCount -lt 1) {
        throw 'Committed smoke URL did not record a history visit count.'
    }

    if ($firstExitCode -ne 0) {
        throw "Openbrowser returned a non-zero status after window close."
    }

    $firstRequestCount = Get-RequestCount -LogPath $serverErr -RequestTarget $requestTarget

    $restoredBrowser = Start-Browser `
        -Exe $exe `
        -WorkingDirectory $runtimeRoot `
        -Arguments @(
            '--disable-gpu',
            "--storage-dir=$stateRoot",
            "--session-file=$sessionFile"
        )

    # A restored page may be satisfied from the browser cache, so a second
    # server GET is not a reliable proof of restoration. The history bridge is
    # updated only after a committed navigation, making visit_count the durable
    # product-level signal that the restored tab actually loaded.
    Wait-Until -Description 'restored packaged navigation commit' -TimeoutSeconds 25 -Condition {
        if ($restoredBrowser.HasExited) {
            throw "Openbrowser exited during restored navigation (code $($restoredBrowser.ExitCode))."
        }
        return (Get-HistoryVisitCount -HistoryPath $historyFile -Url $smokeUrl) -gt $firstHistoryVisitCount
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

    $finalHistoryVisitCount = Get-HistoryVisitCount -HistoryPath $historyFile -Url $smokeUrl
    if ($finalHistoryVisitCount -le $firstHistoryVisitCount) {
        throw 'Restored navigation commit was not persisted in history.'
    }

    $finalRequestCount = Get-RequestCount -LogPath $serverErr -RequestTarget $requestTarget
    Write-Host "Packaged product lifecycle smoke passed."
    Write-Host "  URL: $smokeUrl"
    Write-Host "  Observed network requests: $finalRequestCount"
    Write-Host "  History visits: $finalHistoryVisitCount"
    Write-Host "  Session restore navigation commit: verified"
    Write-Host "  History persistence: verified"
    Write-Host "  Clean shutdown x2: verified"
    Write-Host "  ZIP SHA-256: $actualSha"
} finally {
    Export-ProductSmokeDiagnostics `
        -DiagnosticsRoot $diagnosticsRoot `
        -RuntimeRoot $runtimeRoot `
        -StateRoot $stateRoot `
        -ServerOut $serverOut `
        -ServerErr $serverErr `
        -Browser $browser `
        -RestoredBrowser $restoredBrowser

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
