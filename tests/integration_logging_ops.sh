#!/usr/bin/env bash
set -euo pipefail
DAEMON="$1"
CLI="$2"
RUNDIR="$3"
WS="$RUNDIR/ws_m6"
rm -rf "$WS"
mkdir -p "$WS/docs"
printf 'hello\nlog\n' > "$WS/docs/readme.md"

AI_BRIDGE_LOG_ROTATE_BYTES=120 AI_BRIDGE_LOG_ROTATE_KEEP=2 "$DAEMON" --workspace "$WS" > "$RUNDIR/daemon_m6.log" 2>&1 &
DAEMON_PID=$!
cleanup() {
  kill "$DAEMON_PID" >/dev/null 2>&1 || true
  wait "$DAEMON_PID" >/dev/null 2>&1 || true
}
trap cleanup EXIT
sleep 0.5

INFO_JSON=$("$CLI" info --workspace "$WS" --json)
RUNTIME_DIR=$(python3 - <<'PY' "$INFO_JSON"
import json, sys
print(json.loads(sys.argv[1])["result"]["runtime_dir"])
PY
)

for _ in $(seq 1 8); do
  "$CLI" stat --workspace "$WS" --path docs/readme.md >/dev/null
  "$CLI" read --workspace "$WS" --path docs/readme.md >/dev/null
 done

RUNTIME_LOG="$RUNTIME_DIR/runtime.log"
AUDIT_LOG="$WS/.bridge/audit.log"

[[ -f "$RUNTIME_LOG" ]]
[[ -f "$AUDIT_LOG" ]]
cat "$RUNTIME_LOG"* | grep -q 'method=fs.stat'
cat "$RUNTIME_LOG"* | grep -q 'method=fs.read'
cat "$RUNTIME_LOG"* | grep -q 'duration_ms='
cat "$RUNTIME_LOG"* | grep -q 'request_bytes='
cat "$RUNTIME_LOG"* | grep -q 'response_bytes='
cat "$AUDIT_LOG"* | grep -q $'fs.stat\tdocs/readme.md\t'
cat "$AUDIT_LOG"* | grep -q $'fs.read\tdocs/readme.md\t'
cat "$AUDIT_LOG"* | grep -q 'aibridge-'
cat "$AUDIT_LOG"* | grep -q $'\tok\tfalse\t'

[[ -f "$RUNTIME_LOG.1" ]]
[[ -f "$AUDIT_LOG.1" ]]
