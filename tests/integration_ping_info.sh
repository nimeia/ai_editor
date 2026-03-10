#!/usr/bin/env bash
set -euo pipefail
DAEMON="$1"
CLI="$2"
RUNDIR="$3"
WS="$RUNDIR/ws"
mkdir -p "$WS/docs"
echo hello > "$WS/docs/readme.md"

"$DAEMON" --workspace "$WS" > "$RUNDIR/daemon.log" 2>&1 &
DAEMON_PID=$!
cleanup() {
  kill "$DAEMON_PID" >/dev/null 2>&1 || true
  wait "$DAEMON_PID" >/dev/null 2>&1 || true
}
trap cleanup EXIT
sleep 0.5

PING_OUT=$("$CLI" ping --workspace "$WS" --json)
INFO_OUT=$("$CLI" info --workspace "$WS" --json)
OPEN_OUT=$("$CLI" open --workspace "$WS" --json)
RESOLVE_OUT=$("$CLI" resolve --workspace "$WS" --path docs/readme.md --json)

[[ "$PING_OUT" == *'"message":"pong"'* ]]
[[ "$INFO_OUT" == *'"workspace_root"'* ]]
[[ "$OPEN_OUT" == *'"instance_key"'* ]]
[[ "$RESOLVE_OUT" == *'"policy":"normal"'* ]]
