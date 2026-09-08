param(
    [Parameter(Mandatory = $true)]
    [string]$Executable
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Wait-Until {
    param(
        [Parameter(Mandatory = $true)]
        [scriptblock]$Condition,

        [Parameter(Mandatory = $true)]
        [string]$Description,

        [int]$TimeoutSeconds = 30
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

function Get-SessionTab {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Session
    )

    $tabs = @($Session.tabs)
    if ($tabs.Count -eq 0) {
        throw 'Openbrowser compatibility run produced no persisted tab.'
    }
    $active = $tabs | Where-Object { $_.id -eq $Session.active_tab_id } | Select-Object -First 1
    if ($null -ne $active) {
        return $active
    }
    return $tabs[0]
}

$requiredEnvironment = @(
    'OPENBROWSER_COMPAT_SCENARIO',
    'OPENBROWSER_COMPAT_URL',
    'OPENBROWSER_COMPAT_RESULT_FILE',
    'OPENBROWSER_COMPAT_STORAGE_DIR',
    'OPENBROWSER_COMPAT_STATUS_URL'
)
foreach ($name in $requiredEnvironment) {
    if ([string]::IsNullOrWhiteSpace((Get-Item -Path "Env:$name").Value)) {
        throw "Missing compatibility harness environment variable: $name"
    }
}

$scenario = $env:OPENBROWSER_COMPAT_SCENARIO
$url = $env:OPENBROWSER_COMPAT_URL
$resultPath = $env:OPENBROWSER_COMPAT_RESULT_FILE
$storageDirectory = $env:OPENBROWSER_COMPAT_STORAGE_DIR
$statusUrl = $env:OPENBROWSER_COMPAT_STATUS_URL
$sessionPath = Join-Path $storageDirectory 'session.json'
$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path

New-Item -ItemType Directory -Force -Path $storageDirectory | Out-Null
$browser = $null

try {
    $browser = Start-Process `
        -FilePath $resolvedExecutable `
        -ArgumentList @(
            "--url=$url",
            '--disable-gpu',
            "--storage-dir=$storageDirectory",
            "--session-file=$sessionPath"
        ) `
        -WorkingDirectory (Split-Path -Parent $resolvedExecutable) `
        -PassThru

    Wait-Until -Description 'Openbrowser compatibility window' -Condition {
        if ($browser.HasExited) {
            throw "Openbrowser exited before the compatibility page was ready (code $($browser.ExitCode))."
        }
        $browser.Refresh()
        return $browser.MainWindowHandle -ne [IntPtr]::Zero
    }

    Wait-Until -Description 'local compatibility fixture readiness' -Condition {
        if ($browser.HasExited) {
            throw "Openbrowser exited while loading the compatibility page (code $($browser.ExitCode))."
        }
        try {
            $probe = Invoke-WebRequest -Uri $statusUrl -TimeoutSec 2
            return $probe.StatusCode -eq 200
        } catch {
            return $false
        }
    }

    if (-not $browser.CloseMainWindow()) {
        throw 'Openbrowser rejected the graceful compatibility-run close request.'
    }
    if (-not $browser.WaitForExit(20000)) {
        throw 'Openbrowser did not exit after the graceful compatibility-run close request.'
    }
    if ($browser.ExitCode -ne 0) {
        throw "Openbrowser returned unexpected exit code $($browser.ExitCode)."
    }
    if (-not (Test-Path -LiteralPath $sessionPath -PathType Leaf)) {
        throw 'Openbrowser compatibility run did not persist a session after shutdown.'
    }

    $session = Get-Content -LiteralPath $sessionPath -Raw | ConvertFrom-Json
    if ($session.clean_shutdown -ne $true) {
        throw 'Openbrowser compatibility run did not record a clean shutdown.'
    }
    $tab = Get-SessionTab -Session $session
    $result = [ordered]@{
        schema_version = 1
        scenario_id = $scenario
        final_url = [string]$tab.url
        title = [string]$tab.title
        events = @('navigation_committed', 'fixture_ready', 'clean_shutdown')
    }
    $result | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $resultPath -Encoding UTF8
} finally {
    if ($null -ne $browser -and -not $browser.HasExited) {
        & taskkill.exe /PID $browser.Id /T /F | Out-Null
    }
}
