#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=/dev/null
. "${SCRIPT_DIR}/common.sh"

log "Opening menuconfig (fallback/discovery tool). Persist final changes in sdkconfig or sdkconfig.defaults."
run_idf menuconfig "$@"
