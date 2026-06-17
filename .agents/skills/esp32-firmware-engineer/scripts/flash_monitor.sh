#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=/dev/null
. "${SCRIPT_DIR}/common.sh"

"${SCRIPT_DIR}/flash.sh" "$@"
"${SCRIPT_DIR}/monitor.sh"
