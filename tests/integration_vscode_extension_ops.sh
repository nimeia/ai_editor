#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
source "$ROOT_DIR/tests/helpers/common.sh"
DAEMON="$1"
CLI="$2"
RUN_DIR="$3"
WS="$RUN_DIR/ws_vscode"
rm -rf "$WS"
mkdir -p "$WS/docs"
printf 'one
two
three
' > "$WS/docs/note.txt"
setup_bridge_test "$DAEMON" "$CLI" "$RUN_DIR" "$WS"
start_bridge_daemon daemon_vscode
node "$ROOT_DIR/tests/sdk/vscode_extension_smoke.mjs" "$CLI" "$WS"
