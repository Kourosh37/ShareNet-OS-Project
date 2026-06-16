$ErrorActionPreference = "Stop"
$ErrorView = "ConciseView"

trap {
    Write-Host "Error: $($_.Exception.Message)"
    exit 1
}

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$Build = Join-Path $Root "build\qt-windows"
$Out = Join-Path $Root "dist\qt-windows"
$LogDir = Join-Path $Root "build\logs"
New-Item -ItemType Directory -Force -Path $Build, $Out, $LogDir | Out-Null

$Triplet = "x64-mingw-dynamic"
$ManifestDir = Join-Path $Root "src\qt"
$TotalSteps = 6
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
    if ($Estimate) {
        Write-Host "Estimate: $Estimate"
    }
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

function Update-RunningProgress {
    param([string]$Line, [string]$FallbackTitle)
    $Base = ($script:StepIndex - 1) / $TotalSteps * 100
    $Span = 100 / $TotalSteps

    if ($Line -match "Installing\s+(\d+)/(\d+)\s+(.+?)\.\.\.") {
        $Index = [int]$Matches[1]
        $Total = [int]$Matches[2]
        $Name = Get-ShortPackageName $Matches[3]
        $Percent = [math]::Round($Base + (($Index - 1) / [math]::Max($Total, 1)) * $Span)
        Write-ProgressText $Percent "Installing $Index/$Total $Name"
        return
    }

    if ($Line -match "Building\s+(.+?)\.\.\.") {
        Write-ProgressText ([math]::Round($Base + ($Span * 0.5))) "Building $(Get-ShortPackageName $Matches[1])"
        return
    }

    if ($Line -match "Configuring\s+(.+)") {
        Write-ProgressText ([math]::Round($Base + ($Span * 0.35))) "Configuring $($Matches[1])"
        return
    }

    if ($Line -match "Downloading\s+(.+)") {
        Write-ProgressText ([math]::Round($Base + ($Span * 0.2))) "Downloading dependencies"
        return
    }

    Write-ProgressText ([math]::Round($Base + ($Span * 0.1))) $FallbackTitle
}

function Get-ShortPackageName {
    param([string]$Name)
    $Short = $Name
    if ($Short -match "^([^:\[]+)") {
        $Short = $Matches[1]
    }
    return $Short.Trim()
}

function Show-LogFailure {
    param([string]$LogPath)
    Write-Host ""
    Write-Host "Failed. Important log lines:"
    $Important = Get-Content $LogPath -ErrorAction SilentlyContinue |
        Select-String -Pattern "CMake Error|error:|failed with|FAILED after|No such file|already exists" |
        Select-Object -Last 20
    if ($Important) {
        $Important | ForEach-Object { Write-Host $_.Line }
    } else {
        Get-Content $LogPath -Tail 40 -ErrorAction SilentlyContinue | ForEach-Object { Write-Host $_ }
    }
    Write-Host "Full log: $LogPath"
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
    @(
        "[$StartedAt] START: $Title"
        "Estimate: $(if ($Estimate) { $Estimate } else { 'short' })"
        ""
    ) | Set-Content -LiteralPath $LogPath -Encoding UTF8

    $PreviousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $global:LASTEXITCODE = 0
    try {
        & $Command 2>&1 | ForEach-Object {
            $Line = $_.ToString()
            Add-Content -LiteralPath $LogPath -Encoding UTF8 -Value $Line
            Update-RunningProgress $Line $Title
        }
        $ExitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $PreviousErrorActionPreference
    }
    if ($ExitCode -ne 0) {
        $Timer.Stop()
        $FinishedAt = Get-Date
        Add-Content -LiteralPath $LogPath -Encoding UTF8 -Value @(
            ""
            "[$FinishedAt] FAILED after $(Format-Duration $Timer.Elapsed)"
        )
        Show-LogFailure $LogPath
        throw "$Title failed. Full log: $LogPath"
    }

    $FinishedAt = Get-Date
    Add-Content -LiteralPath $LogPath -Encoding UTF8 -Value @(
        ""
        "[$FinishedAt] DONE after $(Format-Duration $Timer.Elapsed)"
    )
    Complete-Step $Title $Timer
}

function Find-VcpkgRoot {
    if ($env:VCPKG_ROOT -and (Test-Path (Join-Path $env:VCPKG_ROOT "vcpkg.exe"))) {
        return $env:VCPKG_ROOT
    }

    $Candidates = @(
        "$env:USERPROFILE\scoop\apps\vcpkg\current",
        "$env:USERPROFILE\vcpkg",
        "C:\vcpkg"
    )

    foreach ($Candidate in $Candidates) {
        if (Test-Path (Join-Path $Candidate "vcpkg.exe")) {
            return $Candidate
        }
    }

    return $null
}

function Test-VcpkgQt {
    param([string]$VcpkgRoot)
    if (-not $VcpkgRoot) { return $false }
    $Installed = Join-Path $VcpkgRoot "installed\$Triplet"
    return (Test-Path (Join-Path $Installed "share\Qt6")) -or
           (Test-Path (Join-Path $Installed "share\qtbase")) -or
           (Test-Path (Join-Path $Installed "lib\cmake\Qt6"))
}

function Find-QtPrefix {
    if ($env:QT_DIR) { return $env:QT_DIR }
    if ($env:CMAKE_PREFIX_PATH) { return $env:CMAKE_PREFIX_PATH }

    $Candidates = @(
        "$env:USERPROFILE\scoop\apps\qt6\current",
        "$env:USERPROFILE\scoop\apps\qt\current",
        "C:\Qt\6.7.0\mingw_64",
        "C:\Qt\6.6.0\mingw_64"
    )
    foreach ($Candidate in $Candidates) {
        if (Test-Path (Join-Path $Candidate "lib\cmake")) { return $Candidate }
        if (Test-Path $Candidate) {
            $Nested = Get-ChildItem $Candidate -Directory -Recurse -ErrorAction SilentlyContinue |
                Where-Object { Test-Path (Join-Path $_.FullName "lib\cmake") } |
                Select-Object -First 1
            if ($Nested) { return $Nested.FullName }
        }
    }
    return $null
}

function Install-VcpkgIfMissing {
    $VcpkgRoot = Find-VcpkgRoot
    if ($VcpkgRoot) { return $VcpkgRoot }

    if (-not (Test-CommandExists "scoop")) {
        throw "vcpkg was not found and Scoop is not available. Install vcpkg or set VCPKG_ROOT."
    }

    Write-Host "Installing vcpkg with Scoop..."
    Invoke-Quiet "Installing vcpkg" "scoop-vcpkg.log" { scoop install vcpkg } "Usually 1-5 minutes."
    $VcpkgRoot = Find-VcpkgRoot
    if (-not $VcpkgRoot) {
        throw "vcpkg installation finished, but vcpkg.exe was not found. Set VCPKG_ROOT manually."
    }
    return $VcpkgRoot
}

function Install-Qt {
    $VcpkgRoot = Install-VcpkgIfMissing
    $VcpkgExe = Join-Path $VcpkgRoot "vcpkg.exe"

    if (-not (Test-Path $VcpkgExe)) {
        throw "vcpkg.exe was not found at $VcpkgExe"
    }

    if (-not (Test-Path (Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"))) {
        $Bootstrap = Join-Path $VcpkgRoot "bootstrap-vcpkg.bat"
        if (Test-Path $Bootstrap) {
            Invoke-Quiet "Bootstrapping vcpkg" "vcpkg-bootstrap.log" { & $Bootstrap } "Usually 1-3 minutes."
        }
    }

    $env:VCPKG_DEFAULT_TRIPLET = $Triplet
    $env:VCPKG_DEFAULT_HOST_TRIPLET = $Triplet

    Repair-VcpkgToolCache $VcpkgRoot

    Write-Host "Installing minimal Qt dependencies with vcpkg. First run can take a long time."
    Invoke-Quiet "Installing Qt dependencies" "vcpkg-qtbase.log" {
        & $VcpkgExe install "--x-manifest-root=$ManifestDir" "--triplet=$Triplet" "--host-triplet=$Triplet"
    } "First run is commonly 30-120 minutes; cached reruns are much faster."

    return $VcpkgRoot
}

function Repair-VcpkgToolCache {
    param([string]$VcpkgRoot)
    $PerlRoot = Join-Path $VcpkgRoot "downloads\tools\perl"
    if (-not (Test-Path $PerlRoot)) { return }

    Get-ChildItem $PerlRoot -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match "^\d+\.\d+\.\d+\.\d+$" } |
        ForEach-Object {
            Write-Host "Refreshing vcpkg Perl tool cache: $($_.Name)"
            Remove-Item -LiteralPath $_.FullName -Recurse -Force -ErrorAction SilentlyContinue
        }
}

function Find-WindeployQt {
    param([string]$QtPrefix, [string]$VcpkgRoot)

    $PathCommand = Get-Command windeployqt -ErrorAction SilentlyContinue
    if ($PathCommand) { return $PathCommand.Source }

    $Candidates = @()
    if ($QtPrefix) {
        $Candidates += (Join-Path $QtPrefix "bin\windeployqt.exe")
    }
    if ($VcpkgRoot) {
        $Candidates += (Join-Path $VcpkgRoot "installed\$Triplet\tools\Qt6\bin\windeployqt.exe")
        $Candidates += (Join-Path $VcpkgRoot "installed\$Triplet\tools\qt6\bin\windeployqt.exe")
        $Candidates += (Join-Path $VcpkgRoot "installed\$Triplet\bin\windeployqt.exe")
    }

    foreach ($Candidate in $Candidates) {
        if (Test-Path $Candidate) { return $Candidate }
    }

    return $null
}

function Copy-VcpkgRuntimeDlls {
    param([string]$VcpkgRoot)
    if (-not $VcpkgRoot) { return }
    $BinDir = Join-Path $VcpkgRoot "installed\$Triplet\bin"
    if (Test-Path $BinDir) {
        Copy-Item -Force (Join-Path $BinDir "*.dll") $Out -ErrorAction SilentlyContinue
    }
}

if (-not (Test-CommandExists "cmake")) {
    throw "cmake was not found. Install CMake first."
}

$QtPrefix = Find-QtPrefix
$VcpkgRoot = Find-VcpkgRoot
$UseVcpkg = $false

if (-not $QtPrefix -and (Test-VcpkgQt $VcpkgRoot)) {
    $UseVcpkg = $true
}

if (-not $QtPrefix) {
    Write-Host "Qt was not found in QT_DIR/CMAKE_PREFIX_PATH."
    if (Ask-YesNo "Install Qt automatically if possible?") {
        $VcpkgRoot = Install-Qt
        if (Test-VcpkgQt $VcpkgRoot) {
            $UseVcpkg = $true
        } else {
            $QtPrefix = Find-QtPrefix
        }
    }
}

if (-not $QtPrefix -and -not $UseVcpkg) {
    Write-Host "Qt is unavailable. Install it with this script or set QT_DIR/CMAKE_PREFIX_PATH."
    exit 1
}

$ConfigureArgs = @(
    "-S", "$Root\src\qt",
    "-B", $Build,
    "-DCMAKE_BUILD_TYPE=Release"
)

if (Test-CommandExists "ninja") {
    $ConfigureArgs += @("-G", "Ninja")
}

if ($UseVcpkg) {
    $env:VCPKG_DEFAULT_TRIPLET = $Triplet
    $env:VCPKG_DEFAULT_HOST_TRIPLET = $Triplet

    $Toolchain = Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"
    $ConfigureArgs += @(
        "-DCMAKE_TOOLCHAIN_FILE=$Toolchain",
        "-DVCPKG_MANIFEST_DIR=$ManifestDir",
        "-DVCPKG_TARGET_TRIPLET=$Triplet",
        "-DVCPKG_HOST_TRIPLET=$Triplet",
        "-DCMAKE_C_COMPILER=gcc",
        "-DCMAKE_CXX_COMPILER=g++"
    )
} else {
    $ConfigureArgs += "-DCMAKE_PREFIX_PATH=$QtPrefix"
}

Invoke-Quiet "Configuring Qt project" "qt-windows-configure.log" { cmake @ConfigureArgs } "Usually under 2 minutes after Qt is installed."
Invoke-Quiet "Building Qt executables" "qt-windows-build.log" { cmake --build $Build --config Release } "Usually under 5 minutes after Qt is installed."

$Client = Get-ChildItem $Build -Recurse -Filter "sharenet_qt_client.exe" | Select-Object -First 1
$Server = Get-ChildItem $Build -Recurse -Filter "sharenet_qt_server.exe" | Select-Object -First 1
if (-not $Client -or -not $Server) {
    throw "Qt build finished, but one or both executables were not found."
}

Copy-Item -Force $Client.FullName $Out
Copy-Item -Force $Server.FullName $Out

$Deploy = Find-WindeployQt $QtPrefix $VcpkgRoot
if ($Deploy) {
    Invoke-Quiet "Deploying Qt client runtime" "qt-windows-deploy-client.log" {
        & $Deploy (Join-Path $Out "sharenet_qt_client.exe")
    } "Usually under 1 minute."
    Invoke-Quiet "Deploying Qt server runtime" "qt-windows-deploy-server.log" {
        & $Deploy (Join-Path $Out "sharenet_qt_server.exe")
    } "Usually under 1 minute."
} else {
    Copy-VcpkgRuntimeDlls $VcpkgRoot
    Write-Host "windeployqt was not found. Runtime DLLs were copied when available; Qt plugins may still be required."
}

Write-Host "Qt Windows executables written to $Out"
