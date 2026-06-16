# ShareNet Documentation

ShareNet is a client-server file sharing project implemented with C sockets and
a compact binary protocol. The maintained graphical interface is FLTK, keeping
build time and runtime dependencies small.

## Executables

- `sharenet_server`: CLI server
- `sharenet_client`: CLI client
- `sharenet_fltk_server`: FLTK server GUI
- `sharenet_fltk_client`: FLTK client GUI

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

The FLTK server lets the user configure bind IP and port, start or stop the TCP
server, refresh files in `server_files`, delete a selected file, and copy a
selected server file to another path.

The FLTK client lets the user configure server IP and port, choose any upload
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

FLTK GUI:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build-fltk-windows.ps1
```

```sh
./scripts/build-fltk-linux.sh
./scripts/build-fltk-macos.sh
```

Verbose build logs are written to `build/logs`.
