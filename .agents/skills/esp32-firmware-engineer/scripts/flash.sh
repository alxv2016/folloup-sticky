#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=/dev/null
. "${SCRIPT_DIR}/common.sh"

BUILD_ARGS=()
FLASH_ARGS=()
for arg in "$@"; do
  if [ "$arg" = "--strict-warnings" ]; then
    BUILD_ARGS+=("$arg")
  else
    FLASH_ARGS+=("$arg")
  fi
done

if [ "${SKIP_BUILD:-0}" != "1" ]; then
  "${SCRIPT_DIR}/build.sh" "${BUILD_ARGS[@]}"
fi

PORT=$(detect_port)
export PORT
log "Flashing ${ESP_TARGET:-$(read_sdkconfig_target || true)} on port ${PORT} at baud ${IDF_BAUD}"
run_idf -p "$PORT" -b "$IDF_BAUD" flash "${FLASH_ARGS[@]}"
log "Flash completed successfully."
