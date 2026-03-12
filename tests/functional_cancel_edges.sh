#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/helpers/common.sh"
DAEMON="$1"
CLI="$2"
RUN_DIR="$3"
WS="$RUN_DIR/ws_functional_cancel_edges"
rm -rf "$WS"
mkdir -p "$WS/docs"
printf 'cancel edge\n' > "$WS/docs/file.txt"
setup_bridge_test "$DAEMON" "$CLI" "$RUN_DIR" "$WS"
start_bridge_daemon functional_cancel_edges

EMPTY_CANCEL_FAIL="$(invoke_bridge_cli_allow_fail cancel --workspace "$WS" --request-id req-empty-cancel --json)"
MISSING_TARGET_JSON="$(invoke_bridge_cli cancel --workspace "$WS" --target-request-id req-not-running --request-id req-cancel-missing --json)"
DUPLICATE_TARGET_JSON="$(invoke_bridge_cli cancel --workspace "$WS" --target-request-id req-not-running --request-id req-cancel-duplicate --json)"

assert_contains "$EMPTY_CANCEL_FAIL" 'INVALID_PARAMS'
assert_contains "$EMPTY_CANCEL_FAIL" 'target_request_id is empty'
assert_contains "$MISSING_TARGET_JSON" '"ok":true'
assert_contains "$MISSING_TARGET_JSON" '"target_request_id":"req-not-running"'
assert_contains "$DUPLICATE_TARGET_JSON" '"ok":true'
assert_contains "$DUPLICATE_TARGET_JSON" '"target_request_id":"req-not-running"'

stop_bridge_daemon
trap - EXIT
bridge_log 'functional_cancel_edges passed'
