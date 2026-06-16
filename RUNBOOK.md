# ShareNet Runbook

## Build

Windows:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build-windows.ps1
```

Linux:

```sh
./scripts/build-linux.sh
```

macOS:

```sh
./scripts/build-macos.sh
```

## Server

1. Run `sharenet_server`.
2. Choose the server files folder. Default: `server_files/`.
3. Set bind IP and port.
4. Click Start.
5. Use Refresh, Delete, or Copy To for hosted files.

Recommended bind values:

- local only: `127.0.0.1:5000`
- LAN: `0.0.0.0:5000`

## Client

1. Run `sharenet_client`.
2. Set server IP and port.
3. Click List.
4. Choose a local file and click Upload.
5. Select a server file and click Download.
6. Choose the downloads folder when needed. Default: `downloads/`.

## Clean

```powershell
Remove-Item -Recurse -Force dist,build -ErrorAction SilentlyContinue
```

```sh
rm -rf dist build
```

Runtime files under `client_files/`, `server_files/`, and `downloads/` are ignored by git.
