#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/qt-linux"
OUT_DIR="$ROOT_DIR/dist/qt-linux"
LOG_DIR="$ROOT_DIR/build/logs"
mkdir -p "$BUILD_DIR" "$OUT_DIR" "$LOG_DIR"
TOTAL_STEPS=3
STEP_INDEX=0

ask_yes_no() {
  local answer
  read -r -p "$1 [y/N]: " answer || answer=""
  case "$answer" in y|Y|yes|YES|Yes) return 0 ;; *) return 1 ;; esac
}

install_qt_linux() {
  if command -v apt-get >/dev/null 2>&1; then
    run_quiet "Updating apt package index" "apt-update.log" "Usually 1-5 minutes." sudo apt-get update
    run_quiet "Installing Qt dependencies with apt" "apt-qt.log" "Usually 5-20 minutes." sudo apt-get install -y cmake build-essential qt6-base-dev
  elif command -v dnf >/dev/null 2>&1; then
    run_quiet "Installing Qt dependencies with dnf" "dnf-qt.log" "Usually 5-20 minutes." sudo dnf install -y cmake gcc-c++ qt6-qtbase-devel
  elif command -v pacman >/dev/null 2>&1; then
    run_quiet "Installing Qt dependencies with pacman" "pacman-qt.log" "Usually 5-20 minutes." sudo pacman -Sy --needed --noconfirm cmake base-devel qt6-base
  else
    echo "Unsupported package manager. Install CMake and Qt6 manually."
    return 1
  fi
}

run_quiet() {
  local title="$1"
  local log_name="$2"
  local estimate="${3:-short}"
  shift 3
  local log_path="$LOG_DIR/$log_name"
  STEP_INDEX=$((STEP_INDEX + 1))
  local percent=$(( (STEP_INDEX - 1) * 100 / TOTAL_STEPS ))
  local start
  start="$(date +%s)"
  echo "[$STEP_INDEX/$TOTAL_STEPS] $title ($percent%)"
  echo "    Estimate: $estimate"
  {
    echo "[$(date)] START: $title"
    echo "Estimate: $estimate"
    echo
  } >"$log_path"
  if ! "$@" >>"$log_path" 2>&1; then
    local elapsed=$(( $(date +%s) - start ))
    echo "[$(date)] FAILED after ${elapsed}s" >>"$log_path"
    echo "Failed. Last log lines from $log_path:"
    tail -n 80 "$log_path" || true
    return 1
  fi
  local elapsed=$(( $(date +%s) - start ))
  echo "[$(date)] DONE after ${elapsed}s" >>"$log_path"
  local done_percent=$(( STEP_INDEX * 100 / TOTAL_STEPS ))
  echo "    Done in ${elapsed}s ($done_percent%)"
}

if ! command -v cmake >/dev/null 2>&1 || ! cmake -S "$ROOT_DIR/src/qt" -B "$BUILD_DIR/probe" >/dev/null 2>&1; then
  rm -rf "$BUILD_DIR/probe"
  echo "Qt/CMake dependencies were not detected."
  if ask_yes_no "Install Qt build dependencies now?"; then
    install_qt_linux
  fi
fi
rm -rf "$BUILD_DIR/probe"

run_quiet "Configuring Qt project" "qt-linux-configure.log" "Usually under 2 minutes after Qt is installed." cmake -S "$ROOT_DIR/src/qt" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
run_quiet "Building Qt executables" "qt-linux-build.log" "Usually under 5 minutes after Qt is installed." cmake --build "$BUILD_DIR" --config Release
cp "$BUILD_DIR/sharenet_qt_client" "$BUILD_DIR/sharenet_qt_server" "$OUT_DIR/"
echo "Qt Linux executables written to $OUT_DIR"
