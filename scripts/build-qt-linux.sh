#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/qt-linux"
OUT_DIR="$ROOT_DIR/dist/qt-linux"
mkdir -p "$BUILD_DIR" "$OUT_DIR"

ask_yes_no() {
  local answer
  read -r -p "$1 [y/N]: " answer || answer=""
  case "$answer" in y|Y|yes|YES|Yes) return 0 ;; *) return 1 ;; esac
}

install_qt_linux() {
  if command -v apt-get >/dev/null 2>&1; then
    sudo apt-get update
    sudo apt-get install -y cmake build-essential qt6-base-dev
  elif command -v dnf >/dev/null 2>&1; then
    sudo dnf install -y cmake gcc-c++ qt6-qtbase-devel
  elif command -v pacman >/dev/null 2>&1; then
    sudo pacman -Sy --needed --noconfirm cmake base-devel qt6-base
  else
    echo "Unsupported package manager. Install CMake and Qt6 manually."
    return 1
  fi
}

if ! command -v cmake >/dev/null 2>&1 || ! cmake -S "$ROOT_DIR/src/qt" -B "$BUILD_DIR/probe" >/dev/null 2>&1; then
  rm -rf "$BUILD_DIR/probe"
  echo "Qt/CMake dependencies were not detected."
  if ask_yes_no "Install Qt build dependencies now?"; then
    install_qt_linux
  fi
fi
rm -rf "$BUILD_DIR/probe"

cmake -S "$ROOT_DIR/src/qt" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --config Release
cp "$BUILD_DIR/sharenet_qt_client" "$BUILD_DIR/sharenet_qt_server" "$OUT_DIR/"
echo "Qt Linux executables written to $OUT_DIR"
