#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/fltk-linux"
OUT_DIR="$ROOT_DIR/dist/fltk-linux"
LOG_DIR="$ROOT_DIR/build/logs"
mkdir -p "$BUILD_DIR" "$OUT_DIR" "$LOG_DIR"

ask_yes_no() {
  local answer
  read -r -p "$1 [y/N]: " answer || answer=""
  case "$answer" in y|Y|yes|YES|Yes) return 0 ;; *) return 1 ;; esac
}

run_step() {
  local title="$1"
  local log="$LOG_DIR/$2"
  shift 2
  echo "$title..."
  if ! "$@" >"$log" 2>&1; then
    echo "Failed. Last log lines from $log:"
    tail -n 60 "$log" || true
    return 1
  fi
  echo "Done."
}

install_fltk_linux() {
  if command -v apt-get >/dev/null 2>&1; then
    run_step "Updating apt package index" "apt-update.log" sudo apt-get update
    run_step "Installing FLTK dependencies with apt" "apt-fltk.log" sudo apt-get install -y cmake build-essential libfltk1.3-dev
  elif command -v dnf >/dev/null 2>&1; then
    run_step "Installing FLTK dependencies with dnf" "dnf-fltk.log" sudo dnf install -y cmake gcc-c++ fltk-devel
  elif command -v pacman >/dev/null 2>&1; then
    run_step "Installing FLTK dependencies with pacman" "pacman-fltk.log" sudo pacman -Sy --needed --noconfirm cmake base-devel fltk
  else
    echo "Unsupported package manager. Install CMake and FLTK manually."
    return 1
  fi
}

if ! command -v cmake >/dev/null 2>&1 || ! cmake -S "$ROOT_DIR/src/fltk" -B "$BUILD_DIR/probe" >/dev/null 2>&1; then
  rm -rf "$BUILD_DIR/probe"
  echo "FLTK/CMake dependencies were not detected."
  if ask_yes_no "Install FLTK build dependencies now?"; then
    install_fltk_linux
  fi
fi
rm -rf "$BUILD_DIR/probe"

run_step "Configuring FLTK project" "fltk-linux-configure.log" cmake -S "$ROOT_DIR/src/fltk" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
run_step "Building FLTK executables" "fltk-linux-build.log" cmake --build "$BUILD_DIR" --config Release
cp "$BUILD_DIR/sharenet_fltk_client" "$BUILD_DIR/sharenet_fltk_server" "$OUT_DIR/"
echo "FLTK Linux executables written to $OUT_DIR"
