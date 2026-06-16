#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/qt-macos"
OUT_DIR="$ROOT_DIR/dist/qt-macos"
LOG_DIR="$ROOT_DIR/build/logs"
mkdir -p "$BUILD_DIR" "$OUT_DIR" "$LOG_DIR"
TOTAL_STEPS=3
STEP_INDEX=0

ask_yes_no() {
  local answer
  read -r -p "$1 [y/N]: " answer || answer=""
  case "$answer" in y|Y|yes|YES|Yes) return 0 ;; *) return 1 ;; esac
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

if ! command -v brew >/dev/null 2>&1; then
  echo "Homebrew was not found."
  if ask_yes_no "Install Homebrew now?"; then
    run_quiet "Installing Homebrew" "homebrew-install.log" "Usually 5-20 minutes." /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
  fi
fi

if ! command -v cmake >/dev/null 2>&1 || ! brew list qt >/dev/null 2>&1; then
  if ask_yes_no "Install CMake and Qt with Homebrew now?"; then
    run_quiet "Installing CMake and Qt with Homebrew" "brew-qt.log" "Usually 10-60 minutes." brew install cmake qt
  fi
fi

QT_PREFIX="$(brew --prefix qt 2>/dev/null || true)"
run_quiet "Configuring Qt project" "qt-macos-configure.log" "Usually under 2 minutes after Qt is installed." cmake -S "$ROOT_DIR/src/qt" -B "$BUILD_DIR" -DCMAKE_PREFIX_PATH="$QT_PREFIX" -DCMAKE_BUILD_TYPE=Release
run_quiet "Building Qt executables" "qt-macos-build.log" "Usually under 5 minutes after Qt is installed." cmake --build "$BUILD_DIR" --config Release
cp "$BUILD_DIR/sharenet_qt_client" "$BUILD_DIR/sharenet_qt_server" "$OUT_DIR/"
echo "Qt macOS executables written to $OUT_DIR"
