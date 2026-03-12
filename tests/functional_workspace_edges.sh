#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/helpers/common.sh"
DAEMON="$1"
CLI="$2"
RUN_DIR="$3"
WS="$RUN_DIR/ws_functional_workspace_edges"
rm -rf "$WS"
mkdir -p "$WS/docs" "$WS/node_modules/pkg"
printf 'edge\n' > "$WS/docs/file.txt"
printf 'skip\n' > "$WS/node_modules/pkg/file.js"
setup_bridge_test "$DAEMON" "$CLI" "$RUN_DIR" "$WS"
start_bridge_daemon functional_workspace_edges

RESOLVE_ROOT_JSON="$(invoke_bridge_cli resolve --workspace "$WS" --path . --json)"
RESOLVE_DENY_JSON="$(invoke_bridge_cli resolve --workspace "$WS" --path .bridge/data --json)"
RESOLVE_SKIP_JSON="$(invoke_bridge_cli resolve --workspace "$WS" --path node_modules/pkg/file.js --json)"
RESOLVE_OUTSIDE_FAIL="$(invoke_bridge_cli_allow_fail resolve --workspace "$WS" --path ../outside.txt --json)"
LIST_LIMIT_JSON="$(invoke_bridge_cli list --workspace "$WS" --path docs --max-results 1 --json)"
LIST_FILE_FAIL="$(invoke_bridge_cli_allow_fail list --workspace "$WS" --path docs/file.txt --json)"

assert_contains "$RESOLVE_ROOT_JSON" '"ok":true'
assert_contains "$RESOLVE_ROOT_JSON" '"relative_path":""'
assert_contains "$RESOLVE_DENY_JSON" '"policy":"deny"'
assert_contains "$RESOLVE_SKIP_JSON" '"policy":"skip_by_default"'
assert_contains "$RESOLVE_OUTSIDE_FAIL" 'PATH_OUTSIDE_WORKSPACE'
assert_contains "$LIST_LIMIT_JSON" '"ok":true'
assert_contains "$LIST_LIMIT_JSON" '"truncated":true'
assert_contains "$LIST_FILE_FAIL" 'INVALID_PARAMS'
assert_contains "$LIST_FILE_FAIL" 'path is not a directory'

stop_bridge_daemon
trap - EXIT
bridge_log 'functional_workspace_edges passed'
