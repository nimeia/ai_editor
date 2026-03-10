#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build"
CONFIG="Release"
WORK_DIR=".p6_validation_workspace"
INSTALL_DIR=".p6_validation_install"
DIST_DIR=".p6_validation_dist"
JOBS=1

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --config)
      CONFIG="$2"
      shift 2
      ;;
    --workspace-dir)
      WORK_DIR="$2"
      shift 2
      ;;
    --install-dir)
      INSTALL_DIR="$2"
      shift 2
      ;;
    --dist-dir)
      DIST_DIR="$2"
      shift 2
      ;;
    --jobs)
      JOBS="$2"
      shift 2
      ;;
    *)
      echo "unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$CONFIG"
cmake --build "$BUILD_DIR" --parallel "$JOBS"
ctest --test-dir "$BUILD_DIR" --output-on-failure -E "test_integration_(patch_stream|timeout)_ops"
/usr/bin/bash "$(pwd)/tests/integration_patch_stream_ops.sh" "$(pwd)/$BUILD_DIR/apps/bridge_daemon/bridge_daemon" "$(pwd)/$BUILD_DIR/apps/bridge_cli/bridge_cli" "$(pwd)/$BUILD_DIR/tests"
/usr/bin/bash "$(pwd)/tests/integration_timeout_ops.sh" "$(pwd)/$BUILD_DIR/apps/bridge_daemon/bridge_daemon" "$(pwd)/$BUILD_DIR/apps/bridge_cli/bridge_cli" "$(pwd)/$BUILD_DIR/tests"

rm -rf "$WORK_DIR" "$INSTALL_DIR" "$DIST_DIR"
mkdir -p "$WORK_DIR/docs"
cat > "$WORK_DIR/README.md" <<'TXT'
ai_bridge validation workspace
TXT
cat > "$WORK_DIR/docs/sample.txt" <<'TXT'
alpha bridge beta
gamma delta
TXT
cat > "$WORK_DIR/docs/new.txt" <<'TXT'
alpha bridge beta
gamma delta
epsilon zeta
TXT

DAEMON="$BUILD_DIR/apps/bridge_daemon/bridge_daemon"
CLI="$BUILD_DIR/apps/bridge_cli/bridge_cli"
DAEMON_LOG="$WORK_DIR/daemon.out"

"$DAEMON" --workspace "$WORK_DIR" >"$DAEMON_LOG" 2>&1 &
DAEMON_PID=$!
trap 'kill "$DAEMON_PID" >/dev/null 2>&1 || true' EXIT
sleep 1

"$CLI" ping --workspace "$WORK_DIR" >/dev/null
"$CLI" info --workspace "$WORK_DIR" --json > "$WORK_DIR/info.json"
python3 - <<'PY' "$WORK_DIR/info.json"
import json, pathlib, sys
result = json.loads(pathlib.Path(sys.argv[1]).read_text())["result"]
assert result["platform"] == "posix", result
assert result["transport"] == "posix-unix-socket", result
assert result["runtime_dir"], result
PY

"$CLI" open --workspace "$WORK_DIR" >/dev/null
"$CLI" list --workspace "$WORK_DIR" >/dev/null
"$CLI" stat --workspace "$WORK_DIR" --path docs/sample.txt >/dev/null
"$CLI" read --workspace "$WORK_DIR" --path docs/sample.txt >/dev/null
"$CLI" read-range --workspace "$WORK_DIR" --path docs/sample.txt --start 1 --end 1 >/dev/null
"$CLI" search-text --workspace "$WORK_DIR" --query bridge --exts .txt >/dev/null
"$CLI" search-regex --workspace "$WORK_DIR" --pattern 'alpha.*beta' --exts .txt >/dev/null
"$CLI" read --workspace "$WORK_DIR" --path docs/sample.txt --stream >/dev/null

"$CLI" patch-preview --workspace "$WORK_DIR" --path docs/sample.txt --new-content-file "$WORK_DIR/docs/new.txt" --json > "$WORK_DIR/preview.json"
PREVIEW_ID=$(python3 - <<'PY' "$WORK_DIR/preview.json"
import json, pathlib, sys
print(json.loads(pathlib.Path(sys.argv[1]).read_text())["result"]["preview_id"])
PY
)
"$CLI" patch-apply --workspace "$WORK_DIR" --preview-id "$PREVIEW_ID" --json > "$WORK_DIR/apply.json"
"$CLI" history --workspace "$WORK_DIR" --path docs/sample.txt >/dev/null
BACKUP_ID=$(python3 - <<'PY' "$WORK_DIR/apply.json"
import json, pathlib, sys
print(json.loads(pathlib.Path(sys.argv[1]).read_text())["result"]["backup_id"])
PY
)
"$CLI" patch-rollback --workspace "$WORK_DIR" --path docs/sample.txt --backup-id "$BACKUP_ID" >/dev/null

kill "$DAEMON_PID" >/dev/null 2>&1 || true
wait "$DAEMON_PID" 2>/dev/null || true
trap - EXIT

test -f "$WORK_DIR/.bridge/audit.log"
test -f "$WORK_DIR/.bridge/history.log"

cmake --install "$BUILD_DIR" --prefix "$INSTALL_DIR"
test -x "$INSTALL_DIR/bin/bridge_daemon"
test -x "$INSTALL_DIR/bin/bridge_cli"
test -f "$INSTALL_DIR/share/ai_bridge/README.md"
test -f "$INSTALL_DIR/share/ai_bridge/docs/09-v1-validation-report.md"
test -f "$INSTALL_DIR/share/ai_bridge/docs/10-v1-release-checklist.md"

bash ./scripts/package_release.sh --build-dir "$BUILD_DIR" --config "$CONFIG" --out-dir "$DIST_DIR" --generator TGZ --jobs "$JOBS"
test -f "$DIST_DIR/SHA256SUMS.txt"
find "$DIST_DIR" -maxdepth 1 -type f \( -name '*.tar.gz' -o -name '*.tgz' \) | grep -q .

echo "P6 validation complete"
