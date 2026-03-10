#!/usr/bin/env bash
set -euo pipefail
DAEMON="$1"
CLI="$2"
RUNDIR="$3"
WS="$RUNDIR/ws_m4"
rm -rf "$WS"
mkdir -p "$WS/docs" "$WS/node_modules/pkg"
printf 'alpha\nbeta token\ngamma\n' > "$WS/docs/readme.md"
printf 'int main() {}\n// token here\n' > "$WS/docs/code.cpp"
printf "console.log('token');\n" > "$WS/node_modules/pkg/index.js"

"$DAEMON" --workspace "$WS" > "$RUNDIR/daemon_m4.log" 2>&1 &
DAEMON_PID=$!
cleanup() {
  kill "$DAEMON_PID" >/dev/null 2>&1 || true
  wait "$DAEMON_PID" >/dev/null 2>&1 || true
}
trap cleanup EXIT
sleep 0.5

TEXT_OUT=$("$CLI" search-text --workspace "$WS" --query token --exts .md,.cpp --json)
REGEX_OUT=$("$CLI" search-regex --workspace "$WS" --pattern 'main' --exts .cpp --before 0 --after 0 --json)
INCL_OUT=$("$CLI" search-text --workspace "$WS" --query token --include-excluded --json)

[[ "$TEXT_OUT" == *'"path":"docs/readme.md"'* ]]
[[ "$TEXT_OUT" == *'"path":"docs/code.cpp"'* ]]
[[ "$TEXT_OUT" != *'node_modules/pkg/index.js'* ]]
[[ "$REGEX_OUT" == *'"path":"docs/code.cpp"'* ]]
[[ "$INCL_OUT" == *'"path":"node_modules/pkg/index.js"'* ]]
