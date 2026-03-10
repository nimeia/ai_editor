#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/helpers/common.sh"
DAEMON="$1"
CLI="$2"
RUN_DIR="$3"
WS="$RUN_DIR/ws_functional_workspace"
rm -rf "$WS"
mkdir -p "$WS/docs dir"
printf 'hello\nworkspace bridge\n' > "$WS/docs dir/hello 世界.md"
setup_bridge_test "$DAEMON" "$CLI" "$RUN_DIR" "$WS"
start_bridge_daemon functional_workspace
PING_JSON="$(invoke_bridge_cli ping --workspace "$WS" --json)"
INFO_JSON="$(invoke_bridge_cli info --workspace "$WS" --json)"
OPEN_JSON="$(invoke_bridge_cli open --workspace "$WS" --json)"
RESOLVE_JSON="$(invoke_bridge_cli resolve --workspace "$WS" --path 'docs dir/hello 世界.md' --json)"
LIST_JSON="$(invoke_bridge_cli list --workspace "$WS" --recursive --json)"
assert_contains "$PING_JSON" '"ok":true'
assert_contains "$PING_JSON" '"message":"pong"'
assert_contains "$INFO_JSON" '"ok":true'
assert_contains "$INFO_JSON" 'ws_functional_workspace'
assert_contains "$INFO_JSON" '"profile":"default"'
assert_contains "$INFO_JSON" '"policy":"default"'
assert_contains "$INFO_JSON" '"platform":"posix"'
assert_contains "$INFO_JSON" '"transport":"posix-unix-socket"'
BRIDGE_RUNTIME_DIR=$(printf '%s' "$INFO_JSON" | sed -n 's/.*"runtime_dir":"\([^"]*\)".*/\1/p')
[[ -n "$BRIDGE_RUNTIME_DIR" && -d "$BRIDGE_RUNTIME_DIR" ]] || bridge_fail "runtime dir missing: $BRIDGE_RUNTIME_DIR"
assert_contains "$OPEN_JSON" '"ok":true'
assert_contains "$OPEN_JSON" '"instance_key":"'
assert_contains "$OPEN_JSON" '.sock'
assert_contains "$RESOLVE_JSON" '"ok":true'
assert_contains "$RESOLVE_JSON" '"relative_path":"docs dir/hello 世界.md"'
assert_contains "$RESOLVE_JSON" '"policy":"normal"'
assert_contains "$RESOLVE_JSON" 'hello 世界.md'
assert_contains "$LIST_JSON" '"ok":true'
assert_contains "$LIST_JSON" '"path":"docs dir"'
assert_contains "$LIST_JSON" '"path":"docs dir/hello 世界.md"'
stop_bridge_daemon
trap - EXIT
bridge_log 'functional_workspace_ops passed'
