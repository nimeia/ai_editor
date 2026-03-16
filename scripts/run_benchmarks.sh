#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
source "$ROOT_DIR/tests/helpers/common.sh"
BUILD_DIR="${1:-$ROOT_DIR/build}"
REPORT_DIR="${2:-$ROOT_DIR/benchmark/reports/latest}"
RUN_DIR="${3:-$ROOT_DIR/.benchmark_run}"
DAEMON="${BUILD_DIR}/apps/bridge_daemon/bridge_daemon"
CLI="${BUILD_DIR}/apps/bridge_cli/bridge_cli"
[[ -x "$DAEMON" && -x "$CLI" ]] || { echo "expected built binaries under $BUILD_DIR" >&2; exit 2; }
rm -rf "$RUN_DIR" "$REPORT_DIR"
mkdir -p "$RUN_DIR" "$REPORT_DIR"
WS="$RUN_DIR/ws"
mkdir -p "$WS/docs" "$WS/cfg"
printf 'alpha token
beta token
gamma token
' > "$WS/docs/block.txt"
printf 'line-a
' > "$WS/docs/a.txt"
printf 'line-b
' > "$WS/docs/b.txt"
printf '# Guide

## Existing
Body
' > "$WS/docs/guide.md"
printf '{
  "name": "bench"
}
' > "$WS/cfg/settings.json"
INSERT_FILE="$RUN_DIR/insert.txt"
LINE_A="$RUN_DIR/line_a.txt"
LINE_B="$RUN_DIR/line_b.txt"
MD_CONTENT="$RUN_DIR/md_content.md"
JSON_CONTENT="$RUN_DIR/json_content.json"
BLOCK_REBASED="$RUN_DIR/block_rebased.txt"
printf '
rebased-line' > "$INSERT_FILE"
printf 'line-a-updated' > "$LINE_A"
printf 'line-b-updated' > "$LINE_B"
printf 'bench body
' > "$MD_CONTENT"
printf 'true' > "$JSON_CONTENT"
printf 'HEADER
alpha token
beta token
gamma token
' > "$BLOCK_REBASED"
VARS_JSON="$RUN_DIR/vars.json"
cat > "$VARS_JSON" <<JSON
{
  "workspace": "$WS",
  "insert_file": "$INSERT_FILE",
  "line_a": "$LINE_A",
  "line_b": "$LINE_B",
  "md_content": "$MD_CONTENT",
  "json_content": "$JSON_CONTENT",
  "block_rebased": "$BLOCK_REBASED"
}
JSON
setup_bridge_test "$DAEMON" "$CLI" "$RUN_DIR" "$WS"
start_bridge_daemon daemon_benchmark
python3 "$ROOT_DIR/benchmark/suite.py" --cli "$CLI" --scenario-dir "$ROOT_DIR/benchmark/scenarios" --vars "$VARS_JSON" --thresholds "$ROOT_DIR/benchmark/thresholds.json" --output-json "$REPORT_DIR/benchmark_report.json" --output-md "$REPORT_DIR/benchmark_report.md"
