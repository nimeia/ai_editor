#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
source "$ROOT_DIR/tests/helpers/common.sh"

DAEMON="$1"
CLI="$2"
RUN_DIR="$3"
WS="$RUN_DIR/ws_structure"

rm -rf "$WS"
mkdir -p "$WS/docs" "$WS/cfg" "$WS/web"
printf '# Title\n\n## Intro\nold intro\n' > "$WS/docs/guide.md"
printf '{\n  "name": "demo",\n  "items": [1]\n}\n' > "$WS/cfg/settings.json"
printf 'app:\n  name: demo\n  flags:\n    - alpha\n' > "$WS/cfg/settings.yaml"
printf '<html><body><div id="app"><p>Hello</p></div></body></html>' > "$WS/web/index.html"

setup_bridge_test "$DAEMON" "$CLI" "$RUN_DIR" "$WS"
start_bridge_daemon daemon_structure

invoke_bridge_cli session-begin --workspace "$WS" --session-id sess-struct --json >/dev/null

MD_FILE="$RUN_DIR/md.txt"
printf 'new intro\n' > "$MD_FILE"
MD_JSON="$(invoke_bridge_cli markdown-replace-section --workspace "$WS" --session-id sess-struct --path docs/guide.md --heading Intro --heading-level 2 --content-file "$MD_FILE" --json)"
assert_json_eq "$MD_JSON" 'result.path' 'docs/guide.md'

JSON_FILE="$RUN_DIR/json_value.txt"
printf '2' > "$JSON_FILE"
JSON_RES="$(invoke_bridge_cli json-append-array-item --workspace "$WS" --session-id sess-struct --path cfg/settings.json --key-path items --content-file "$JSON_FILE" --json)"
assert_json_eq "$JSON_RES" 'result.path' 'cfg/settings.json'

YAML_FILE="$RUN_DIR/yaml_value.txt"
printf 'beta' > "$YAML_FILE"
YAML_RES="$(invoke_bridge_cli yaml-append-item --workspace "$WS" --session-id sess-struct --path cfg/settings.yaml --key-path app.flags --content-file "$YAML_FILE" --json)"
assert_json_eq "$YAML_RES" 'result.path' 'cfg/settings.yaml'

HTML_RES="$(invoke_bridge_cli html-set-attribute --workspace "$WS" --session-id sess-struct --path web/index.html --query 'div id="app"' --attr-name data-mode --attr-value active --json)"
assert_json_eq "$HTML_RES" 'result.path' 'web/index.html'

PREVIEW_JSON="$(invoke_bridge_cli session-preview --workspace "$WS" --session-id sess-struct --json)"
assert_json_eq "$PREVIEW_JSON" 'result.previewed_file_count' '4'
assert_json_contains "$PREVIEW_JSON" 'result.summary' 'risk'

COMMIT_JSON="$(invoke_bridge_cli session-commit --workspace "$WS" --session-id sess-struct --json)"
assert_json_eq "$COMMIT_JSON" 'result.committed_file_count' '4'

assert_contains "$(cat "$WS/docs/guide.md")" 'new intro'
assert_contains "$(cat "$WS/cfg/settings.json")" '2'
assert_contains "$(cat "$WS/cfg/settings.yaml")" '- beta'
assert_contains "$(cat "$WS/web/index.html")" 'data-mode="active"'
