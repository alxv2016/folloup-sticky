#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PROJECT_DIR=${PROJECT_DIR:-$(cd "${SCRIPT_DIR}/.." && pwd)}
BUILD_DIR=${BUILD_DIR:-"${PROJECT_DIR}/build"}
IDF_PY=${IDF_PY:-idf.py}
IDF_BAUD=${IDF_BAUD:-460800}

log() {
  printf '[esp-idf-scripts] %s\n' "$*"
}

print_shell_setup_hint() {
  log "Shell UX hint (zsh): add this to ~/.zshrc if missing:"
  printf '%s\n' '# ESP-IDF Environment (auto-load)'
  printf '%s\n' '# source ~/.esp_idf_env  # Uncomment to auto-load on terminal start'
  printf '%s\n' 'alias idf="source \"$HOME/.esp_idf_env\""'
  printf '%s\n' 'export PATH="$PATH:$HOME/go/bin"'
  printf '%s\n' 'export PATH="$HOME/.local/bin:$PATH"'
}

os_name() {
  uname -s 2>/dev/null || printf 'Unknown'
}

source_idf_env_if_needed() {
  if command -v "$IDF_PY" >/dev/null 2>&1; then
    return 0
  fi

  local candidates=()
  if [ -n "${IDF_PATH:-}" ]; then
    candidates+=("${IDF_PATH}/export.sh")
  fi
  candidates+=(
    "$HOME/esp/esp-idf/export.sh"
    "$HOME/.espressif/frameworks/esp-idf-v5.*/export.sh"
    "$HOME/.esp_idf_env"
  )

  local path
  for path in "${candidates[@]}"; do
    # shellcheck disable=SC2086
    for expanded in $path; do
      if [ -f "$expanded" ]; then
        # shellcheck source=/dev/null
        . "$expanded"
        if command -v "$IDF_PY" >/dev/null 2>&1; then
          log "Loaded ESP-IDF environment from $expanded"
          return 0
        fi
      fi
    done
  done

  log "ERROR: idf.py not found. Set IDF_PATH or source ESP-IDF export.sh first."
  print_shell_setup_hint
  return 1
}

verify_idf_installation() {
  if [ "${IDF_PREFLIGHT_DONE:-0}" = "1" ]; then
    return 0
  fi

  source_idf_env_if_needed

  if ! command -v "$IDF_PY" >/dev/null 2>&1; then
    log "ERROR: idf.py is still not available after environment load."
    print_shell_setup_hint
    return 1
  fi

  local idf_version
  if ! idf_version=$("$IDF_PY" --version 2>/dev/null); then
    log "ERROR: idf.py exists but failed to run. ESP-IDF installation/environment may be incomplete."
    log "Try: source ~/.esp_idf_env"
    print_shell_setup_hint
    return 1
  fi

  log "ESP-IDF ready: ${idf_version}"
  export IDF_PREFLIGHT_DONE=1
}

verify_plugin_compatibility_evidence() {
  if [ "${SKIP_PLUGIN_COMPAT_CHECK:-0}" = "1" ]; then
    log "Skipping plugin compatibility check (SKIP_PLUGIN_COMPAT_CHECK=1)."
    return 0
  fi

  local checker="${SCRIPT_DIR}/check_plugin_compatibility.py"
  if [ ! -f "$checker" ]; then
    log "Plugin compatibility checker not found at $checker (skipping)."
    return 0
  fi

  if ! command -v python3 >/dev/null 2>&1; then
    log "ERROR: python3 is required for plugin compatibility checks."
    return 1
  fi

  mkdir -p "$BUILD_DIR"
  log "Verifying plugin/framework compatibility evidence (ESP-IDF <-> ADF/SR/etc.)"
  if ! PROJECT_DIR="$PROJECT_DIR" BUILD_DIR="$BUILD_DIR" IDF_PY="$IDF_PY" python3 "$checker"; then
    log "ERROR: Plugin compatibility verification failed."
    log "See evidence report: ${BUILD_DIR}/plugin-compatibility-evidence.txt"
    log "Tip: Set ESP_REQUIRED_PLUGINS=esp-adf,esp-sr (or subset) to force/clarify checks."
    log "Tip: Provide cross-stack proof via ESP_STACK_COMPAT_EVIDENCE or a project compatibility lock file."
    return 1
  fi
}

read_sdkconfig_target() {
  local sdkconfig="${PROJECT_DIR}/sdkconfig"
  if [ -f "$sdkconfig" ]; then
    sed -n 's/^CONFIG_IDF_TARGET="\([^"]*\)"/\1/p' "$sdkconfig" | head -n1
  fi
}

ensure_target() {
  local requested="${ESP_TARGET:-}"
  local configured
  configured=$(read_sdkconfig_target || true)

  if [ -z "$requested" ] && [ -n "$configured" ]; then
    requested="$configured"
  fi

  if [ -z "$requested" ]; then
    log "ERROR: ESP target not known. Set ESP_TARGET (e.g. esp32s3) or create sdkconfig with CONFIG_IDF_TARGET."
    return 1
  fi

  if [ "${configured:-}" != "$requested" ]; then
    log "Setting target to ${requested} (sdkconfig target was '${configured:-unset}')"
    "$IDF_PY" -C "$PROJECT_DIR" -B "$BUILD_DIR" set-target "$requested"
  fi

  export ESP_TARGET="$requested"
}

detect_port() {
  if [ -n "${PORT:-}" ]; then
    if [ ! -e "$PORT" ]; then
      log "WARNING: PORT=${PORT} does not exist. Check device connection and path."
    fi
    printf '%s\n' "$PORT"
    return 0
  fi

  local os
  os=$(os_name)
  local globs=()

  case "$os" in
    Darwin)
      globs=(
        /dev/cu.usbmodem*
        /dev/cu.usbserial*
        /dev/cu.SLAB_USBtoUART*
        /dev/cu.wchusbserial*
      )
      ;;
    Linux)
      globs=(
        /dev/serial/by-id/*
        /dev/ttyACM*
        /dev/ttyUSB*
      )
      ;;
    *)
      globs=(/dev/ttyACM* /dev/ttyUSB* /dev/cu.usbmodem* /dev/cu.usbserial*)
      ;;
  esac

  local found=()
  local g expanded
  for g in "${globs[@]}"; do
    # shellcheck disable=SC2086
    for expanded in $g; do
      if [ -e "$expanded" ]; then
        found+=("$expanded")
      fi
    done
  done

  if [ "${#found[@]}" -eq 0 ]; then
    log "ERROR: No serial port detected. Connect the device and try again."
    log "       Set PORT=/dev/ttyUSB0 (Linux) or PORT=/dev/cu.usbmodemXXXX (macOS) to override."
    return 1
  fi

  if [ "${#found[@]}" -eq 1 ]; then
    log "Auto-detected port: ${found[0]}"
    printf '%s\n' "${found[0]}"
    return 0
  fi

  log "ERROR: Multiple serial ports detected. Set PORT=<device> to choose one:"
  local p
  for p in "${found[@]}"; do
    log "  $p"
  done
  log "Example: PORT=${found[0]} ./scripts/flash.sh"
  return 1
}

run_idf() {
  verify_idf_installation
  verify_plugin_compatibility_evidence
  ensure_target
  "$IDF_PY" -C "$PROJECT_DIR" -B "$BUILD_DIR" "$@"
}
