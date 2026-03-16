#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
source "$ROOT_DIR/tests/helpers/common.sh"

DAEMON="$1"
CLI="$2"
RUN_DIR="$3"
WS="$RUN_DIR/ws_session_drop_contract"

rm -rf "$WS"
mkdir -p "$WS/docs"
printf 'one
two
three
' > "$WS/docs/note.txt"

setup_bridge_test "$DAEMON" "$CLI" "$RUN_DIR" "$WS"
start_bridge_daemon daemon_session_drop_contract

BEGIN_JSON="$(invoke_bridge_cli session-begin --workspace "$WS" --session-id sess-drop-contract --json)"
assert_json_eq "$BEGIN_JSON" 'result.state' 'created'

FIRST_FILE="$RUN_DIR/session_first.txt"
SECOND_FILE="$RUN_DIR/session_second.txt"
printf 'ONE' > "$FIRST_FILE"
printf 'THREE!' > "$SECOND_FILE"

FIRST_ADD_JSON="$(invoke_bridge_cli edit-replace-range --workspace "$WS" --session-id sess-drop-contract --path docs/note.txt --start 1 --end 1 --content-file "$FIRST_FILE" --json)"
SECOND_ADD_JSON="$(invoke_bridge_cli edit-replace-range --workspace "$WS" --session-id sess-drop-contract --path docs/note.txt --start 3 --end 3 --content-file "$SECOND_FILE" --json)"
assert_json_eq "$SECOND_ADD_JSON" 'result.staged_change_count' '2'
FIRST_CHANGE_ID="$(json_get "$FIRST_ADD_JSON" 'result.change_id')"
SECOND_CHANGE_ID="$(json_get "$SECOND_ADD_JSON" 'result.change_id')"

DROP_JSON="$(invoke_bridge_cli session-drop-change --workspace "$WS" --session-id sess-drop-contract --change-id "$SECOND_CHANGE_ID" --json)"
assert_json_eq "$DROP_JSON" 'result.change_id' "$SECOND_CHANGE_ID"
assert_json_eq "$DROP_JSON" 'result.staged_change_count' '1'

INSPECT_JSON="$(invoke_bridge_cli session-inspect --workspace "$WS" --session-id sess-drop-contract --json)"
assert_json_eq "$INSPECT_JSON" 'result.staged_change_count' '1'
assert_json_eq "$INSPECT_JSON" 'result.items.0.change_id' "$FIRST_CHANGE_ID"
assert_json_contains "$INSPECT_JSON" 'result.files.0.summary' 'docs/note.txt'

COMMIT_JSON="$(invoke_bridge_cli session-commit --workspace "$WS" --session-id sess-drop-contract --json)"
assert_json_eq "$COMMIT_JSON" 'result.state' 'committed'
FILE_TEXT="$(cat "$WS/docs/note.txt")"
assert_contains "$FILE_TEXT" 'ONE'
assert_not_contains "$FILE_TEXT" 'THREE!'

stop_bridge_daemon
trap - EXIT
bridge_log 'functional_session_drop_change_contract passed'
