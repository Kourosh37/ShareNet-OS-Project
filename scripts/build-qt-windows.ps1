$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$Build = Join-Path $Root "build\qt-windows"
$Out = Join-Path $Root "dist\qt-windows"
$LogDir = Join-Path $Root "build\logs"
New-Item -ItemType Directory -Force -Path $Build, $Out, $LogDir | Out-Null

$Triplet = "x64-mingw-dynamic"

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
        [scriptblock]$Command
    )

    $LogPath = Join-Path $LogDir $LogName
    Write-Host "$Title..."
    & $Command *> $LogPath
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Failed. Last log lines from ${LogPath}:"
        Get-Content $LogPath -Tail 80
        throw "$Title failed. Full log: $LogPath"
    }
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
    Invoke-Quiet "Installing vcpkg" "scoop-vcpkg.log" { scoop install vcpkg }
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
            Invoke-Quiet "Bootstrapping vcpkg" "vcpkg-bootstrap.log" { & $Bootstrap }
        }
    }

    $env:VCPKG_DEFAULT_TRIPLET = $Triplet
    $env:VCPKG_DEFAULT_HOST_TRIPLET = $Triplet

    Write-Host "Installing Qt base with vcpkg. First run can take a long time."
    Invoke-Quiet "Installing Qt dependencies" "vcpkg-qtbase.log" {
        & $VcpkgExe install "qtbase" "--triplet=$Triplet" "--host-triplet=$Triplet"
    }

    return $VcpkgRoot
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
        "-DVCPKG_TARGET_TRIPLET=$Triplet",
        "-DVCPKG_HOST_TRIPLET=$Triplet",
        "-DCMAKE_C_COMPILER=gcc",
        "-DCMAKE_CXX_COMPILER=g++"
    )
} else {
    $ConfigureArgs += "-DCMAKE_PREFIX_PATH=$QtPrefix"
}

Invoke-Quiet "Configuring Qt project" "qt-windows-configure.log" { cmake @ConfigureArgs }
Invoke-Quiet "Building Qt executables" "qt-windows-build.log" { cmake --build $Build --config Release }

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
    }
    Invoke-Quiet "Deploying Qt server runtime" "qt-windows-deploy-server.log" {
        & $Deploy (Join-Path $Out "sharenet_qt_server.exe")
    }
} else {
    Copy-VcpkgRuntimeDlls $VcpkgRoot
    Write-Host "windeployqt was not found. Runtime DLLs were copied when available; Qt plugins may still be required."
}

Write-Host "Qt Windows executables written to $Out"
