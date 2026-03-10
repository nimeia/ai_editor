#!/usr/bin/env bash
set -euo pipefail
DAEMON="$1"
CLI="$2"
RUNDIR="$3"
WS="$RUNDIR/ws_read_stream"
rm -rf "$WS"
mkdir -p "$WS/docs"
python3 - <<'PY2' "$WS/docs/big.txt"
import sys
p = sys.argv[1]
with open(p, 'w', encoding='utf-8') as f:
    for i in range(1, 2001):
        f.write(f"line-{i:04d} token token token\n")
PY2

"$DAEMON" --workspace "$WS" > "$RUNDIR/daemon_read_stream.log" 2>&1 &
DAEMON_PID=$!
cleanup() {
  kill "$DAEMON_PID" >/dev/null 2>&1 || true
  wait "$DAEMON_PID" >/dev/null 2>&1 || true
}
trap cleanup EXIT
sleep 0.5

STREAM_OUT=$("$CLI" read --workspace "$WS" --path docs/big.txt --max-bytes 131072 --chunk-bytes 2048 --stream --json)
[[ "$STREAM_OUT" == *'"type":"chunk"'* ]]
[[ "$STREAM_OUT" == *'"event":"fs.read.chunk"'* ]]
[[ "$STREAM_OUT" == *'"type":"final"'* ]]
[[ "$STREAM_OUT" == *'"ok":true'* ]]
[[ "$STREAM_OUT" == *'"path":"docs/big.txt"'* ]]
[[ "$STREAM_OUT" == *'"chunk_count":'* ]]
[[ "$STREAM_OUT" == *'"stream_event":"fs.read.chunk"'* ]]
[[ "$STREAM_OUT" == *'"timed_out":false'* ]]
python3 - <<'PY3' "$STREAM_OUT"
import json, sys
joined = []
for line in sys.argv[1].splitlines():
    obj = json.loads(line)
    if obj.get('type') == 'chunk':
        joined.append(obj['data']['content'])
text = ''.join(joined)
assert 'line-0001 token token token\n' in text
assert 'line-2000 token token token\n' in text
PY3

RANGE_OUT=$("$CLI" read-range --workspace "$WS" --path docs/big.txt --start 10 --end 12 --chunk-bytes 8 --stream --json)
[[ "$RANGE_OUT" == *'"event":"fs.read_range.chunk"'* ]]
[[ "$RANGE_OUT" == *'"type":"final"'* ]]
[[ "$RANGE_OUT" == *'"stream_event":"fs.read_range.chunk"'* ]]
[[ "$RANGE_OUT" == *'"timed_out":false'* ]]
python3 - <<'PY4' "$RANGE_OUT"
import json, sys
joined = []
for line in sys.argv[1].splitlines():
    obj = json.loads(line)
    if obj.get('type') == 'chunk':
        joined.append(obj['data']['content'])
text = ''.join(joined)
assert text == 'line-0010 token token token\nline-0011 token token token\nline-0012 token token token'
PY4
