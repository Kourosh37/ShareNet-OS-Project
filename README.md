# ShareNet-Phase1-C

ShareNet is a C-based TCP file sharing project with a binary protocol, chunked transfers, a single-client server model, CLI tools, and a Qt GUI.

## Features

- TCP client/server file sharing
- Binary protocol with `MessageHeader` and `ChunkHeader`
- Chunked upload and download
- Safe filename validation against path traversal
- CLI server and client
- Qt server and client
- Windows, Linux, and macOS build scripts
- Windows portable package script

## Requirements

CLI:

- C compiler
- make on Linux/macOS or WSL

Qt GUI:

- CMake
- Qt 6 preferred, Qt 5 fallback supported by CMake
- C++17 compiler
- Windows Qt auto-install uses `vcpkg` with `qtbase:x64-mingw-dynamic`

The build scripts ask before installing missing dependencies.

## Build CLI

Linux/WSL:

```sh
make
```

Windows:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build-windows.ps1
```

## Build Qt GUI

Windows:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build-qt-windows.ps1
```

Linux:

```sh
./scripts/build-qt-linux.sh
```

macOS:

```sh
./scripts/build-qt-macos.sh
```

## Qt Server

The Qt server supports:

- configurable bind IP and port
- file table for `server_files`
- delete selected file
- copy/download selected server file to a local path
- protocol-compatible TCP service

## Qt Client

The Qt client supports:

- configurable server IP and port
- native file picker for uploads
- server file list
- selected-row download
- selectable download folder
- progress bar for upload/download
- animated success tick
- async networking through `QTcpSocket`

## CLI Demo

Terminal 1:

```sh
./sharenet_server
```

Terminal 2:

```sh
./sharenet_client
```

Demo:

1. Upload `client_files/sample.txt`.
2. List files.
3. Download `sample.txt`.
4. Verify integrity.

## Verify Integrity

```sh
diff client_files/sample.txt downloads/sample.txt
sha256sum client_files/sample.txt downloads/sample.txt
```

No `diff` output and matching hashes mean the transfer is correct.

## Windows Portable Package

```powershell
powershell -ExecutionPolicy Bypass -File scripts\package-windows-portable.ps1
```

Outputs:

```text
dist\ShareNet-Windows-Portable.zip
dist\ShareNet-Windows-Portable.exe
```

See [RUNBOOK.md](RUNBOOK.md) for the full operating guide.
