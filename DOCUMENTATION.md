# ShareNet Documentation

ShareNet is a client-server file sharing project implemented with C sockets and
a compact binary protocol. The maintained graphical interface is Qt Widgets.

## Executables

- `sharenet_server`: CLI server
- `sharenet_client`: CLI client
- `sharenet_qt_server`: Qt server GUI
- `sharenet_qt_client`: Qt client GUI

## Protocol

Every message starts with:

```c
typedef struct {
    uint32_t type;
    uint32_t payload_size;
} MessageHeader;
```

File data is transferred in chunks:

```c
typedef struct {
    uint32_t chunk_index;
    uint32_t total_chunks;
    uint32_t chunk_size;
} ChunkHeader;
```

All integer fields are sent in network byte order.

## GUI Behavior

The Qt server lets the user configure bind IP and port, start or stop the TCP
server, refresh files in `server_files`, delete a selected file, and copy a
selected server file to another path.

The Qt client lets the user configure server IP and port, choose any upload
file, choose a download folder, list server files, upload files with progress,
and download the selected server file with progress.

## Builds

CLI:

```sh
make
```

Windows CLI:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build-windows.ps1
```

Qt GUI:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build-qt-windows.ps1
```

```sh
./scripts/build-qt-linux.sh
./scripts/build-qt-macos.sh
```

Verbose build logs are written to `build/logs`.
