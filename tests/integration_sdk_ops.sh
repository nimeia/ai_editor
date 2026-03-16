#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
source "$ROOT_DIR/tests/helpers/common.sh"

DAEMON="$1"
CLI="$2"
RUN_DIR="$3"
WS="$RUN_DIR/ws_sdk"

rm -rf "$WS"
mkdir -p "$WS/docs" "$WS/cfg" "$WS/web"
printf '# Title\n' > "$WS/docs/guide.md"
printf '{\n  "name": "demo"\n}\n' > "$WS/cfg/settings.json"
printf 'app:\n  flags:\n    - base\n' > "$WS/cfg/settings.yaml"
printf '<html><body><div id="app"></div></body></html>' > "$WS/web/index.html"

setup_bridge_test "$DAEMON" "$CLI" "$RUN_DIR" "$WS"
start_bridge_daemon daemon_sdk

python3 "$ROOT_DIR/tests/sdk/python_smoke.py" "$CLI" "$WS"
node "$ROOT_DIR/tests/sdk/node_smoke.mjs" "$CLI" "$WS"
