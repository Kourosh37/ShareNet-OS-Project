#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/dist/linux"
mkdir -p "$OUT_DIR"

CC="${CC:-gcc}"
CFLAGS="-Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -I$ROOT_DIR/include"
COMMON="$ROOT_DIR/src/common/protocol.c $ROOT_DIR/src/common/socket_utils.c $ROOT_DIR/src/common/file_utils.c $ROOT_DIR/src/common/chunk.c"

if pkg-config --exists raylib; then
  RAYLIB_CFLAGS="$(pkg-config --cflags raylib)"
  RAYLIB_LIBS="$(pkg-config --libs raylib)"
else
  RAYLIB_CFLAGS=""
  RAYLIB_LIBS="-lraylib -lm -ldl -lpthread -lGL -lrt -lX11"
fi

"$CC" $CFLAGS -o "$OUT_DIR/sharenet_server" $COMMON "$ROOT_DIR/src/server/server_main.c" "$ROOT_DIR/src/server/server.c"
"$CC" $CFLAGS -o "$OUT_DIR/sharenet_client" $COMMON "$ROOT_DIR/src/client/client_main.c" "$ROOT_DIR/src/client/client.c"
"$CC" $CFLAGS $RAYLIB_CFLAGS -o "$OUT_DIR/sharenet_server_gui" $COMMON "$ROOT_DIR/src/server/server.c" "$ROOT_DIR/src/gui/server_gui.c" $RAYLIB_LIBS
"$CC" $CFLAGS $RAYLIB_CFLAGS -o "$OUT_DIR/sharenet_client_gui" $COMMON "$ROOT_DIR/src/gui/client_gui.c" $RAYLIB_LIBS

echo "Linux executables written to $OUT_DIR"
