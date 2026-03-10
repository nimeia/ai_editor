#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/helpers/common.sh"
DAEMON="$1"
CLI="$2"
RUN_DIR="$3"
WS="$RUN_DIR/ws_functional_stream"
rm -rf "$WS"
mkdir -p "$WS/docs"
python3 - <<'PY' "$WS/docs/big.txt" "$RUN_DIR/new_big_content.txt"
from pathlib import Path
import sys
big = Path(sys.argv[1])
newf = Path(sys.argv[2])
with big.open('w', encoding='utf-8') as f:
    for i in range(1, 2401):
        f.write(f"line-{i:04d} token token token\n")
newf.write_text(big.read_text(encoding='utf-8').replace('token', 'updated-token'), encoding='utf-8')
PY
setup_bridge_test "$DAEMON" "$CLI" "$RUN_DIR" "$WS"
export AI_BRIDGE_SEARCH_DELAY_MS=2
export AI_BRIDGE_READ_STREAM_DELAY_MS=2
export AI_BRIDGE_PATCH_STREAM_DELAY_MS=2
start_bridge_daemon functional_stream
SEARCH_STREAM_OUT="$(invoke_bridge_cli search-text --workspace "$WS" --path docs/big.txt --query token --stream --json)"
READ_STREAM_FILE="$RUN_DIR/functional_read_stream.jsonl"
RANGE_STREAM_FILE="$RUN_DIR/functional_range_stream.jsonl"
PATCH_STREAM_FILE="$RUN_DIR/functional_patch_stream.jsonl"
invoke_bridge_cli read --workspace "$WS" --path docs/big.txt --max-bytes 131072 --chunk-bytes 2048 --stream --json > "$READ_STREAM_FILE"
invoke_bridge_cli read-range --workspace "$WS" --path docs/big.txt --start 10 --end 12 --chunk-bytes 8 --stream --json > "$RANGE_STREAM_FILE"
invoke_bridge_cli patch-preview --workspace "$WS" --path docs/big.txt --new-content-file "$RUN_DIR/new_big_content.txt" --chunk-bytes 2048 --stream --json > "$PATCH_STREAM_FILE"
READ_STREAM_OUT="$(cat "$READ_STREAM_FILE")"
RANGE_STREAM_OUT="$(cat "$RANGE_STREAM_FILE")"
PATCH_STREAM_OUT="$(cat "$PATCH_STREAM_FILE")"
assert_contains "$SEARCH_STREAM_OUT" '"type":"chunk"'
assert_contains "$SEARCH_STREAM_OUT" '"event":"search.match"'
assert_contains "$SEARCH_STREAM_OUT" '"type":"final"'
assert_contains "$SEARCH_STREAM_OUT" '"stream_event":"search.match"'
assert_contains "$SEARCH_STREAM_OUT" '"timed_out":false'
assert_contains "$READ_STREAM_OUT" '"event":"fs.read.chunk"'
assert_contains "$READ_STREAM_OUT" '"type":"final"'
assert_contains "$READ_STREAM_OUT" '"stream_event":"fs.read.chunk"'
assert_contains "$READ_STREAM_OUT" '"timed_out":false'
python3 - <<'PY' "$READ_STREAM_FILE"
import json, sys
joined = []
final = None
for line in open(sys.argv[1], encoding='utf-8'):
    obj = json.loads(line)
    if obj.get('type') == 'chunk':
        joined.append(obj['data']['content'])
    elif obj.get('type') == 'final':
        final = obj
text = ''.join(joined)
assert 'line-0001 token token token\n' in text
assert 'line-2000 token token token\n' in text
assert final and final['result']['chunk_count'] >= 2
PY
assert_contains "$RANGE_STREAM_OUT" '"event":"fs.read_range.chunk"'
assert_contains "$RANGE_STREAM_OUT" '"type":"final"'
assert_contains "$RANGE_STREAM_OUT" '"stream_event":"fs.read_range.chunk"'
python3 - <<'PY' "$RANGE_STREAM_FILE"
import json, sys
joined = []
for line in open(sys.argv[1], encoding='utf-8'):
    obj = json.loads(line)
    if obj.get('type') == 'chunk':
        joined.append(obj['data']['content'])
text = ''.join(joined)
assert text == 'line-0010 token token token\nline-0011 token token token\nline-0012 token token token'
PY
assert_contains "$PATCH_STREAM_OUT" '"event":"patch.preview.chunk"'
assert_contains "$PATCH_STREAM_OUT" '"type":"final"'
assert_contains "$PATCH_STREAM_OUT" '"stream_event":"patch.preview.chunk"'
python3 - <<'PY' "$PATCH_STREAM_FILE"
import json, sys
joined = []
final = None
for line in open(sys.argv[1], encoding='utf-8'):
    obj = json.loads(line)
    if obj.get('type') == 'chunk':
        joined.append(obj['data']['content'])
    elif obj.get('type') == 'final':
        final = obj
text = ''.join(joined)
assert '--- a/docs/big.txt\n' in text
assert '+++ b/docs/big.txt\n' in text
assert '-line-0001 token token token' in text
assert '+line-2000 updated-token updated-token updated-token' in text
assert final and final['result']['chunk_count'] >= 2
assert final['result']['timed_out'] is False
PY
SEARCH_CANCEL_OUT_FILE="$RUN_DIR/functional_search_cancel.out"
set +e
"$BRIDGE_CLI" search-text --workspace "$WS" --path docs/big.txt --query definitely-not-present --request-id req-functional-search-cancel --timeout-ms 30000 --json > "$SEARCH_CANCEL_OUT_FILE" 2>&1 &
SEARCH_PID=$!
set -e
sleep 0.2
SEARCH_CANCEL_REPLY="$(invoke_bridge_cli cancel --workspace "$WS" --target-request-id req-functional-search-cancel --request-id req-functional-search-cancel-sender --json)"
wait "$SEARCH_PID" || true
SEARCH_CANCEL_OUT="$(cat "$SEARCH_CANCEL_OUT_FILE")"
assert_contains "$SEARCH_CANCEL_REPLY" '"ok":true'
assert_contains "$SEARCH_CANCEL_REPLY" 'req-functional-search-cancel'
assert_contains "$SEARCH_CANCEL_OUT" 'REQUEST_CANCELLED'
READ_CANCEL_OUT_FILE="$RUN_DIR/functional_read_cancel.out"
set +e
"$BRIDGE_CLI" read --workspace "$WS" --path docs/big.txt --request-id req-functional-read-cancel --stream --chunk-bytes 64 --timeout-ms 30000 --json > "$READ_CANCEL_OUT_FILE" 2>&1 &
READ_PID=$!
set -e
sleep 0.2
READ_CANCEL_REPLY="$(invoke_bridge_cli cancel --workspace "$WS" --target-request-id req-functional-read-cancel --request-id req-functional-read-cancel-sender --json)"
wait "$READ_PID" || true
READ_CANCEL_OUT="$(cat "$READ_CANCEL_OUT_FILE")"
assert_contains "$READ_CANCEL_REPLY" '"ok":true'
assert_contains "$READ_CANCEL_REPLY" 'req-functional-read-cancel'
assert_contains "$READ_CANCEL_OUT" 'REQUEST_CANCELLED'
SEARCH_TIMEOUT_FAIL="$(invoke_bridge_cli_allow_fail search-text --workspace "$WS" --path docs/big.txt --query definitely-not-present --request-id req-functional-timeout-search --timeout-ms 5 --json)"
READ_TIMEOUT_FAIL="$(invoke_bridge_cli_allow_fail read --workspace "$WS" --path docs/big.txt --request-id req-functional-timeout-read --stream --chunk-bytes 64 --timeout-ms 5 --json)"
PATCH_TIMEOUT_FAIL="$(invoke_bridge_cli_allow_fail patch-preview --workspace "$WS" --path docs/big.txt --new-content-file "$RUN_DIR/new_big_content.txt" --request-id req-functional-timeout-patch --stream --chunk-bytes 64 --timeout-ms 5 --json)"
assert_contains "$SEARCH_TIMEOUT_FAIL" 'SEARCH_TIMEOUT'
assert_contains "$READ_TIMEOUT_FAIL" 'REQUEST_TIMEOUT'
assert_contains "$PATCH_TIMEOUT_FAIL" 'REQUEST_TIMEOUT'
stop_bridge_daemon
trap - EXIT
bridge_log 'functional_stream_cancel_timeout passed'
