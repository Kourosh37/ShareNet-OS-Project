$ErrorActionPreference = "Stop"
$ErrorView = "ConciseView"

trap {
    Write-Host "Error: $($_.Exception.Message)"
    exit 1
}

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$Out = Join-Path $Root "dist\windows"
$LogDir = Join-Path $Root "build\logs"
New-Item -ItemType Directory -Force -Path $Out, $LogDir | Out-Null
$ProgressActivity = "ShareNet Windows CLI Build"
$TotalSteps = 2
$script:StepIndex = 0

function Format-Duration {
    param([TimeSpan]$Duration)
    if ($Duration.TotalHours -ge 1) { return "{0:h\:mm\:ss}" -f $Duration }
    return "{0:mm\:ss}" -f $Duration
}

function Start-Step {
    param([string]$Title, [string]$Estimate)
    $script:StepIndex += 1
    $Percent = [math]::Round((($script:StepIndex - 1) / $TotalSteps) * 100)
    Write-Progress -Activity $ProgressActivity -Status "$Title (starting)" -PercentComplete $Percent
    Write-Host ("[{0}/{1}] {2}" -f $script:StepIndex, $TotalSteps, $Title)
    if ($Estimate) { Write-Host "    Estimate: $Estimate" }
    return [System.Diagnostics.Stopwatch]::StartNew()
}

function Complete-Step {
    param([string]$Title, [System.Diagnostics.Stopwatch]$Timer)
    $Timer.Stop()
    $Percent = [math]::Round(($script:StepIndex / $TotalSteps) * 100)
    Write-Progress -Activity $ProgressActivity -Status "$Title completed in $(Format-Duration $Timer.Elapsed)" -PercentComplete $Percent
    Write-Host "    Done in $(Format-Duration $Timer.Elapsed)"
}

function Ask-YesNo {
    param([string]$Question)
    $Answer = Read-Host "$Question [y/N]"
    return $Answer -match "^(y|yes)$"
}

function Test-CommandExists {
    param([string]$Name)
    return [bool](Get-Command $Name -ErrorAction SilentlyContinue)
}

function Invoke-Quiet {
    param(
        [string]$Title,
        [string]$LogName,
        [scriptblock]$Command,
        [string]$Estimate = ""
    )

    $LogPath = Join-Path $LogDir $LogName
    $Timer = Start-Step $Title $Estimate
    $StartedAt = Get-Date
    @("[$StartedAt] START: $Title", "Estimate: $(if ($Estimate) { $Estimate } else { 'short' })", "") |
        Set-Content -LiteralPath $LogPath -Encoding UTF8

    & $Command *>> $LogPath
    if ($LASTEXITCODE -ne 0) {
        $Timer.Stop()
        Add-Content -LiteralPath $LogPath -Encoding UTF8 -Value @("", "[$(Get-Date)] FAILED after $(Format-Duration $Timer.Elapsed)")
        Write-Host "Failed. Last log lines from ${LogPath}:"
        Get-Content $LogPath -Tail 80
        throw "$Title failed. Full log: $LogPath"
    }

    Add-Content -LiteralPath $LogPath -Encoding UTF8 -Value @("", "[$(Get-Date)] DONE after $(Format-Duration $Timer.Elapsed)")
    Complete-Step $Title $Timer
}

function Install-WindowsCompiler {
    if (Test-CommandExists "scoop") {
        Invoke-Quiet "Installing GCC with Scoop" "scoop-gcc.log" { scoop install gcc } "Usually 2-10 minutes."
        return
    }
    if (Test-CommandExists "winget") {
        Invoke-Quiet "Installing GCC with winget" "winget-gcc.log" {
            winget install --id BrechtSanders.WinLibs.POSIX.UCRT -e --accept-package-agreements --accept-source-agreements
        } "Usually 2-10 minutes."
        Write-Host "If gcc is still not on PATH, open a new PowerShell window and rerun this script."
        return
    }
    throw "No supported package manager found. Install gcc/MinGW-w64 manually or install Scoop/winget."
}

function Invoke-Compile {
    param([string]$Name)
    $CompileArgs = $args
    Invoke-Quiet "Building $Name" "build-$Name.log" {
        & $CC @CompileArgs
    }
}

$CC = if ($env:CC) { $env:CC } else { "gcc" }
$CFlags = @("-Wall", "-Wextra", "-std=c11", "-I$Root\include")
$Common = @(
    "$Root\src\common\protocol.c",
    "$Root\src\common\socket_utils.c",
    "$Root\src\common\file_utils.c",
    "$Root\src\common\chunk.c"
)

if (-not (Test-CommandExists $CC)) {
    Write-Host "Compiler '$CC' was not found."
    if (Ask-YesNo "Install a Windows C compiler now?") {
        Install-WindowsCompiler
    } else {
        throw "Cannot build without a C compiler."
    }
}

Invoke-Compile "windows-cli-server" @CFlags -o "$Out\sharenet_server.exe" @Common "$Root\src\server\server_main.c" "$Root\src\server\server.c" -lws2_32
Invoke-Compile "windows-cli-client" @CFlags -o "$Out\sharenet_client.exe" @Common "$Root\src\client\client_main.c" "$Root\src\client\client.c" -lws2_32

Write-Host "Windows CLI executables written to $Out"
Write-Progress -Activity $ProgressActivity -Completed
