# ShareNet Documentation

## Architecture

- Language: Go
- GUI: Fyne
- Transport: TCP
- Request format: one JSON line
- File payload: raw bytes after the JSON header for upload/download

## Commands

- `list`: returns hosted files
- `upload`: sends a filename, size, and raw bytes
- `download`: receives a size and raw bytes
- `delete`: removes a hosted file

## Source Layout

- `cmd/server`: GUI server app
- `cmd/client`: GUI client app
- `internal/netshare`: protocol, client helpers, and server implementation
- `scripts`: build scripts
