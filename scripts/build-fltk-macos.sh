#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/fltk-macos"
OUT_DIR="$ROOT_DIR/dist/fltk-macos"
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

if ! command -v brew >/dev/null 2>&1; then
  echo "Homebrew was not found."
  if ask_yes_no "Install Homebrew now?"; then
    run_step "Installing Homebrew" "homebrew-install.log" /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
  fi
fi

if ! command -v cmake >/dev/null 2>&1 || ! brew list fltk >/dev/null 2>&1; then
  if ask_yes_no "Install CMake and FLTK with Homebrew now?"; then
    run_step "Installing CMake and FLTK" "brew-fltk.log" brew install cmake fltk
  fi
fi

FLTK_PREFIX="$(brew --prefix fltk 2>/dev/null || true)"
run_step "Configuring FLTK project" "fltk-macos-configure.log" cmake -S "$ROOT_DIR/src/fltk" -B "$BUILD_DIR" -DCMAKE_PREFIX_PATH="$FLTK_PREFIX" -DCMAKE_BUILD_TYPE=Release
run_step "Building FLTK executables" "fltk-macos-build.log" cmake --build "$BUILD_DIR" --config Release
cp "$BUILD_DIR/sharenet_fltk_client" "$BUILD_DIR/sharenet_fltk_server" "$OUT_DIR/"
echo "FLTK macOS executables written to $OUT_DIR"
