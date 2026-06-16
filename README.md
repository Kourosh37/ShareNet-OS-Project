# ShareNet

ShareNet is a small Go + Fyne file sharing app with two GUI programs:

- `sharenet_server`: hosts files from `server_files/`
- `sharenet_client`: lists, uploads, downloads, and deletes files through the server

The protocol is simple TCP with JSON request headers and raw file bytes for transfers.

## Requirements

- Go 1.22+
- A C compiler for Fyne desktop builds

On Windows, `scripts\build-windows.ps1` can install missing tools with Scoop after asking for confirmation.

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

Outputs are written under `dist/`.

Runtime folders such as `server_files/`, `client_files/`, and `downloads/` are
created automatically when the apps run and are not tracked by git.

## Run

Start the server first, then the client.

Default address:

```text
127.0.0.1:5000
```

Use `0.0.0.0:5000` on the server if clients on your LAN need to connect.

See [RUNBOOK.md](RUNBOOK.md) for the short operating guide.
