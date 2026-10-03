#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SKILL_DIR="${ROOT}/.cursor/skills/easyinput-board-cy"
CHECKER="${SKILL_DIR}/scripts/check_board_baseline.py"
TARGET="${ROOT}/firmware"

if [[ ! -f "${CHECKER}" ]]; then
  echo "missing easyinput-board-cy skill at ${SKILL_DIR}" >&2
  echo "expected sibling checkout: Documents/GitHub/easyinput-board-cy" >&2
  exit 2
fi

exec python3 "${CHECKER}" "${TARGET}" "$@"
