#!/usr/bin/env bash
set -euo pipefail
DAEMON="$1"
CLI="$2"
RUNDIR="$3"
WS="$RUNDIR/ws_cancel"
rm -rf "$WS"
mkdir -p "$WS/docs"
python - <<'PY' "$WS/docs/big.txt"
from pathlib import Path
import sys
p=Path(sys.argv[1])
with p.open('w', encoding='utf-8') as f:
    for i in range(3000):
        f.write(f"line {i} token\n")
PY

AI_BRIDGE_SEARCH_DELAY_MS=2 AI_BRIDGE_READ_STREAM_DELAY_MS=2 "$DAEMON" --workspace "$WS" > "$RUNDIR/daemon_cancel.log" 2>&1 &
DAEMON_PID=$!
cleanup() {
  kill "$DAEMON_PID" >/dev/null 2>&1 || true
  wait "$DAEMON_PID" >/dev/null 2>&1 || true
}
trap cleanup EXIT
sleep 0.5

SEARCH_OUT_FILE="$RUNDIR/search_cancel.out"
set +e
"$CLI" search-text --workspace "$WS" --path docs/big.txt --query definitely-not-present --request-id req-cancel-me --timeout-ms 30000 --json > "$SEARCH_OUT_FILE" 2>&1 &
SEARCH_PID=$!
set -e
sleep 0.2
CANCEL_OUT=$("$CLI" cancel --workspace "$WS" --target-request-id req-cancel-me --request-id req-cancel-sender --json)
wait "$SEARCH_PID" || true
SEARCH_OUT=$(cat "$SEARCH_OUT_FILE")

[[ "$CANCEL_OUT" == *'"ok":true'* ]]
[[ "$CANCEL_OUT" == *'"target_request_id":"req-cancel-me"'* ]]
[[ "$SEARCH_OUT" == *'"code":"REQUEST_CANCELLED"'* ]]


READ_OUT_FILE="$RUNDIR/read_cancel.out"
set +e
"$CLI" read --workspace "$WS" --path docs/big.txt --request-id req-read-cancel-me --stream --chunk-bytes 64 --timeout-ms 30000 --json > "$READ_OUT_FILE" 2>&1 &
READ_PID=$!
set -e
sleep 0.2
READ_CANCEL_OUT=$("$CLI" cancel --workspace "$WS" --target-request-id req-read-cancel-me --request-id req-read-cancel-sender --json)
wait "$READ_PID" || true
READ_OUT=$(cat "$READ_OUT_FILE")
[[ "$READ_CANCEL_OUT" == *'"ok":true'* ]]
[[ "$READ_CANCEL_OUT" == *'"target_request_id":"req-read-cancel-me"'* ]]
[[ "$READ_OUT" == *'"code":"REQUEST_CANCELLED"'* ]]
