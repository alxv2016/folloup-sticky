#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=/dev/null
. "${SCRIPT_DIR}/common.sh"

PORT=$(detect_port)
export PORT

log "Opening monitor on ${PORT}"
run_idf -p "$PORT" monitor "$@"
