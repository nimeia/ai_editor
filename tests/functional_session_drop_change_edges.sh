#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
source "$ROOT_DIR/tests/helpers/common.sh"

split_allow_fail() {
  local payload="$1"
  local __code_var="$2"
  local __text_var="$3"
  local code
  code="$(printf '%s
' "$payload" | sed -n '1p')"
  local text
  text="$(printf '%s
' "$payload" | sed '1d')"
  printf -v "$__code_var" '%s' "$code"
  printf -v "$__text_var" '%s' "$text"
}

DAEMON="$1"
CLI="$2"
RUN_DIR="$3"
WS="$RUN_DIR/ws_session_drop_edges"

rm -rf "$WS"
mkdir -p "$WS/docs"
printf 'one
two
three
' > "$WS/docs/note.txt"

setup_bridge_test "$DAEMON" "$CLI" "$RUN_DIR" "$WS"
start_bridge_daemon daemon_session_drop_edges

BEGIN_JSON="$(invoke_bridge_cli session-begin --workspace "$WS" --session-id sess-drop-edges --json)"
assert_json_eq "$BEGIN_JSON" 'result.state' 'created'

FIRST_FILE="$RUN_DIR/session_edge.txt"
printf 'ONE' > "$FIRST_FILE"
ADD_JSON="$(invoke_bridge_cli edit-replace-range --workspace "$WS" --session-id sess-drop-edges --path docs/note.txt --start 1 --end 1 --content-file "$FIRST_FILE" --json)"
assert_json_eq "$ADD_JSON" 'result.staged_change_count' '1'
FIRST_CHANGE_ID="$(json_get "$ADD_JSON" 'result.change_id')"

EMPTY_DROP_PAYLOAD="$(invoke_bridge_cli_allow_fail session-drop-change --workspace "$WS" --session-id sess-drop-edges --change-id '' --json)"
split_allow_fail "$EMPTY_DROP_PAYLOAD" EMPTY_DROP_CODE EMPTY_DROP_TEXT
[[ "$EMPTY_DROP_CODE" == "1" ]] || bridge_fail "expected empty change_id drop to exit 1"
assert_contains "$EMPTY_DROP_TEXT" '"code":"INVALID_PARAMS"'
assert_contains "$EMPTY_DROP_TEXT" '"message":"change_id required"'

INSPECT_JSON="$(invoke_bridge_cli session-inspect --workspace "$WS" --session-id sess-drop-edges --json)"
assert_json_eq "$INSPECT_JSON" 'result.staged_change_count' '1'
assert_json_eq "$INSPECT_JSON" 'result.items.0.change_id' "$FIRST_CHANGE_ID"

stop_bridge_daemon
trap - EXIT
bridge_log 'functional_session_drop_change_edges passed'
