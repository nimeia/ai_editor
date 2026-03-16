#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
source "$ROOT_DIR/tests/helpers/common.sh"

DAEMON="$1"
CLI="$2"
RUN_DIR="$3"
WS="$RUN_DIR/ws_session"

rm -rf "$WS"
mkdir -p "$WS/docs"
printf 'one
two
three
' > "$WS/docs/note.txt"

setup_bridge_test "$DAEMON" "$CLI" "$RUN_DIR" "$WS"
start_bridge_daemon daemon_session

BEGIN_JSON="$(invoke_bridge_cli session-begin --workspace "$WS" --session-id sess-e2e --json)"
assert_json_eq "$BEGIN_JSON" 'result.session_id' 'sess-e2e'
assert_json_eq "$BEGIN_JSON" 'result.state' 'created'

NEW_CONTENT_FILE="$RUN_DIR/session_replace.txt"
printf 'TWO' > "$NEW_CONTENT_FILE"
ADD_JSON="$(invoke_bridge_cli edit-replace-range --workspace "$WS" --session-id sess-e2e --path docs/note.txt --start 2 --end 2 --content-file "$NEW_CONTENT_FILE" --json)"
assert_json_eq "$ADD_JSON" 'result.session_id' 'sess-e2e'
assert_json_eq "$ADD_JSON" 'result.path' 'docs/note.txt'
assert_json_eq "$ADD_JSON" 'result.staged_change_count' '1'

INSPECT_JSON="$(invoke_bridge_cli session-inspect --workspace "$WS" --session-id sess-e2e --json)"
assert_json_eq "$INSPECT_JSON" 'result.staged_change_count' '1'
assert_json_eq "$INSPECT_JSON" 'result.items.0.operation' 'replace_range'
assert_json_eq "$INSPECT_JSON" 'result.risk_level' 'low'
assert_json_contains "$INSPECT_JSON" 'result.summary' 'overall risk low'
assert_json_contains "$INSPECT_JSON" 'result.files.0.summary' 'docs/note.txt'

PREVIEW_JSON="$(invoke_bridge_cli session-preview --workspace "$WS" --session-id sess-e2e --json)"
assert_json_eq "$PREVIEW_JSON" 'result.previewed_file_count' '1'
assert_json_eq "$PREVIEW_JSON" 'result.risk_level' 'low'
assert_json_truthy "$PREVIEW_JSON" 'result.total_hunk_count'
assert_json_contains "$PREVIEW_JSON" 'result.summary' 'overall risk low'
assert_json_contains "$PREVIEW_JSON" 'result.files.0.diff' 'TWO'
assert_json_contains "$PREVIEW_JSON" 'result.files.0.summary' 'docs/note.txt'

DROP_FILE="$RUN_DIR/session_drop.txt"
printf 'THREE' > "$DROP_FILE"
SECOND_ADD_JSON="$(invoke_bridge_cli edit-replace-range --workspace "$WS" --session-id sess-e2e --path docs/note.txt --start 3 --end 3 --content-file "$DROP_FILE" --json)"
assert_json_eq "$SECOND_ADD_JSON" 'result.staged_change_count' '2'
SECOND_CHANGE_ID="$(json_get "$SECOND_ADD_JSON" 'result.change_id')"
DROP_JSON="$(invoke_bridge_cli session-drop-change --workspace "$WS" --session-id sess-e2e --change-id "$SECOND_CHANGE_ID" --json)"
assert_json_eq "$DROP_JSON" 'result.staged_change_count' '1'
assert_json_eq "$DROP_JSON" 'result.change_id' "$SECOND_CHANGE_ID"

COMMIT_JSON="$(invoke_bridge_cli session-commit --workspace "$WS" --session-id sess-e2e --json)"
assert_json_eq "$COMMIT_JSON" 'result.state' 'committed'
assert_json_eq "$COMMIT_JSON" 'result.committed_file_count' '1'

FILE_TEXT="$(cat "$WS/docs/note.txt")"
assert_contains "$FILE_TEXT" 'TWO'
assert_not_contains "$FILE_TEXT" 'THREE'
