#!/usr/bin/env bash
set -euo pipefail
DAEMON="$1"
CLI="$2"
RUNDIR="$3"
WS="$RUNDIR/ws_timeout"
rm -rf "$WS"
mkdir -p "$WS/docs"
python - <<'PY' "$WS/docs/big.txt" "$RUNDIR/new_big_content.txt"
from pathlib import Path
import sys
big = Path(sys.argv[1])
newf = Path(sys.argv[2])
text = ''.join(f"line {i} token\n" for i in range(1200))
big.write_text(text, encoding='utf-8')
newf.write_text(text.replace('token', 'updated-token'), encoding='utf-8')
PY

AI_BRIDGE_SEARCH_DELAY_MS=2 AI_BRIDGE_READ_STREAM_DELAY_MS=2 AI_BRIDGE_PATCH_STREAM_DELAY_MS=2 "$DAEMON" --workspace "$WS" > "$RUNDIR/daemon_timeout.log" 2>&1 &
DAEMON_PID=$!
cleanup() {
  kill "$DAEMON_PID" >/dev/null 2>&1 || true
  wait "$DAEMON_PID" >/dev/null 2>&1 || true
}
trap cleanup EXIT
sleep 0.5

SEARCH_OUT=$("$CLI" search-text --workspace "$WS" --path docs/big.txt --query definitely-not-present --request-id req-timeout-search --timeout-ms 5 --json || true)
[[ "$SEARCH_OUT" == *'"code":"SEARCH_TIMEOUT"'* ]]

READ_OUT=$("$CLI" read --workspace "$WS" --path docs/big.txt --request-id req-timeout-read --stream --chunk-bytes 64 --timeout-ms 5 --json || true)
[[ "$READ_OUT" == *'"code":"REQUEST_TIMEOUT"'* ]]

PATCH_OUT=$("$CLI" patch-preview --workspace "$WS" --path docs/big.txt --new-content-file "$RUNDIR/new_big_content.txt" --request-id req-timeout-patch --stream --chunk-bytes 64 --timeout-ms 5 --json || true)
[[ "$PATCH_OUT" == *'"code":"REQUEST_TIMEOUT"'* ]]
