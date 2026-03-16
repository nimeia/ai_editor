#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
source "$ROOT_DIR/tests/helpers/common.sh"

DAEMON="$1"
CLI="$2"
RUN_DIR="$3"
WS="$RUN_DIR/ws_recovery"

rm -rf "$WS"
mkdir -p "$WS/docs"
printf 'alpha\nbeta token\ngamma\n' > "$WS/docs/block.txt"
printf 'one\ntwo\nthree\n' > "$WS/docs/note.txt"

setup_bridge_test "$DAEMON" "$CLI" "$RUN_DIR" "$WS"
start_bridge_daemon daemon_recovery

BEGIN_JSON="$(invoke_bridge_cli session-begin --workspace "$WS" --session-id sess-rebase --json)"
assert_json_eq "$BEGIN_JSON" 'result.session_id' 'sess-rebase'
INSERT_FILE="$RUN_DIR/recovery_insert.txt"
printf '\nrebased' > "$INSERT_FILE"
ADD_JSON="$(invoke_bridge_cli edit-insert-after --workspace "$WS" --session-id sess-rebase --path docs/block.txt --query 'beta token' --content-file "$INSERT_FILE" --json)"
assert_json_eq "$ADD_JSON" 'result.path' 'docs/block.txt'

printf 'HEADER\nalpha\nbeta token\ngamma\n' > "$WS/docs/block.txt"
CHECK_JSON="$(invoke_bridge_cli recovery-check --workspace "$WS" --session-id sess-rebase --json)"
assert_json_truthy "$CHECK_JSON" 'result.recoverable'
assert_json_eq "$CHECK_JSON" 'result.rebase_file_count' '1'
assert_json_eq "$CHECK_JSON" 'result.files.0.status' 'rebase_required'

COMMIT_FAIL="$(invoke_bridge_cli_allow_fail session-commit --workspace "$WS" --session-id sess-rebase --json)"
[[ "$COMMIT_FAIL" == 1* ]] || bridge_fail 'expected session-commit to fail before rebase'
assert_contains "$COMMIT_FAIL" 'SESSION_REBASE_REQUIRED'

REBASE_JSON="$(invoke_bridge_cli recovery-rebase --workspace "$WS" --session-id sess-rebase --json)"
assert_json_eq "$REBASE_JSON" 'result.rebased_file_count' '1'
COMMIT_JSON="$(invoke_bridge_cli session-commit --workspace "$WS" --session-id sess-rebase --json)"
assert_json_eq "$COMMIT_JSON" 'result.state' 'committed'
FILE_TEXT="$(cat "$WS/docs/block.txt")"
assert_contains "$FILE_TEXT" 'HEADER'
assert_contains "$FILE_TEXT" 'rebased'

BEGIN_CONFLICT="$(invoke_bridge_cli session-begin --workspace "$WS" --session-id sess-conflict --json)"
assert_json_eq "$BEGIN_CONFLICT" 'result.session_id' 'sess-conflict'
RANGE_FILE="$RUN_DIR/recovery_replace.txt"
printf 'SECOND' > "$RANGE_FILE"
RANGE_JSON="$(invoke_bridge_cli edit-replace-range --workspace "$WS" --session-id sess-conflict --path docs/note.txt --start 2 --end 2 --content-file "$RANGE_FILE" --json)"
assert_json_eq "$RANGE_JSON" 'result.path' 'docs/note.txt'
printf 'one\nmutated\nthree\n' > "$WS/docs/note.txt"
CONFLICT_JSON="$(invoke_bridge_cli recovery-check --workspace "$WS" --session-id sess-conflict --json)"
assert_json_falsey "$CONFLICT_JSON" 'result.recoverable'
assert_json_eq "$CONFLICT_JSON" 'result.conflict_file_count' '1'
assert_json_eq "$CONFLICT_JSON" 'result.files.0.status' 'conflict'
