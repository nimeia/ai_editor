#!/usr/bin/env bash
set -euo pipefail
DAEMON="$1"
CLI="$2"
RUNDIR="$3"
WS="$RUNDIR/ws_m5"
rm -rf "$WS"
mkdir -p "$WS/docs"
printf 'hello\nworld\n' > "$WS/docs/readme.md"
printf 'hello\nbridge\n' > "$RUNDIR/new_content.txt"

AI_BRIDGE_HISTORY_ROTATE_BYTES=60 AI_BRIDGE_HISTORY_ROTATE_KEEP=2 AI_BRIDGE_BACKUP_KEEP=2 AI_BRIDGE_PREVIEW_KEEP=2 AI_BRIDGE_PREVIEW_STATUS_KEEP=2 \
  "$DAEMON" --workspace "$WS" > "$RUNDIR/daemon_m5.log" 2>&1 &
DAEMON_PID=$!
cleanup() {
  kill "$DAEMON_PID" >/dev/null 2>&1 || true
  wait "$DAEMON_PID" >/dev/null 2>&1 || true
}
trap cleanup EXIT
sleep 0.5

PREVIEW_OUT=$("$CLI" patch-preview --workspace "$WS" --path docs/readme.md --new-content-file "$RUNDIR/new_content.txt" --json)
PREVIEW_ID=$(printf '%s' "$PREVIEW_OUT" | sed -n 's/.*"preview_id":"\([^"]*\)".*/\1/p')
APPLY_OUT=$("$CLI" patch-apply --workspace "$WS" --path docs/readme.md --preview-id "$PREVIEW_ID" --json)
BACKUP_ID=$(printf '%s' "$APPLY_OUT" | sed -n 's/.*"backup_id":"\([^"]*\)".*/\1/p')
HISTORY_OUT=$("$CLI" history --workspace "$WS" --path docs/readme.md --json)
ROLLBACK_OUT=$("$CLI" patch-rollback --workspace "$WS" --path docs/readme.md --backup-id "$BACKUP_ID" --json)

[[ -n "$PREVIEW_ID" ]]
[[ "$PREVIEW_OUT" == *'"applicable":true'* ]]
[[ "$PREVIEW_OUT" == *'"preview_created_at":"'* ]]
[[ "$PREVIEW_OUT" == *'+bridge'* ]]
[[ "$APPLY_OUT" == *'"applied":true'* ]]
[[ "$APPLY_OUT" == *'"preview_status":"applied"'* ]]
[[ "$APPLY_OUT" == *"\"preview_id\":\"$PREVIEW_ID\""* ]]
[[ -n "$BACKUP_ID" ]]
[[ "$HISTORY_OUT" == *'"method":"patch.apply.preview"'* ]]
[[ "$ROLLBACK_OUT" == *'"rolled_back":true'* ]]
[[ "$ROLLBACK_OUT" == *'"current_hash":"'* ]]
grep -q '^world$' "$WS/docs/readme.md"

set +e
CONSUMED_OUT=$("$CLI" patch-apply --workspace "$WS" --path docs/readme.md --preview-id "$PREVIEW_ID" --json 2>&1)
CONSUMED_CODE=$?
set -e
[[ "$CONSUMED_CODE" -ne 0 ]]
[[ "$CONSUMED_OUT" == *'"code":"PREVIEW_CONSUMED"'* ]]
[[ "$CONSUMED_OUT" == *'preview already applied'* ]]

PREVIEW_DIRECT=$("$CLI" patch-preview --workspace "$WS" --path docs/readme.md --new-content-file "$RUNDIR/new_content.txt" --json)
BASE_MTIME=$(printf '%s' "$PREVIEW_DIRECT" | sed -n 's/.*"current_mtime":"\([^"]*\)".*/\1/p')
BASE_HASH=$(printf '%s' "$PREVIEW_DIRECT" | sed -n 's/.*"current_hash":"\([^"]*\)".*/\1/p')
DIRECT_APPLY=$("$CLI" patch-apply --workspace "$WS" --path docs/readme.md --new-content-file "$RUNDIR/new_content.txt" --base-mtime "$BASE_MTIME" --base-hash "$BASE_HASH" --json)
[[ "$DIRECT_APPLY" == *'"applied":true'* ]]

for i in 0 1 2 3 4 5; do
  printf 'iter-%s\n' "$i" > "$RUNDIR/iter.txt"
  P=$("$CLI" patch-preview --workspace "$WS" --path docs/readme.md --new-content-file "$RUNDIR/iter.txt" --json)
  PID=$(printf '%s' "$P" | sed -n 's/.*"preview_id":"\([^"]*\)".*/\1/p')
  "$CLI" patch-apply --workspace "$WS" --path docs/readme.md --preview-id "$PID" --json >/dev/null
 done

[[ -f "$WS/.bridge/history.log" ]]
[[ -f "$WS/.bridge/history.log.1" ]]
BACKUP_COUNT=$(find "$WS/.bridge/backups" -type f | wc -l | tr -d ' ')
[[ "$BACKUP_COUNT" -le 2 ]]
PREVIEW_COUNT=$(find "$WS/.bridge/previews" -name '*.meta' -type f | wc -l | tr -d ' ')
[[ "$PREVIEW_COUNT" -le 2 ]]
PREVIEW_STATUS_COUNT=$(find "$WS/.bridge/previews" -name '*.status' -type f | wc -l | tr -d ' ')
[[ "$PREVIEW_STATUS_COUNT" -le 2 ]]
