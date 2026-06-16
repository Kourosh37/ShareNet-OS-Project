#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/qt-macos"
OUT_DIR="$ROOT_DIR/dist/qt-macos"
mkdir -p "$BUILD_DIR" "$OUT_DIR"

ask_yes_no() {
  local answer
  read -r -p "$1 [y/N]: " answer || answer=""
  case "$answer" in y|Y|yes|YES|Yes) return 0 ;; *) return 1 ;; esac
}

if ! command -v brew >/dev/null 2>&1; then
  echo "Homebrew was not found."
  if ask_yes_no "Install Homebrew now?"; then
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
  fi
fi

if ! command -v cmake >/dev/null 2>&1 || ! brew list qt >/dev/null 2>&1; then
  if ask_yes_no "Install CMake and Qt with Homebrew now?"; then
    brew install cmake qt
  fi
fi

QT_PREFIX="$(brew --prefix qt 2>/dev/null || true)"
cmake -S "$ROOT_DIR/src/qt" -B "$BUILD_DIR" -DCMAKE_PREFIX_PATH="$QT_PREFIX" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --config Release
cp "$BUILD_DIR/sharenet_qt_client" "$BUILD_DIR/sharenet_qt_server" "$OUT_DIR/"
echo "Qt macOS executables written to $OUT_DIR"
