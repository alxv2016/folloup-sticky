#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_DIR="$ROOT_DIR/.venv"
PYTHON_BIN="${PYTHON:-python3}"
VALIDATOR_SCRIPT="/Users/adamlipecz/.codex/skills/.system/skill-creator/scripts/quick_validate.py"
TARGET_DIR="${1:-$ROOT_DIR}"

if [[ ! -f "$VALIDATOR_SCRIPT" ]]; then
  echo "Validator not found: $VALIDATOR_SCRIPT" >&2
  exit 1
fi

if [[ ! -x "$VENV_DIR/bin/python" ]]; then
  echo "Creating virtual environment at $VENV_DIR"
  "$PYTHON_BIN" -m venv "$VENV_DIR"
fi

if ! "$VENV_DIR/bin/python" -c "import yaml" >/dev/null 2>&1; then
  echo "Installing PyYAML into $VENV_DIR"
  "$VENV_DIR/bin/python" -m pip install --disable-pip-version-check pyyaml
fi

echo "Running validator on: $TARGET_DIR"
"$VENV_DIR/bin/python" "$VALIDATOR_SCRIPT" "$TARGET_DIR"
exit $?
