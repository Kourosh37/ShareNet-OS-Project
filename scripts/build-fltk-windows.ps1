$ErrorActionPreference = "Stop"
$ErrorView = "ConciseView"

trap {
    Write-Host "Error: $($_.Exception.Message)"
    exit 1
}

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$Build = Join-Path $Root "build\fltk-windows"
$Out = Join-Path $Root "dist\fltk-windows"
$LogDir = Join-Path $Root "build\logs"
$Triplet = "x64-mingw-dynamic"
$ManifestDir = Join-Path $Root "src\fltk"
New-Item -ItemType Directory -Force -Path $Build, $Out, $LogDir | Out-Null

function Ask-YesNo {
    param([string]$Question)
    $Answer = Read-Host "$Question [y/N]"
    return $Answer -match "^(y|yes)$"
}

function Test-CommandExists {
    param([string]$Name)
    return [bool](Get-Command $Name -ErrorAction SilentlyContinue)
}

function Find-VcpkgRoot {
    if ($env:VCPKG_ROOT -and (Test-Path (Join-Path $env:VCPKG_ROOT "vcpkg.exe"))) { return $env:VCPKG_ROOT }
    foreach ($Candidate in @("$env:USERPROFILE\scoop\apps\vcpkg\current", "$env:USERPROFILE\vcpkg", "C:\vcpkg")) {
        if (Test-Path (Join-Path $Candidate "vcpkg.exe")) { return $Candidate }
    }
    return $null
}

function Invoke-Step {
    param([string]$Title, [string]$LogName, [scriptblock]$Command)
    $LogPath = Join-Path $LogDir $LogName
    Write-Host "$Title..."
    $Start = Get-Date
    "[$Start] START: $Title`n" | Set-Content -LiteralPath $LogPath -Encoding UTF8
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
        Add-Content -LiteralPath $LogPath -Encoding UTF8 -Value "`n[$(Get-Date)] FAILED"
        Write-Host "Failed. Important log lines:"
        $Lines = Get-Content $LogPath | Select-String -Pattern "error:|CMake Error|failed|not found" | Select-Object -Last 20
        if ($Lines) { $Lines | ForEach-Object { Write-Host $_.Line } } else { Get-Content $LogPath -Tail 40 }
        Write-Host "Full log: $LogPath"
        throw "$Title failed"
    }
    $Elapsed = (Get-Date) - $Start
    Add-Content -LiteralPath $LogPath -Encoding UTF8 -Value "`n[$(Get-Date)] DONE after $($Elapsed.ToString('mm\:ss'))"
    Write-Host "Done in $($Elapsed.ToString('mm\:ss'))"
}

function Ensure-Vcpkg {
    $RootPath = Find-VcpkgRoot
    if ($RootPath) { return $RootPath }
    if (-not (Test-CommandExists "scoop")) { throw "vcpkg was not found. Install Scoop/vcpkg or set VCPKG_ROOT." }
    if (Ask-YesNo "vcpkg was not found. Install it with Scoop now?") {
        Invoke-Step "Installing vcpkg" "scoop-vcpkg.log" { scoop install vcpkg }
        $RootPath = Find-VcpkgRoot
    }
    if (-not $RootPath) { throw "vcpkg is unavailable." }
    return $RootPath
}

if (-not (Test-CommandExists "cmake")) { throw "cmake was not found. Install CMake first." }

$VcpkgRoot = Ensure-Vcpkg
$VcpkgExe = Join-Path $VcpkgRoot "vcpkg.exe"
$Toolchain = Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"
$env:VCPKG_DEFAULT_TRIPLET = $Triplet
$env:VCPKG_DEFAULT_HOST_TRIPLET = $Triplet

Write-Host "Installing lightweight FLTK dependencies."
Invoke-Step "Installing FLTK dependencies" "vcpkg-fltk.log" {
    & $VcpkgExe install "--x-manifest-root=$ManifestDir" "--triplet=$Triplet" "--host-triplet=$Triplet"
}

$Configure = @(
    "-S", "$Root\src\fltk",
    "-B", $Build,
    "-DCMAKE_BUILD_TYPE=Release",
    "-DCMAKE_TOOLCHAIN_FILE=$Toolchain",
    "-DVCPKG_MANIFEST_DIR=$ManifestDir",
    "-DVCPKG_TARGET_TRIPLET=$Triplet",
    "-DVCPKG_HOST_TRIPLET=$Triplet",
    "-DCMAKE_C_COMPILER=gcc",
    "-DCMAKE_CXX_COMPILER=g++"
)
if (Test-CommandExists "ninja") { $Configure += @("-G", "Ninja") }

Invoke-Step "Configuring FLTK project" "fltk-windows-configure.log" { cmake @Configure }
Invoke-Step "Building FLTK executables" "fltk-windows-build.log" { cmake --build $Build --config Release }

$Client = Get-ChildItem $Build -Recurse -Filter "sharenet_fltk_client.exe" | Select-Object -First 1
$Server = Get-ChildItem $Build -Recurse -Filter "sharenet_fltk_server.exe" | Select-Object -First 1
if (-not $Client -or -not $Server) { throw "FLTK build finished, but executables were not found." }
Copy-Item -Force $Client.FullName $Out
Copy-Item -Force $Server.FullName $Out

$BinDir = Join-Path $ManifestDir "vcpkg_installed\$Triplet\bin"
if (Test-Path $BinDir) { Copy-Item -Force (Join-Path $BinDir "*.dll") $Out -ErrorAction SilentlyContinue }
$GlobalBin = Join-Path $VcpkgRoot "installed\$Triplet\bin"
if (Test-Path $GlobalBin) { Copy-Item -Force (Join-Path $GlobalBin "*.dll") $Out -ErrorAction SilentlyContinue }

Write-Host "FLTK Windows executables written to $Out"
