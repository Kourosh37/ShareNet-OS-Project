# ShareNet Runbook

This runbook is the operational guide for the current ShareNet project. The maintained UI is FLTK. The CLI tools are kept for protocol testing and simple demos.

## 1. Variants

- CLI server/client: terminal tools written in C.
- FLTK server/client: lightweight cross-platform GUI.

## 2. Clean Workspace

```sh
make clean
```

Generated output is ignored by git:

- `build/`
- `dist/`
- root executables such as `sharenet_server`
- runtime files in `server_files/` and `downloads/`

Keep the `.gitkeep` files. Runtime files can be deleted when you want a clean local state.

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

## 4. Build FLTK GUI

Windows:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build-fltk-windows.ps1
```

Linux:

```sh
./scripts/build-fltk-linux.sh
```

macOS:

```sh
./scripts/build-fltk-macos.sh
```

If FLTK is missing, the scripts ask before attempting installation. On Windows,
the FLTK script uses `vcpkg` and installs `fltk:x64-mingw-dynamic`.

Build scripts keep terminal output short. Verbose compiler, package manager,
CMake, and vcpkg output is written to:

```text
build\logs\
```

Each log includes the step start time, estimate, completion time, and elapsed
duration. The terminal shows numbered steps, stage percentages, and short
failure tails.

FLTK outputs:

```text
sharenet_fltk_server
sharenet_fltk_client
```

On Windows they are placed under:

```text
dist\fltk-windows\
```

## 5. Run FLTK

Start server first:

```text
sharenet_fltk_server
```

Recommended bind settings:

- local testing: `127.0.0.1:5000`
- LAN testing: `0.0.0.0:5000`

Then start:

```text
sharenet_fltk_client
```

Client workflow:

1. Set server IP and port.
2. Pick an upload file with the native file dialog.
3. Upload and watch the progress bar.
4. Click List Files.
5. Select a server file row.
6. Choose a download folder.
7. Download and verify progress reaches 100%.

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

1. Put any file under `client_files/`.
2. Upload that file.
3. List server files.
4. Download the same file.
5. Verify the transfer.

## 7. Verify Transfer Integrity

Linux or WSL:

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

The package includes CLI and FLTK outputs when FLTK is available. It also includes runtime files needed on a target Windows computer.

## 9. Troubleshooting

- `Address already in use`: stop the existing server or use another port.
- Client cannot connect: verify IP, port, firewall rules, and that the server is listening.
- FLTK build fails on Windows: rerun `scripts\build-fltk-windows.ps1`, answer `y`
  if asked to install missing dependencies, and inspect `build\logs\`.
- Portable app does not start: keep all generated DLLs next to their executables.
