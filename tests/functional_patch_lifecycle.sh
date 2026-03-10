#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/helpers/common.sh"
DAEMON="$1"
CLI="$2"
RUN_DIR="$3"
WS="$RUN_DIR/ws_functional_patch"
rm -rf "$WS"
mkdir -p "$WS/docs"
printf 'hello\nworld\n' > "$WS/docs/readme.md"
printf 'hello\nbridge\n' > "$RUN_DIR/new_content.txt"
printf 'hello\nrollback target\n' > "$RUN_DIR/alt_content.txt"
setup_bridge_test "$DAEMON" "$CLI" "$RUN_DIR" "$WS"
export AI_BRIDGE_HISTORY_ROTATE_BYTES=80
export AI_BRIDGE_HISTORY_ROTATE_KEEP=2
export AI_BRIDGE_BACKUP_KEEP=2
export AI_BRIDGE_PREVIEW_KEEP=2
export AI_BRIDGE_PREVIEW_STATUS_KEEP=2
start_bridge_daemon functional_patch
PREVIEW_JSON="$(invoke_bridge_cli patch-preview --workspace "$WS" --path docs/readme.md --new-content-file "$RUN_DIR/new_content.txt" --json)"
PREVIEW_ID=$(printf '%s' "$PREVIEW_JSON" | sed -n 's/.*"preview_id":"\([^"]*\)".*/\1/p')
APPLY_JSON="$(invoke_bridge_cli patch-apply --workspace "$WS" --path docs/readme.md --preview-id "$PREVIEW_ID" --json)"
BACKUP_ID=$(printf '%s' "$APPLY_JSON" | sed -n 's/.*"backup_id":"\([^"]*\)".*/\1/p')
HISTORY_JSON="$(invoke_bridge_cli history --workspace "$WS" --path docs/readme.md --json)"
ROLLBACK_JSON="$(invoke_bridge_cli patch-rollback --workspace "$WS" --path docs/readme.md --backup-id "$BACKUP_ID" --json)"
CONSUMED_FAIL="$(invoke_bridge_cli_allow_fail patch-apply --workspace "$WS" --path docs/readme.md --preview-id "$PREVIEW_ID" --json)"
assert_contains "$PREVIEW_JSON" '"ok":true'
assert_contains "$PREVIEW_JSON" '"applicable":true'
assert_contains "$PREVIEW_JSON" '"preview_id":"'
assert_contains "$PREVIEW_JSON" '+bridge'
assert_contains "$PREVIEW_JSON" '"preview_created_at":"'
assert_contains "$APPLY_JSON" '"applied":true'
assert_contains "$APPLY_JSON" '"preview_status":"applied"'
assert_contains "$APPLY_JSON" '"backup_id":"'
assert_contains "$HISTORY_JSON" 'patch.apply.preview'
assert_contains "$ROLLBACK_JSON" '"rolled_back":true'
assert_contains "$(cat "$WS/docs/readme.md")" $'hello\nworld'
assert_contains "$CONSUMED_FAIL" 'PREVIEW_CONSUMED'
TTL_WS="$RUN_DIR/ws_functional_patch_ttl"
rm -rf "$TTL_WS"
mkdir -p "$TTL_WS/docs"
printf 'one\ntwo\n' > "$TTL_WS/docs/ttl.txt"
stop_bridge_daemon
BRIDGE_WORKSPACE="$TTL_WS"
export AI_BRIDGE_PREVIEW_TTL_MS=1
start_bridge_daemon functional_patch_ttl
TTL_PREVIEW_JSON="$(invoke_bridge_cli patch-preview --workspace "$TTL_WS" --path docs/ttl.txt --new-content-file "$RUN_DIR/alt_content.txt" --json)"
TTL_PREVIEW_ID=$(printf '%s' "$TTL_PREVIEW_JSON" | sed -n 's/.*"preview_id":"\([^"]*\)".*/\1/p')
sleep 0.05
TTL_FAIL="$(invoke_bridge_cli_allow_fail patch-apply --workspace "$TTL_WS" --path docs/ttl.txt --preview-id "$TTL_PREVIEW_ID" --json)"
assert_contains "$TTL_FAIL" 'PREVIEW_EXPIRED'
CONFLICT_WS="$RUN_DIR/ws_functional_patch_conflict"
rm -rf "$CONFLICT_WS"
mkdir -p "$CONFLICT_WS/docs"
printf 'start\nbase\n' > "$CONFLICT_WS/docs/conflict.txt"
stop_bridge_daemon
BRIDGE_WORKSPACE="$CONFLICT_WS"
unset AI_BRIDGE_PREVIEW_TTL_MS
start_bridge_daemon functional_patch_conflict
CONFLICT_PREVIEW_JSON="$(invoke_bridge_cli patch-preview --workspace "$CONFLICT_WS" --path docs/conflict.txt --new-content-file "$RUN_DIR/new_content.txt" --json)"
BASE_MTIME=$(printf '%s' "$CONFLICT_PREVIEW_JSON" | sed -n 's/.*"current_mtime":"\([^"]*\)".*/\1/p')
BASE_HASH=$(printf '%s' "$CONFLICT_PREVIEW_JSON" | sed -n 's/.*"current_hash":"\([^"]*\)".*/\1/p')
printf 'start\nmutated\n' > "$CONFLICT_WS/docs/conflict.txt"
CONFLICT_FAIL="$(invoke_bridge_cli_allow_fail patch-apply --workspace "$CONFLICT_WS" --path docs/conflict.txt --new-content-file "$RUN_DIR/new_content.txt" --base-mtime "$BASE_MTIME" --base-hash "$BASE_HASH" --json)"
assert_contains "$CONFLICT_FAIL" 'PATCH_CONFLICT'
if [[ "$CONFLICT_FAIL" != *'mtime_and_hash_changed'* && "$CONFLICT_FAIL" != *'hash_changed'* && "$CONFLICT_FAIL" != *'mtime_changed'* ]]; then
  bridge_fail "expected patch conflict reason to mention mtime/hash change"
fi
stop_bridge_daemon
trap - EXIT
bridge_log 'functional_patch_lifecycle passed'
