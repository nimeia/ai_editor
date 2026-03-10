#!/usr/bin/env bash
set -euo pipefail
DAEMON="$1"
CLI="$2"
RUNDIR="$3"
WS="$RUNDIR/ws_m3"
rm -rf "$WS"
mkdir -p "$WS/docs" "$WS/node_modules/pkg"
printf 'alpha\nbeta\ngamma\n' > "$WS/docs/readme.md"
printf 'console.log(1);\n' > "$WS/node_modules/pkg/index.js"

"$DAEMON" --workspace "$WS" > "$RUNDIR/daemon_m3.log" 2>&1 &
DAEMON_PID=$!
cleanup() {
  kill "$DAEMON_PID" >/dev/null 2>&1 || true
  wait "$DAEMON_PID" >/dev/null 2>&1 || true
}
trap cleanup EXIT
sleep 0.5

LIST_OUT=$("$CLI" list --workspace "$WS" --json)
STAT_OUT=$("$CLI" stat --workspace "$WS" --path docs/readme.md --json)
READ_OUT=$("$CLI" read --workspace "$WS" --path docs/readme.md --json)
RANGE_OUT=$("$CLI" read-range --workspace "$WS" --path docs/readme.md --start 2 --end 3 --json)
READ_EXCLUDED_OUT=$("$CLI" read --workspace "$WS" --path node_modules/pkg/index.js --json)
RECURSIVE_OUT=$("$CLI" list --workspace "$WS" --json --recursive)

[[ "$LIST_OUT" == *'"path":"docs"'* ]]
[[ "$LIST_OUT" == *'"path":"node_modules"'* ]]
[[ "$STAT_OUT" == *'"kind":"file"'* ]]
[[ "$READ_OUT" == *'alpha\nbeta\ngamma\n'* ]]
[[ "$RANGE_OUT" == *'beta\ngamma'* ]]
[[ "$READ_EXCLUDED_OUT" == *'"policy":"skip_by_default"'* ]]
[[ "$RECURSIVE_OUT" != *'node_modules/pkg/index.js'* ]]

MISSING_OUT=$("$CLI" stat --workspace "$WS" --path docs/missing.md --json || true)
OUTSIDE_OUT=$("$CLI" read --workspace "$WS" --path ../outside.txt --json || true)
INVALID_RANGE_OUT=$("$CLI" read-range --workspace "$WS" --path docs/readme.md --start 3 --end 2 --json || true)

[[ "$MISSING_OUT" == *'"code":"FILE_NOT_FOUND"'* ]]
[[ "$OUTSIDE_OUT" == *'"code":"PATH_OUTSIDE_WORKSPACE"'* ]]
[[ "$INVALID_RANGE_OUT" == *'"code":"INVALID_PARAMS"'* ]]
