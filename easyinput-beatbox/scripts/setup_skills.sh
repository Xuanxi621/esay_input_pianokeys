#!/usr/bin/env bash
# Re-link project + personal Cursor skills for this machine.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BOARD_SRC="$(cd "${ROOT}/../easyinput-board-cy" && pwd)"
PERSONAL_SKILLS="${HOME}/.cursor/skills"

mkdir -p "${ROOT}/.cursor/skills" "${PERSONAL_SKILLS}"

ln -sfn ../../../easyinput-board-cy "${ROOT}/.cursor/skills/easyinput-board-cy"
ln -sfn "${BOARD_SRC}" "${PERSONAL_SKILLS}/easyinput-board-cy"
ln -sfn "${ROOT}/.cursor/skills/easyinput-drum-machine" "${PERSONAL_SKILLS}/easyinput-drum-machine"

echo "project board skill -> $(readlink "${ROOT}/.cursor/skills/easyinput-board-cy")"
echo "personal board skill -> $(readlink "${PERSONAL_SKILLS}/easyinput-board-cy")"
echo "personal drum skill  -> $(readlink "${PERSONAL_SKILLS}/easyinput-drum-machine")"

test -f "${ROOT}/.cursor/skills/easyinput-board-cy/SKILL.md"
test -f "${ROOT}/.cursor/skills/easyinput-drum-machine/SKILL.md"
echo "skills OK"
