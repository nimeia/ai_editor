#!/usr/bin/env bash
set -euo pipefail
DAEMON="$1"
CLI="$2"
RUNDIR="$3"
WS="$RUNDIR/ws_patch_stream"
rm -rf "$WS"
mkdir -p "$WS/docs"
python3 - <<'PY2' "$WS/docs/big.md"
import sys
p=sys.argv[1]
with open(p,'w',encoding='utf-8') as f:
    for i in range(1,1201):
        f.write(f"old-line-{i:04d}\n")
PY2
python3 - <<'PY3' "$RUNDIR/new_big_content.txt"
import sys
p=sys.argv[1]
with open(p,'w',encoding='utf-8') as f:
    for i in range(1,1201):
        f.write(f"new-line-{i:04d}\n")
PY3
"$DAEMON" --workspace "$WS" > "$RUNDIR/daemon_patch_stream.log" 2>&1 &
DAEMON_PID=$!
cleanup() {
  kill "$DAEMON_PID" >/dev/null 2>&1 || true
  wait "$DAEMON_PID" >/dev/null 2>&1 || true
}
trap cleanup EXIT
sleep 0.5
OUT=$("$CLI" patch-preview --workspace "$WS" --path docs/big.md --new-content-file "$RUNDIR/new_big_content.txt" --chunk-bytes 2048 --stream --json)
[[ "$OUT" == *'"type":"chunk"'* ]]
[[ "$OUT" == *'"event":"patch.preview.chunk"'* ]]
[[ "$OUT" == *'"type":"final"'* ]]
[[ "$OUT" == *'"ok":true'* ]]
[[ "$OUT" == *'"stream_event":"patch.preview.chunk"'* ]]
[[ "$OUT" == *'"timed_out":false'* ]]
python3 - <<'PY4' "$OUT"
import json, sys
joined=[]
final=None
for line in sys.argv[1].splitlines():
    obj=json.loads(line)
    if obj.get('type')=='chunk':
        joined.append(obj['data']['content'])
    elif obj.get('type')=='final':
        final=obj
text=''.join(joined)
assert '--- a/docs/big.md\n' in text
assert '+++ b/docs/big.md\n' in text
assert '-old-line-0001' in text
assert '+new-line-1200' in text
assert final and final['result']['chunk_count'] >= 1
assert final['result']['stream_event'] == 'patch.preview.chunk'
assert final['result']['timed_out'] is False
assert final['result']['total_bytes'] >= len(text)
PY4
