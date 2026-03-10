#!/usr/bin/env bash
set -euo pipefail

BRIDGE_HELPER_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BRIDGE_JSON_ASSERT_PY="$BRIDGE_HELPER_DIR/json_assert.py"
BRIDGE_DAEMON_PID=""
BRIDGE_DAEMON_LOG=""
BRIDGE_RUNTIME_DIR=""

bridge_log() {
  printf '[bridge-test] %s\n' "$*"
}

bridge_fail() {
  local msg="$1"
  printf '[bridge-test] FAIL: %s\n' "$msg" >&2
  bridge_dump_diagnostics >&2 || true
  exit 1
}

bridge_dump_diagnostics() {
  if [[ -n "${BRIDGE_DAEMON_LOG:-}" && -f "$BRIDGE_DAEMON_LOG" ]]; then
    echo '--- daemon log ---'
    cat "$BRIDGE_DAEMON_LOG"
  fi
  if [[ -n "${BRIDGE_RUNTIME_DIR:-}" && -f "$BRIDGE_RUNTIME_DIR/runtime.log" ]]; then
    echo '--- runtime.log ---'
    cat "$BRIDGE_RUNTIME_DIR/runtime.log"
  fi
  if [[ -n "${BRIDGE_WORKSPACE:-}" && -f "$BRIDGE_WORKSPACE/.bridge/audit.log" ]]; then
    echo '--- audit.log ---'
    cat "$BRIDGE_WORKSPACE/.bridge/audit.log"
  fi
  if [[ -n "${BRIDGE_WORKSPACE:-}" && -f "$BRIDGE_WORKSPACE/.bridge/history.log" ]]; then
    echo '--- history.log ---'
    cat "$BRIDGE_WORKSPACE/.bridge/history.log"
  fi
}

json_get() {
  local json="$1"
  local path="$2"
  python3 "$BRIDGE_JSON_ASSERT_PY" get "$path" <<<"$json"
}

assert_json_eq() {
  local json="$1"; local path="$2"; local expected="$3"
  python3 "$BRIDGE_JSON_ASSERT_PY" eq "$path" "$expected" <<<"$json" || bridge_fail "expected $path == $expected"
}

assert_json_contains() {
  local json="$1"; local path="$2"; local needle="$3"
  python3 "$BRIDGE_JSON_ASSERT_PY" contains "$path" "$needle" <<<"$json" || bridge_fail "expected $path to contain $needle"
}

assert_json_truthy() {
  local json="$1"; local path="$2"
  python3 "$BRIDGE_JSON_ASSERT_PY" truthy "$path" <<<"$json" || bridge_fail "expected $path to be truthy"
}

assert_json_falsey() {
  local json="$1"; local path="$2"
  python3 "$BRIDGE_JSON_ASSERT_PY" falsey "$path" <<<"$json" || bridge_fail "expected $path to be falsey"
}

assert_json_len_ge() {
  local json="$1"; local path="$2"; local minimum="$3"
  python3 "$BRIDGE_JSON_ASSERT_PY" len_ge "$path" "$minimum" <<<"$json" || bridge_fail "expected len($path) >= $minimum"
}

assert_contains() {
  local text="$1"; local needle="$2"
  [[ "$text" == *"$needle"* ]] || bridge_fail "expected text to contain: $needle"
}

assert_not_contains() {
  local text="$1"; local needle="$2"
  [[ "$text" != *"$needle"* ]] || bridge_fail "expected text to not contain: $needle"
}

invoke_bridge_cli() {
  local out
  if ! out="$($BRIDGE_CLI "$@")"; then
    bridge_fail "bridge_cli failed: $*"
  fi
  printf '%s' "$out"
}

invoke_bridge_cli_allow_fail() {
  local out
  set +e
  out="$($BRIDGE_CLI "$@" 2>&1)"
  local code=$?
  set -e
  printf '%s\n%s' "$code" "$out"
}

start_bridge_daemon() {
  BRIDGE_DAEMON_LOG="$BRIDGE_RUN_DIR/${1:-daemon}.log"
  shift || true
  bridge_log "starting daemon for workspace: $BRIDGE_WORKSPACE"
  "$BRIDGE_DAEMON" --workspace "$BRIDGE_WORKSPACE" "$@" >"$BRIDGE_DAEMON_LOG" 2>&1 &
  BRIDGE_DAEMON_PID=$!
  wait_for_bridge_ready
}

wait_for_bridge_ready() {
  local attempts=30
  local out=''
  for ((i=1; i<=attempts; ++i)); do
    sleep 0.2
    set +e
    out="$($BRIDGE_CLI ping --workspace "$BRIDGE_WORKSPACE" --json 2>/dev/null)"
    local code=$?
    set -e
    if [[ $code -eq 0 && "$out" == *'"ok":true'* ]]; then
      return 0
    fi
  done
  bridge_fail "daemon did not become ready"
}

stop_bridge_daemon() {
  if [[ -n "${BRIDGE_DAEMON_PID:-}" ]]; then
    kill "$BRIDGE_DAEMON_PID" >/dev/null 2>&1 || true
    for _ in $(seq 1 20); do
      if ! kill -0 "$BRIDGE_DAEMON_PID" >/dev/null 2>&1; then
        break
      fi
      sleep 0.1
    done
    if kill -0 "$BRIDGE_DAEMON_PID" >/dev/null 2>&1; then
      kill -9 "$BRIDGE_DAEMON_PID" >/dev/null 2>&1 || true
    fi
    wait "$BRIDGE_DAEMON_PID" >/dev/null 2>&1 || true
    BRIDGE_DAEMON_PID=""
  fi
}

capture_runtime_dir() {
  local info_json="$1"
  BRIDGE_RUNTIME_DIR="$(json_get "$info_json" 'result.runtime_dir')"
}

setup_bridge_test() {
  BRIDGE_DAEMON="$1"
  BRIDGE_CLI="$2"
  BRIDGE_RUN_DIR="$3"
  BRIDGE_WORKSPACE="$4"
  mkdir -p "$BRIDGE_RUN_DIR"
  trap stop_bridge_daemon EXIT
}
