#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/helpers/common.sh"

split_allow_fail() {
  local payload="$1"
  local __code_var="$2"
  local __text_var="$3"
  local code
  code="$(printf '%s\n' "$payload" | sed -n '1p')"
  local text
  text="$(printf '%s\n' "$payload" | sed '1d')"
  printf -v "$__code_var" '%s' "$code"
  printf -v "$__text_var" '%s' "$text"
}

DAEMON="$1"
CLI="$2"
RUN_DIR="$3"
WS="$RUN_DIR/ws_functional_cli_contract"
rm -rf "$WS"
mkdir -p "$WS/docs"
printf 'alpha\nbeta token\ngamma\n' > "$WS/docs/sample.txt"
printf 'other file\n' > "$WS/docs/other.txt"
python3 - <<'PY' "$WS/docs/blob.bin" "$RUN_DIR/sample_new.txt"
from pathlib import Path
import sys
Path(sys.argv[1]).write_bytes(b'\x00\x01\x02token\x03')
Path(sys.argv[2]).write_text('alpha\nbeta updated-token\ngamma\n', encoding='utf-8')
PY

setup_bridge_test "$DAEMON" "$CLI" "$RUN_DIR" "$WS"
export AI_BRIDGE_SEARCH_DELAY_MS=2
start_bridge_daemon functional_cli_contract

NOARGS_PAYLOAD="$(invoke_bridge_cli_allow_fail)"
split_allow_fail "$NOARGS_PAYLOAD" NOARGS_CODE NOARGS_TEXT
[[ "$NOARGS_CODE" == "1" ]] || bridge_fail "expected bridge_cli with no args to exit 1"
assert_contains "$NOARGS_TEXT" 'usage: bridge_cli <ping|info|open|resolve|list|stat|read|read-range|write|mkdir|move|copy|rename|search-text|search-regex|session-begin|session-inspect|session-preview|session-commit|session-abort|session-drop-change|session-drop-path|session-recover|session-snapshot|edit-replace-range|edit-replace-block|edit-insert-before|edit-insert-after|edit-delete-block|markdown-replace-section|markdown-insert-after-heading|markdown-upsert-section|json-replace-value|json-upsert-key|json-append-array-item|yaml-replace-value|yaml-upsert-key|yaml-append-item|html-replace-node|html-insert-after-node|html-set-attribute|cancel|patch-preview|patch-apply|patch-rollback|history>'

UNSUPPORTED_PAYLOAD="$(invoke_bridge_cli_allow_fail not-a-command)"
split_allow_fail "$UNSUPPORTED_PAYLOAD" UNSUPPORTED_CODE UNSUPPORTED_TEXT
[[ "$UNSUPPORTED_CODE" == "1" ]] || bridge_fail "expected unsupported command to exit 1"
assert_contains "$UNSUPPORTED_TEXT" 'unsupported command'

VERSION_TEXT="$(invoke_bridge_cli --version)"
assert_contains "$VERSION_TEXT" 'ai_bridge_cli '
assert_contains "$VERSION_TEXT" 'platform='
assert_contains "$VERSION_TEXT" 'transport='

PING_TEXT="$(invoke_bridge_cli ping --workspace "$WS")"
[[ "$PING_TEXT" == 'pong' ]] || bridge_fail "expected ping human output to be pong"

LIST_TEXT="$(invoke_bridge_cli list --workspace "$WS" --path docs)"
assert_contains "$LIST_TEXT" 'docs/sample.txt [file] (normal)'
assert_contains "$LIST_TEXT" 'docs/other.txt [file] (normal)'

READ_TEXT="$(invoke_bridge_cli read --workspace "$WS" --path docs/sample.txt)"
assert_contains "$READ_TEXT" 'alpha'
assert_contains "$READ_TEXT" 'beta token'

SEARCH_TEXT_OUT="$(invoke_bridge_cli search-text --workspace "$WS" --path docs/sample.txt --query token)"
assert_contains "$SEARCH_TEXT_OUT" 'docs/sample.txt:2'
assert_contains "$SEARCH_TEXT_OUT" 'beta token'

STAT_TEXT="$(invoke_bridge_cli stat --workspace "$WS" --path docs/sample.txt)"
MKDIR_TEXT="$(invoke_bridge_cli mkdir --workspace "$WS" --path docs/generated/nested)"
WRITE_TEXT="$(invoke_bridge_cli write --workspace "$WS" --path docs/generated/nested/new.txt --content-file "$RUN_DIR/sample_new.txt")"
MOVE_TEXT="$(invoke_bridge_cli move --workspace "$WS" --path docs/generated/nested/new.txt --target-path docs/generated/moved.txt)"
COPY_TEXT="$(invoke_bridge_cli copy --workspace "$WS" --path docs/generated/moved.txt --target-path docs/generated/copies/copied.txt --create-parents)"
RENAME_TEXT="$(invoke_bridge_cli rename --workspace "$WS" --path docs/generated/moved.txt --target-path docs/generated/renamed.txt)"
assert_contains "$STAT_TEXT" 'path: docs/sample.txt'
assert_contains "$STAT_TEXT" 'kind: file'
assert_contains "$STAT_TEXT" 'encoding: utf-8'
assert_contains "$MKDIR_TEXT" 'path: docs/generated/nested'
assert_contains "$WRITE_TEXT" 'path: docs/generated/nested/new.txt'
assert_contains "$MOVE_TEXT" 'target_path: docs/generated/moved.txt'
assert_contains "$COPY_TEXT" 'target_path: docs/generated/copies/copied.txt'
assert_contains "$RENAME_TEXT" 'target_path: docs/generated/renamed.txt'

RESOLVE_TEXT="$(invoke_bridge_cli resolve --workspace "$WS" --path docs/sample.txt)"
assert_contains "$RESOLVE_TEXT" 'workspace_root: '
assert_contains "$RESOLVE_TEXT" 'relative_path: docs/sample.txt'
assert_contains "$RESOLVE_TEXT" 'policy: normal'

PREVIEW_TEXT="$(invoke_bridge_cli patch-preview --workspace "$WS" --path docs/sample.txt --new-content-file "$RUN_DIR/sample_new.txt")"
assert_contains "$PREVIEW_TEXT" 'preview_id: '
assert_contains "$PREVIEW_TEXT" '--- a/docs/sample.txt'
assert_contains "$PREVIEW_TEXT" '+++ b/docs/sample.txt'

READ_MISSING_PAYLOAD="$(invoke_bridge_cli_allow_fail read --workspace "$WS" --path docs/missing.txt)"
split_allow_fail "$READ_MISSING_PAYLOAD" READ_MISSING_CODE READ_MISSING_TEXT
[[ "$READ_MISSING_CODE" == "1" ]] || bridge_fail "expected missing read to exit 1"
assert_contains "$READ_MISSING_TEXT" 'error [FILE_NOT_FOUND]: path not found'

READ_BINARY_PAYLOAD="$(invoke_bridge_cli_allow_fail read --workspace "$WS" --path docs/blob.bin)"
split_allow_fail "$READ_BINARY_PAYLOAD" READ_BINARY_CODE READ_BINARY_TEXT
[[ "$READ_BINARY_CODE" == "1" ]] || bridge_fail "expected binary read to exit 1"
assert_contains "$READ_BINARY_TEXT" 'error [BINARY_FILE]: binary file'

BAD_REGEX_PAYLOAD="$(invoke_bridge_cli_allow_fail search-regex --workspace "$WS" --path docs/sample.txt --pattern '[')"
split_allow_fail "$BAD_REGEX_PAYLOAD" BAD_REGEX_CODE BAD_REGEX_TEXT
[[ "$BAD_REGEX_CODE" == "1" ]] || bridge_fail "expected invalid regex to exit 1"
assert_contains "$BAD_REGEX_TEXT" 'error [INVALID_PARAMS]:'

MISSING_CONTENT_PAYLOAD="$(invoke_bridge_cli_allow_fail patch-preview --workspace "$WS" --path docs/sample.txt --new-content-file "$RUN_DIR/does-not-exist.txt")"
split_allow_fail "$MISSING_CONTENT_PAYLOAD" MISSING_CONTENT_CODE MISSING_CONTENT_TEXT
[[ "$MISSING_CONTENT_CODE" == "1" ]] || bridge_fail "expected missing new content file to exit 1"
assert_contains "$MISSING_CONTENT_TEXT" 'failed to open file:'

RESOLVE_OUTSIDE_JSON_PAYLOAD="$(invoke_bridge_cli_allow_fail resolve --workspace "$WS" --path ../outside.txt --json)"
split_allow_fail "$RESOLVE_OUTSIDE_JSON_PAYLOAD" RESOLVE_OUTSIDE_CODE RESOLVE_OUTSIDE_TEXT
[[ "$RESOLVE_OUTSIDE_CODE" == "1" ]] || bridge_fail "expected outside resolve to exit 1"
assert_contains "$RESOLVE_OUTSIDE_TEXT" '"code":"PATH_OUTSIDE_WORKSPACE"'
assert_contains "$RESOLVE_OUTSIDE_TEXT" '"message":"path is outside workspace"'

LIST_FILE_JSON_PAYLOAD="$(invoke_bridge_cli_allow_fail list --workspace "$WS" --path docs/sample.txt --json)"
split_allow_fail "$LIST_FILE_JSON_PAYLOAD" LIST_FILE_CODE LIST_FILE_TEXT
[[ "$LIST_FILE_CODE" == "1" ]] || bridge_fail "expected list on file to exit 1"
assert_contains "$LIST_FILE_TEXT" '"code":"INVALID_PARAMS"'
assert_contains "$LIST_FILE_TEXT" '"message":"path is not a directory"'

READ_MISSING_JSON_PAYLOAD="$(invoke_bridge_cli_allow_fail read --workspace "$WS" --path docs/missing.txt --json)"
split_allow_fail "$READ_MISSING_JSON_PAYLOAD" READ_MISSING_JSON_CODE READ_MISSING_JSON_TEXT
[[ "$READ_MISSING_JSON_CODE" == "1" ]] || bridge_fail "expected json missing read to exit 1"
assert_contains "$READ_MISSING_JSON_TEXT" '"code":"FILE_NOT_FOUND"'

READ_BINARY_JSON_PAYLOAD="$(invoke_bridge_cli_allow_fail read --workspace "$WS" --path docs/blob.bin --json)"
split_allow_fail "$READ_BINARY_JSON_PAYLOAD" READ_BINARY_JSON_CODE READ_BINARY_JSON_TEXT
[[ "$READ_BINARY_JSON_CODE" == "1" ]] || bridge_fail "expected json binary read to exit 1"
assert_contains "$READ_BINARY_JSON_TEXT" '"code":"BINARY_FILE"'

BAD_REGEX_JSON_PAYLOAD="$(invoke_bridge_cli_allow_fail search-regex --workspace "$WS" --path docs/sample.txt --pattern '[' --json)"
split_allow_fail "$BAD_REGEX_JSON_PAYLOAD" BAD_REGEX_JSON_CODE BAD_REGEX_JSON_TEXT
[[ "$BAD_REGEX_JSON_CODE" == "1" ]] || bridge_fail "expected json bad regex to exit 1"
assert_contains "$BAD_REGEX_JSON_TEXT" '"code":"INVALID_PARAMS"'

SEARCH_TIMEOUT_JSON_PAYLOAD="$(invoke_bridge_cli_allow_fail search-text --workspace "$WS" --path docs/sample.txt --query missing-token --timeout-ms 5 --json)"
split_allow_fail "$SEARCH_TIMEOUT_JSON_PAYLOAD" SEARCH_TIMEOUT_CODE SEARCH_TIMEOUT_TEXT
[[ "$SEARCH_TIMEOUT_CODE" == "1" ]] || bridge_fail "expected timed out search to exit 1"
assert_contains "$SEARCH_TIMEOUT_TEXT" '"code":"SEARCH_TIMEOUT"'

PREVIEW_JSON="$(invoke_bridge_cli patch-preview --workspace "$WS" --path docs/sample.txt --new-content-file "$RUN_DIR/sample_new.txt" --request-id req-cli-preview --json)"
PREVIEW_ID="$(json_get "$PREVIEW_JSON" 'result.preview_id')"
assert_contains "$PREVIEW_JSON" '"ok":true'
PREVIEW_NOT_FOUND_PAYLOAD="$(invoke_bridge_cli_allow_fail patch-apply --workspace "$WS" --path docs/sample.txt --preview-id preview-missing --json)"
split_allow_fail "$PREVIEW_NOT_FOUND_PAYLOAD" PREVIEW_NOT_FOUND_CODE PREVIEW_NOT_FOUND_TEXT
[[ "$PREVIEW_NOT_FOUND_CODE" == "1" ]] || bridge_fail "expected preview not found apply to exit 1"
assert_contains "$PREVIEW_NOT_FOUND_TEXT" '"code":"PREVIEW_NOT_FOUND"'

PREVIEW_MISMATCH_PAYLOAD="$(invoke_bridge_cli_allow_fail patch-apply --workspace "$WS" --path docs/other.txt --preview-id "$PREVIEW_ID" --json)"
split_allow_fail "$PREVIEW_MISMATCH_PAYLOAD" PREVIEW_MISMATCH_CODE PREVIEW_MISMATCH_TEXT
[[ "$PREVIEW_MISMATCH_CODE" == "1" ]] || bridge_fail "expected preview mismatch apply to exit 1"
assert_contains "$PREVIEW_MISMATCH_TEXT" '"code":"PREVIEW_MISMATCH"'
assert_contains "$PREVIEW_MISMATCH_TEXT" '"message":"preview path mismatch"'

ROLLBACK_MISSING_PAYLOAD="$(invoke_bridge_cli_allow_fail patch-rollback --workspace "$WS" --path docs/sample.txt --backup-id backup-missing --json)"
split_allow_fail "$ROLLBACK_MISSING_PAYLOAD" ROLLBACK_MISSING_CODE ROLLBACK_MISSING_TEXT
[[ "$ROLLBACK_MISSING_CODE" == "1" ]] || bridge_fail "expected rollback missing to exit 1"
assert_contains "$ROLLBACK_MISSING_TEXT" '"code":"ROLLBACK_NOT_FOUND"'
assert_contains "$ROLLBACK_MISSING_TEXT" '"message":"backup not found"'

stop_bridge_daemon
trap - EXIT
bridge_log 'functional_cli_contract passed'
