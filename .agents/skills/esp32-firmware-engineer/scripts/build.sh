#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=/dev/null
. "${SCRIPT_DIR}/common.sh"

FAIL_ON_WARNINGS=0
if [ "${1:-}" = "--strict-warnings" ]; then
  FAIL_ON_WARNINGS=1
  shift
fi

LOG_FILE=${BUILD_LOG_FILE:-"${BUILD_DIR}/build.log"}
mkdir -p "$BUILD_DIR"

log "Project: $PROJECT_DIR"
log "Build dir: $BUILD_DIR"
run_idf build "$@" 2>&1 | tee "$LOG_FILE"

if [ "$FAIL_ON_WARNINGS" -eq 1 ] && grep -Eiq '(^|[^[:alpha:]])warning:' "$LOG_FILE"; then
  log "ERROR: Build completed with warnings and --strict-warnings was enabled."
  exit 2
fi

log "Build completed successfully."
