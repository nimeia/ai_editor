#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/helpers/common.sh"
DAEMON="$1"
CLI="$2"
RUN_DIR="$3"
WS="$RUN_DIR/ws_functional_fs"
rm -rf "$WS"
mkdir -p "$WS/docs" "$WS/node_modules/pkg" "$WS/build/out"
printf 'alpha\r\nbeta\r\ngamma\r\n' > "$WS/docs/windows_eol.txt"
printf 'one\ntwo\nthree\n' > "$WS/docs/range.txt"
printf 'console.log(1);\n' > "$WS/node_modules/pkg/index.js"
python3 - <<'PY' "$WS/docs/utf16.txt" "$WS/docs/blob.bin"
from pathlib import Path
import sys
Path(sys.argv[1]).write_bytes(b'\xff\xfeh\x00i\x00\n\x00')
Path(sys.argv[2]).write_bytes(b'\x00\x01\x02abc')
PY
setup_bridge_test "$DAEMON" "$CLI" "$RUN_DIR" "$WS"
start_bridge_daemon functional_fs
LIST_JSON="$(invoke_bridge_cli list --workspace "$WS" --json --recursive)"
LIST_INCL_JSON="$(invoke_bridge_cli list --workspace "$WS" --json --recursive --include-excluded)"
STAT_JSON="$(invoke_bridge_cli stat --workspace "$WS" --path docs/windows_eol.txt --json)"
READ_JSON="$(invoke_bridge_cli read --workspace "$WS" --path docs/windows_eol.txt --json)"
RANGE_JSON="$(invoke_bridge_cli read-range --workspace "$WS" --path docs/range.txt --start 2 --end 3 --json)"
UTF16_STAT_JSON="$(invoke_bridge_cli stat --workspace "$WS" --path docs/utf16.txt --json)"
BINARY_STAT_JSON="$(invoke_bridge_cli stat --workspace "$WS" --path docs/blob.bin --json)"
UTF16_READ_FAIL="$(invoke_bridge_cli_allow_fail read --workspace "$WS" --path docs/utf16.txt --json)"
BINARY_READ_FAIL="$(invoke_bridge_cli_allow_fail read --workspace "$WS" --path docs/blob.bin --json)"
OUTSIDE_FAIL="$(invoke_bridge_cli_allow_fail read --workspace "$WS" --path ../outside.txt --json)"
MISSING_FAIL="$(invoke_bridge_cli_allow_fail stat --workspace "$WS" --path docs/missing.txt --json)"
assert_contains "$LIST_JSON" '"ok":true'
assert_contains "$LIST_JSON" '"path":"docs"'
assert_not_contains "$LIST_JSON" 'node_modules/pkg/index.js'
assert_contains "$LIST_INCL_JSON" '"path":"node_modules/pkg/index.js"'
assert_contains "$STAT_JSON" '"kind":"file"'
assert_contains "$STAT_JSON" '"encoding":"utf-8"'
assert_contains "$STAT_JSON" '"eol":"crlf"'
assert_contains "$STAT_JSON" '"binary":false'
assert_contains "$READ_JSON" 'alpha\r\nbeta\r\ngamma\r\n'
assert_contains "$RANGE_JSON" 'two\nthree'
assert_contains "$UTF16_STAT_JSON" '"encoding":"utf-16le"'
assert_contains "$UTF16_STAT_JSON" '"bom":true'
assert_contains "$UTF16_STAT_JSON" '"binary":true'
assert_contains "$BINARY_STAT_JSON" '"binary":true'
assert_contains "$UTF16_READ_FAIL" 'BINARY_FILE'
assert_contains "$BINARY_READ_FAIL" 'BINARY_FILE'
assert_contains "$OUTSIDE_FAIL" 'PATH_OUTSIDE_WORKSPACE'
assert_contains "$MISSING_FAIL" 'FILE_NOT_FOUND'
stop_bridge_daemon
trap - EXIT
bridge_log 'functional_fs_ops passed'
