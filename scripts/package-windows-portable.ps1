$ErrorActionPreference = "Stop"
$ErrorView = "ConciseView"

trap {
    Write-Host "Error: $($_.Exception.Message)"
    exit 1
}

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$Dist = Join-Path $Root "dist"
$WinOut = Join-Path $Dist "windows"
$QtOut = Join-Path $Dist "qt-windows"
$PackageRoot = Join-Path $Dist "package"
$Portable = Join-Path $PackageRoot "ShareNet-Windows-Portable"
$ZipPath = Join-Path $Dist "ShareNet-Windows-Portable.zip"
$SfxPath = Join-Path $Dist "ShareNet-Windows-Portable.exe"
$LogDir = Join-Path $Root "build\logs"
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
$TotalSteps = 4
$script:StepIndex = 0
$script:LastProgressLength = 0

function Format-Duration {
    param([TimeSpan]$Duration)
    if ($Duration.TotalHours -ge 1) { return "{0:h\:mm\:ss}" -f $Duration }
    return "{0:mm\:ss}" -f $Duration
}

function Start-Step {
    param([string]$Title, [string]$Estimate)
    $script:StepIndex += 1
    $Percent = [math]::Round((($script:StepIndex - 1) / $TotalSteps) * 100)
    Write-ProgressText $Percent ("[{0}/{1}] {2}" -f $script:StepIndex, $TotalSteps, $Title)
    Write-Host ""
    if ($Estimate) { Write-Host "Estimate: $Estimate" }
    return [System.Diagnostics.Stopwatch]::StartNew()
}

function Complete-Step {
    param([string]$Title, [System.Diagnostics.Stopwatch]$Timer)
    $Timer.Stop()
    $Percent = [math]::Round(($script:StepIndex / $TotalSteps) * 100)
    Write-ProgressText $Percent ("Done: {0} in {1}" -f $Title, (Format-Duration $Timer.Elapsed))
    Write-Host ""
}

function Write-ProgressText {
    param([int]$Percent, [string]$Status)
    if ($Percent -lt 0) { $Percent = 0 }
    if ($Percent -gt 100) { $Percent = 100 }
    $Width = 24
    $Filled = [math]::Floor($Width * $Percent / 100)
    $Bar = ("#" * $Filled).PadRight($Width, "-")
    $MaxStatus = 54
    if ($Status.Length -gt $MaxStatus) {
        $Status = $Status.Substring(0, $MaxStatus - 3) + "..."
    }
    $Text = ("`r[{0}] {1,3}%  {2}" -f $Bar, $Percent, $Status)
    $Pad = ""
    if ($script:LastProgressLength -gt $Text.Length) {
        $Pad = " " * ($script:LastProgressLength - $Text.Length)
    }
    Write-Host -NoNewline ($Text + $Pad)
    $script:LastProgressLength = $Text.Length
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
    @("[$(Get-Date)] START: $Title", "Estimate: $(if ($Estimate) { $Estimate } else { 'short' })", "") |
        Set-Content -LiteralPath $LogPath -Encoding UTF8

    $PreviousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $global:LASTEXITCODE = 0
    try {
        & $Command *>> $LogPath
        $ExitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $PreviousErrorActionPreference
    }
    if ($ExitCode -ne 0) {
        $Timer.Stop()
        Add-Content -LiteralPath $LogPath -Encoding UTF8 -Value @("", "[$(Get-Date)] FAILED after $(Format-Duration $Timer.Elapsed)")
        Write-Host "Failed. Last log lines from ${LogPath}:"
        Get-Content $LogPath -Tail 80
        throw "$Title failed. Full log: $LogPath"
    }

    Add-Content -LiteralPath $LogPath -Encoding UTF8 -Value @("", "[$(Get-Date)] DONE after $(Format-Duration $Timer.Elapsed)")
    Complete-Step $Title $Timer
}

function Install-7Zip {
    if (Test-CommandExists "scoop") {
        Invoke-Quiet "Installing 7-Zip with Scoop" "scoop-7zip.log" { scoop install 7zip } "Usually 1-3 minutes."
        return
    }
    if (Test-CommandExists "winget") {
        Invoke-Quiet "Installing 7-Zip with winget" "winget-7zip.log" {
            winget install --id 7zip.7zip -e --accept-package-agreements --accept-source-agreements
        } "Usually 1-3 minutes."
        return
    }
    throw "No supported package manager found for 7-Zip."
}

function Find-7Zip {
    $Command = Get-Command 7z -ErrorAction SilentlyContinue
    if ($Command) { return $Command.Source }

    $Candidates = @(
        "$env:USERPROFILE\scoop\apps\7zip\current\7z.exe",
        "$env:ProgramFiles\7-Zip\7z.exe",
        "${env:ProgramFiles(x86)}\7-Zip\7z.exe"
    )
    foreach ($Candidate in $Candidates) {
        if ($Candidate -and (Test-Path $Candidate)) { return $Candidate }
    }
    return $null
}

function Find-7ZipSfx {
    $Candidates = @(
        "$env:USERPROFILE\scoop\apps\7zip\current\7z.sfx",
        "$env:USERPROFILE\scoop\apps\7zip\current\7zCon.sfx",
        "$env:ProgramFiles\7-Zip\7z.sfx",
        "$env:ProgramFiles\7-Zip\7zCon.sfx",
        "${env:ProgramFiles(x86)}\7-Zip\7z.sfx",
        "${env:ProgramFiles(x86)}\7-Zip\7zCon.sfx"
    )
    foreach ($Candidate in $Candidates) {
        if ($Candidate -and (Test-Path $Candidate)) { return $Candidate }
    }
    return $null
}

function Copy-DirectoryContents {
    param([string]$Source, [string]$Target)
    if (-not (Test-Path $Source)) { return }
    New-Item -ItemType Directory -Force -Path $Target | Out-Null
    Copy-Item -Force -Recurse (Join-Path $Source "*") $Target
}

Write-Host "Building Windows CLI executables..."
Invoke-Quiet "Building Windows CLI executables" "package-build-windows.log" {
    & powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "build-windows.ps1")
} "Usually under 2 minutes."

try {
    Invoke-Quiet "Building Qt Windows executables" "package-build-qt-windows.log" {
        & powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "build-qt-windows.ps1")
    } "First run can take a long time; cached reruns are faster."
} catch {
    Write-Host "Qt packaging skipped: $($_.Exception.Message)"
}

Remove-Item -Recurse -Force $Portable -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $Portable | Out-Null

Copy-DirectoryContents $WinOut (Join-Path $Portable "windows")
Copy-DirectoryContents $QtOut (Join-Path $Portable "qt-windows")

New-Item -ItemType Directory -Force -Path (Join-Path $Portable "server_files") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $Portable "client_files") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $Portable "downloads") | Out-Null

$Launcher = @'
@echo off
setlocal
cd /d "%~dp0"
:menu
cls
echo ShareNet Portable
echo.
echo 1. Run Qt Server
echo 2. Run Qt Client
echo 3. Run CLI Server
echo 4. Run CLI Client
echo 5. Exit
echo.
set /p choice=Select:
if "%choice%"=="1" if exist "qt-windows\sharenet_qt_server.exe" start "" "qt-windows\sharenet_qt_server.exe" & goto menu
if "%choice%"=="2" if exist "qt-windows\sharenet_qt_client.exe" start "" "qt-windows\sharenet_qt_client.exe" & goto menu
if "%choice%"=="3" start "ShareNet CLI Server" cmd /k "windows\sharenet_server.exe" & goto menu
if "%choice%"=="4" start "ShareNet CLI Client" cmd /k "windows\sharenet_client.exe" & goto menu
if "%choice%"=="5" exit /b 0
echo Option unavailable or invalid.
pause
goto menu
'@
Set-Content -LiteralPath (Join-Path $Portable "ShareNetLauncher.cmd") -Value $Launcher -Encoding ASCII

$Readme = @'
ShareNet Windows Portable

This folder is self-contained. No compiler or Qt installation is required on the target computer.

Run ShareNetLauncher.cmd and choose the server/client variant you want.

Keep DLL files next to the executables. Do not move individual EXE files out of this folder unless you also move their DLLs.
'@
Set-Content -LiteralPath (Join-Path $Portable "README-PORTABLE.txt") -Value $Readme -Encoding UTF8

Remove-Item -Force $ZipPath -ErrorAction SilentlyContinue
$ZipTimer = Start-Step "Creating portable ZIP" "Usually under 5 minutes."
Compress-Archive -Path $Portable -DestinationPath $ZipPath -Force
Complete-Step "Creating portable ZIP" $ZipTimer
Write-Host "Portable ZIP written to $ZipPath"

$SevenZip = Find-7Zip
if (-not $SevenZip) {
    if (Ask-YesNo "7-Zip is required for a single self-extracting EXE. Install 7-Zip now?") {
        Install-7Zip
        $SevenZip = Find-7Zip
    }
}

if ($SevenZip) {
    $Archive7z = Join-Path $PackageRoot "ShareNet-Windows-Portable.7z"
    Remove-Item -Force $Archive7z, $SfxPath -ErrorAction SilentlyContinue
    Push-Location $PackageRoot
    Invoke-Quiet "Creating 7z archive" "package-7z.log" {
        & $SevenZip a -t7z $Archive7z "ShareNet-Windows-Portable\*"
    } "Usually under 5 minutes."
    Pop-Location

    $SfxModule = Find-7ZipSfx
    if ($SfxModule) {
        $Config = Join-Path $PackageRoot "sfx-config.txt"
        @'
;!@Install@!UTF-8!
Title="ShareNet Portable"
BeginPrompt="Extract and run ShareNet Portable?"
RunProgram="ShareNetLauncher.cmd"
;!@InstallEnd@!
'@ | Set-Content -LiteralPath $Config -Encoding UTF8

        $OutStream = [System.IO.File]::Create($SfxPath)
        foreach ($Part in @($SfxModule, $Config, $Archive7z)) {
            $Bytes = [System.IO.File]::ReadAllBytes($Part)
            $OutStream.Write($Bytes, 0, $Bytes.Length)
        }
        $OutStream.Close()
        Write-Host "Single self-extracting EXE written to $SfxPath"
    } else {
        Write-Host "7-Zip was found, but no SFX module was found. ZIP package is ready; single EXE was skipped."
    }
} else {
    Write-Host "7-Zip is unavailable. ZIP package is ready; single EXE was skipped."
}
