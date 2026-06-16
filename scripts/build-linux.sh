#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/dist/linux"
mkdir -p "$OUT_DIR"

if ! command -v go >/dev/null 2>&1; then
  echo "Go was not found. Install Go 1.22+ and rerun this script."
  exit 1
fi

cd "$ROOT_DIR"
export GOPROXY="https://proxy.golang.org,direct"
go mod download
go build -trimpath -ldflags "-s -w" -o "$OUT_DIR/sharenet_server" ./cmd/server
go build -trimpath -ldflags "-s -w" -o "$OUT_DIR/sharenet_client" ./cmd/client
echo "Linux executables written to $OUT_DIR"
