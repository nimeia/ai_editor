#!/usr/bin/env bash
set -euo pipefail
DAEMON="$1"
CLI="$2"
RUNDIR="$3"
WS="$RUNDIR/ws_stream"
rm -rf "$WS"
mkdir -p "$WS/docs"
printf 'alpha token\nbeta token\ngamma\n' > "$WS/docs/readme.md"
printf 'token in code\n' > "$WS/docs/code.cpp"

"$DAEMON" --workspace "$WS" > "$RUNDIR/daemon_stream.log" 2>&1 &
DAEMON_PID=$!
cleanup() {
  kill "$DAEMON_PID" >/dev/null 2>&1 || true
  wait "$DAEMON_PID" >/dev/null 2>&1 || true
}
trap cleanup EXIT
sleep 0.5

STREAM_OUT=$("$CLI" search-text --workspace "$WS" --query token --exts .md,.cpp --stream --json)
[[ "$STREAM_OUT" == *'"type":"chunk"'* ]]
[[ "$STREAM_OUT" == *'"event":"search.match"'* ]]
[[ "$STREAM_OUT" == *'"path":"docs/readme.md"'* ]]
[[ "$STREAM_OUT" == *'"path":"docs/code.cpp"'* ]]
[[ "$STREAM_OUT" == *'"type":"final"'* ]]
[[ "$STREAM_OUT" == *'"ok":true'* ]]
[[ "$STREAM_OUT" == *'"stream_event":"search.match"'* ]]
[[ "$STREAM_OUT" == *'"timed_out":false'* ]]
