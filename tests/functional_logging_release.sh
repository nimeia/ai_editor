#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SOURCE_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$SCRIPT_DIR/helpers/common.sh"
DAEMON="$1"
CLI="$2"
RUN_DIR="$3"
BUILD_DIR="$(cd "$RUN_DIR/.." && pwd)"
WS="$RUN_DIR/ws_functional_logging"
INSTALL_DIR="$RUN_DIR/install_functional"
DIST_DIR="$RUN_DIR/dist_functional"
rm -rf "$WS" "$INSTALL_DIR" "$DIST_DIR"
mkdir -p "$WS/docs"
printf 'hello\nlog bridge\n' > "$WS/docs/readme.md"
setup_bridge_test "$DAEMON" "$CLI" "$RUN_DIR" "$WS"
export AI_BRIDGE_LOG_ROTATE_BYTES=120
export AI_BRIDGE_LOG_ROTATE_KEEP=10
start_bridge_daemon functional_logging
INFO_JSON="$(invoke_bridge_cli info --workspace "$WS" --json)"
capture_runtime_dir "$INFO_JSON"
for _ in $(seq 1 8); do
  invoke_bridge_cli stat --workspace "$WS" --path docs/readme.md --json >/dev/null
  invoke_bridge_cli read --workspace "$WS" --path docs/readme.md --json >/dev/null
  invoke_bridge_cli search-text --workspace "$WS" --query bridge --exts .md --json >/dev/null
  invoke_bridge_cli patch-preview --workspace "$WS" --path docs/readme.md --new-content-file "$WS/docs/readme.md" --json >/dev/null
  sleep 0.02
done
[[ -f "$BRIDGE_RUNTIME_DIR/runtime.log" ]] || bridge_fail "runtime.log missing"
[[ -f "$WS/.bridge/audit.log" ]] || bridge_fail "audit.log missing"
grep -q 'method=fs.stat' "$BRIDGE_RUNTIME_DIR"/runtime.log* || bridge_fail 'runtime log missing fs.stat'
grep -q 'method=fs.read' "$BRIDGE_RUNTIME_DIR"/runtime.log* || bridge_fail 'runtime log missing fs.read'
grep -q 'method=search.text' "$BRIDGE_RUNTIME_DIR"/runtime.log* || bridge_fail 'runtime log missing search.text'
grep -q 'duration_ms=' "$BRIDGE_RUNTIME_DIR"/runtime.log* || bridge_fail 'runtime log missing duration_ms'
grep -q 'request_bytes=' "$BRIDGE_RUNTIME_DIR"/runtime.log* || bridge_fail 'runtime log missing request_bytes'
grep -q 'response_bytes=' "$BRIDGE_RUNTIME_DIR"/runtime.log* || bridge_fail 'runtime log missing response_bytes'
grep -q $'fs.stat\tdocs/readme.md\t' "$WS/.bridge"/audit.log* || bridge_fail 'audit log missing fs.stat'
grep -q $'fs.read\tdocs/readme.md\t' "$WS/.bridge"/audit.log* || bridge_fail 'audit log missing fs.read'
grep -q 'patch.preview' "$WS/.bridge"/audit.log* || bridge_fail 'audit log missing patch.preview'
ls "$BRIDGE_RUNTIME_DIR"/runtime.log.* >/dev/null 2>&1 || bridge_fail 'runtime log rotation file missing'
ls "$WS/.bridge"/audit.log.* >/dev/null 2>&1 || bridge_fail 'audit log rotation file missing'
stop_bridge_daemon
cmake --install "$BUILD_DIR" --prefix "$INSTALL_DIR" >/dev/null
[[ -x "$INSTALL_DIR/bin/bridge_daemon" ]] || bridge_fail 'installed bridge_daemon missing'
[[ -x "$INSTALL_DIR/bin/bridge_cli" ]] || bridge_fail 'installed bridge_cli missing'
[[ -f "$INSTALL_DIR/share/ai_bridge/README.md" ]] || bridge_fail 'installed README missing'
[[ -f "$INSTALL_DIR/share/ai_bridge/docs/09-v1-validation-report.md" ]] || bridge_fail 'installed validation report missing'
[[ -f "$INSTALL_DIR/share/ai_bridge/docs/10-v1-release-checklist.md" ]] || bridge_fail 'installed release checklist missing'
mkdir -p "$DIST_DIR"
(
  cd "$SOURCE_ROOT"
  cpack --config "$BUILD_DIR/CPackConfig.cmake" -G TGZ -B "$DIST_DIR" >/dev/null
)
find "$DIST_DIR" -maxdepth 1 -type f \( -name '*.tar.gz' -o -name '*.tgz' \) | sort > "$DIST_DIR/archive_list.txt"
[[ -s "$DIST_DIR/archive_list.txt" ]] || bridge_fail 'release archive missing'
if command -v sha256sum >/dev/null 2>&1; then
  xargs -r sha256sum < "$DIST_DIR/archive_list.txt" > "$DIST_DIR/SHA256SUMS.txt"
elif command -v shasum >/dev/null 2>&1; then
  xargs -r shasum -a 256 < "$DIST_DIR/archive_list.txt" > "$DIST_DIR/SHA256SUMS.txt"
else
  bridge_fail 'no sha256 tool available'
fi
[[ -f "$DIST_DIR/SHA256SUMS.txt" ]] || bridge_fail 'SHA256SUMS.txt missing'
grep -q 'ai_bridge-' "$DIST_DIR/SHA256SUMS.txt" || bridge_fail 'checksum file missing archive entry'
trap - EXIT
bridge_log 'functional_logging_release passed'
