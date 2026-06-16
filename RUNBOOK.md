# ShareNet Runbook

Operational guide for the current ShareNet project. The maintained GUI is Qt Widgets. The CLI tools remain available for protocol checks and simple terminal demos.

## 1. Project Layout

- `src/server/`, `src/client/`: CLI server/client implementation.
- `src/common/`: shared protocol, socket, file, and chunk helpers.
- `src/qt/`: Qt GUI server/client.
- `scripts/`: platform build and packaging scripts.
- `server_files/`: files hosted by the server at runtime.
- `client_files/`: optional local files to upload from the client.
- `downloads/`: downloaded files.

Only `.gitkeep` files are tracked inside runtime folders. Real uploaded/downloaded files are ignored by git.

## 2. Clean Workspace

Windows PowerShell:

```powershell
Remove-Item -Recurse -Force build,dist -ErrorAction SilentlyContinue
Remove-Item -Force sharenet_server,sharenet_client,*.exe,*.o,*.out -ErrorAction SilentlyContinue
```

Linux, macOS, or WSL:

```sh
make clean
rm -rf build dist
```

Generated output is intentionally ignored:

- `build/`
- `dist/`
- root executables such as `sharenet_server`
- runtime files under `client_files/`, `server_files/`, and `downloads/`
- local Qt/vcpkg install folders

## 3. Build CLI

Linux, macOS, or WSL:

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

If Qt or build dependencies are missing, the scripts ask before installing anything. On Windows, missing tools are installed through Scoop. If Scoop is missing, the scripts ask first and then install it for the current user with:

```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
Invoke-RestMethod -Uri https://get.scoop.sh | Invoke-Expression
```

Windows uses vcpkg and builds `qtbase:x64-mingw-dynamic` when a usable Qt installation is not already available.

Build output:

```text
dist\qt-windows\sharenet_qt_server.exe
dist\qt-windows\sharenet_qt_client.exe
build/qt-linux/sharenet_qt_server
build/qt-linux/sharenet_qt_client
build/qt-macos/sharenet_qt_server
build/qt-macos/sharenet_qt_client
```

Verbose compiler/package logs are written to:

```text
build\logs\
```

## 5. Run Qt

Start the server first:

```text
sharenet_qt_server
```

Recommended bind settings:

- local machine only: `127.0.0.1:5000`
- LAN testing: `0.0.0.0:5000`

Then start the client:

```text
sharenet_qt_client
```

Client workflow:

1. Set server IP and port.
2. Pick any local file with Browse.
3. Upload and wait for the progress bar to finish.
4. Click List.
5. Select a server file.
6. Choose a download folder.
7. Download and verify progress reaches 100%.

Server workflow:

1. Set bind IP and port.
2. Start listening.
3. Refresh the file list when needed.
4. Delete selected hosted files when needed.
5. Copy/download a selected hosted file to a local path when needed.

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

1. Put any file under `client_files/`.
2. Upload that file from the CLI client.
3. List server files.
4. Download the same file.
5. Verify the transfer.

## 7. Verify Transfer Integrity

Linux, macOS, or WSL:

```sh
diff client_files/YOUR_FILE downloads/YOUR_FILE
sha256sum client_files/YOUR_FILE downloads/YOUR_FILE
```

Windows PowerShell:

```powershell
Compare-Object (Get-Content client_files\YOUR_FILE) (Get-Content downloads\YOUR_FILE)
Get-FileHash client_files\YOUR_FILE
Get-FileHash downloads\YOUR_FILE
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

The portable package includes CLI executables, Qt executables, runtime DLLs, launcher script, and empty runtime folders. It should run on a Windows machine without requiring Qt or a compiler to be installed.

## 9. Troubleshooting

- `Address already in use`: stop the existing server or choose another port.
- Client cannot connect: verify IP, port, firewall rules, and server state.
- Windows Qt build takes a long time: first vcpkg Qt builds can take a long time; cached reruns are faster.
- Windows Qt build fails: rerun `scripts\build-qt-windows.ps1` and inspect `build\logs\`.
- Portable app does not start: keep generated DLLs next to their executables.
- Files do not appear: confirm the server is using the expected `server_files/` folder and click Refresh/List.
