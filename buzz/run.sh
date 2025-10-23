#!/usr/bin/env bash
set -euo pipefail

# Determine the directory of this script and run the bundled binary
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXEC="$HERE/buzz"

if [[ ! -x "$EXEC" ]]; then
  echo "Error: '$EXEC' not found or not executable." >&2
  echo "Build the project first or run dist/package.sh to populate the binary." >&2
  exit 1
fi

# If launched from a file manager double-click, there may be no terminal.
# We try to detect if stdout is a TTY; if not, attempt to open a terminal emulator.
if [[ ! -t 1 ]]; then
  # Pick a terminal emulator available on the system
  for term in x-terminal-emulator gnome-terminal konsole xfce4-terminal xterm; do
    if command -v "$term" >/dev/null 2>&1; then
      "$term" -e "$EXEC"
      exit $?
    fi
  done
fi

"$EXEC" "$@"
