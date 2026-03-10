#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/helpers/common.sh"
DAEMON="$1"
CLI="$2"
RUN_DIR="$3"
WS="$RUN_DIR/ws_functional_search"
rm -rf "$WS"
mkdir -p "$WS/docs/sub" "$WS/node_modules/pkg"
printf 'alpha token\nbeta keep\n' > "$WS/docs/readme.md"
printf 'int main() {}\n// token in code\n' > "$WS/docs/sub/code.cpp"
printf "console.log('token from excluded');\n" > "$WS/node_modules/pkg/index.js"
setup_bridge_test "$DAEMON" "$CLI" "$RUN_DIR" "$WS"
start_bridge_daemon functional_search
TEXT_JSON="$(invoke_bridge_cli search-text --workspace "$WS" --query token --exts .md,.cpp --json)"
REGEX_JSON="$(invoke_bridge_cli search-regex --workspace "$WS" --path docs/sub --pattern 'main' --before 0 --after 0 --json)"
INCL_JSON="$(invoke_bridge_cli search-text --workspace "$WS" --query token --include-excluded --json)"
INVALID_FAIL="$(invoke_bridge_cli_allow_fail search-regex --workspace "$WS" --pattern '[' --json)"
NO_MATCH_JSON="$(invoke_bridge_cli search-text --workspace "$WS" --query definitely-not-present --json)"
assert_contains "$TEXT_JSON" '"ok":true'
assert_contains "$TEXT_JSON" '"path":"docs/readme.md"'
assert_contains "$TEXT_JSON" '"path":"docs/sub/code.cpp"'
assert_not_contains "$TEXT_JSON" 'node_modules/pkg/index.js'
assert_contains "$TEXT_JSON" '"timed_out":false'
assert_contains "$REGEX_JSON" '"ok":true'
assert_contains "$REGEX_JSON" '"path":"docs/sub/code.cpp"'
assert_contains "$REGEX_JSON" 'int main() {}'
assert_contains "$INCL_JSON" '"path":"node_modules/pkg/index.js"'
assert_contains "$INVALID_FAIL" 'INVALID_PARAMS'
assert_contains "$INVALID_FAIL" 'regular expression'
assert_contains "$NO_MATCH_JSON" '"ok":true'
assert_contains "$NO_MATCH_JSON" '"matches":[]'
stop_bridge_daemon
trap - EXIT
bridge_log 'functional_search_ops passed'
