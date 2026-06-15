$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$Out = Join-Path $Root "dist\windows"
New-Item -ItemType Directory -Force -Path $Out | Out-Null

$CC = if ($env:CC) { $env:CC } else { "gcc" }
$CFlags = @("-Wall", "-Wextra", "-std=c11", "-I$Root\include")
$Common = @(
    "$Root\src\common\protocol.c",
    "$Root\src\common\socket_utils.c",
    "$Root\src\common\file_utils.c",
    "$Root\src\common\chunk.c"
)

function Invoke-Compile {
    & $CC @args
    if ($LASTEXITCODE -ne 0) {
        throw "Compilation failed: $CC $args"
    }
}

Invoke-Compile @CFlags -o "$Out\sharenet_server.exe" @Common "$Root\src\server\server_main.c" "$Root\src\server\server.c" -lws2_32
Invoke-Compile @CFlags -o "$Out\sharenet_client.exe" @Common "$Root\src\client\client_main.c" "$Root\src\client\client.c" -lws2_32

$RaylibPath = $env:RAYLIB_PATH
if (-not $RaylibPath) {
    Write-Host "RAYLIB_PATH is not set. Skipping Windows GUI builds."
    Write-Host "Set RAYLIB_PATH to a raylib install folder containing include\ and lib\."
    exit 0
}

Invoke-Compile @CFlags "-I$RaylibPath\include" -o "$Out\sharenet_server_gui.exe" @Common "$Root\src\server\server.c" "$Root\src\gui\server_gui.c" "-L$RaylibPath\lib" -lraylib -lopengl32 -lgdi32 -lwinmm -lws2_32
Invoke-Compile @CFlags "-I$RaylibPath\include" -o "$Out\sharenet_client_gui.exe" @Common "$Root\src\gui\client_gui.c" "-L$RaylibPath\lib" -lraylib -lopengl32 -lgdi32 -lwinmm -lws2_32

Write-Host "Windows executables written to $Out"
