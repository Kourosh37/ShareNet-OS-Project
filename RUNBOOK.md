# ShareNet Runbook

This runbook is the operational guide for the current ShareNet project. The maintained UI is Qt. The CLI tools are kept for protocol testing and simple demos.

## 1. Variants

- CLI server/client: terminal tools written in C.
- Qt server/client: modern GUI built with Qt Widgets and Qt Network.

## 2. Clean Workspace

```sh
make clean
```

Generated output is ignored by git:

- `build/`
- `dist/`
- root executables such as `sharenet_server`
- runtime files in `server_files/` and `downloads/`

Keep `client_files/sample.txt`, `test_files/sample.txt`, and `.gitkeep` files.

## 3. Build CLI

Linux or WSL:

```sh
make
```

Windows:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build-windows.ps1
```

Windows CLI outputs:

```text
dist\windows\sharenet_server.exe
dist\windows\sharenet_client.exe
```

## 4. Build Qt GUI

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

If Qt is missing, the scripts ask before attempting installation. On Windows,
the Qt script uses `vcpkg` and installs `qtbase:x64-mingw-dynamic`; it also
uses an existing Qt installation when `QT_DIR` or `CMAKE_PREFIX_PATH` is set.

Build scripts keep terminal output short. Verbose compiler, package manager,
CMake, and vcpkg output is written to:

```text
build\logs\
```

Each log includes the step start time, estimate, completion time, and elapsed
duration. The terminal shows numbered steps, stage percentages, and short
failure tails.

Qt outputs:

```text
sharenet_qt_server
sharenet_qt_client
```

On Windows they are placed under:

```text
dist\qt-windows\
```

## 5. Run Qt

Start server first:

```text
sharenet_qt_server
```

Recommended bind settings:

- local testing: `127.0.0.1:5000`
- LAN testing: `0.0.0.0:5000`

Then start:

```text
sharenet_qt_client
```

Client workflow:

1. Set server IP and port.
2. Pick an upload file with the native file dialog.
3. Upload and watch the progress bar.
4. Click List Files.
5. Select a server file row.
6. Choose a download folder.
7. Download and verify the success tick.

Server workflow:

1. Start listening.
2. Refresh the server file table.
3. Delete selected files when needed.
4. Copy/download selected server files to a local path when needed.

## 6. Run CLI Demo

Terminal 1:

```sh
./sharenet_server
```

Terminal 2:

```sh
./sharenet_client
```

Demo flow:

1. Upload `client_files/sample.txt`.
2. List server files.
3. Download `sample.txt`.
4. Verify the transfer.

## 7. Verify Transfer Integrity

Linux or WSL:

```sh
diff client_files/sample.txt downloads/sample.txt
sha256sum client_files/sample.txt downloads/sample.txt
```

Windows PowerShell:

```powershell
Compare-Object (Get-Content client_files\sample.txt) (Get-Content downloads\sample.txt)
Get-FileHash client_files\sample.txt
Get-FileHash downloads\sample.txt
```

No diff output and matching hashes mean the transfer is correct.

## 8. Build Portable Windows Package

```powershell
powershell -ExecutionPolicy Bypass -File scripts\package-windows-portable.ps1
```

Outputs:

```text
dist\ShareNet-Windows-Portable.zip
dist\ShareNet-Windows-Portable.exe
```

The package includes CLI and Qt outputs when Qt is available. It also includes runtime files needed on a target Windows computer.

## 9. Troubleshooting

- `Address already in use`: stop the existing server or use another port.
- Client cannot connect: verify IP, port, firewall rules, and that the server is listening.
- Qt build fails on Windows: rerun `scripts\build-qt-windows.ps1`, answer `y`
  when asked to install Qt, or set `QT_DIR`/`CMAKE_PREFIX_PATH` for an existing
  Qt installation.
- Portable app does not start: keep all generated DLLs next to their executables.
