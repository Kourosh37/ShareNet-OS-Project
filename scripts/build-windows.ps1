$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$Out = Join-Path $Root "dist\windows"
$LogDir = Join-Path $Root "build\logs"
New-Item -ItemType Directory -Force -Path $Out, $LogDir | Out-Null

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

function Install-WindowsCompiler {
    if (Test-CommandExists "scoop") {
        Invoke-Quiet "Installing GCC with Scoop" "scoop-gcc.log" { scoop install gcc }
        return
    }
    if (Test-CommandExists "winget") {
        Invoke-Quiet "Installing GCC with winget" "winget-gcc.log" {
            winget install --id BrechtSanders.WinLibs.POSIX.UCRT -e --accept-package-agreements --accept-source-agreements
        }
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
