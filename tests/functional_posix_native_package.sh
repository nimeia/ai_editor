#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$1"
DIST_DIR="$2"

rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"
(
  cd "$REPO_ROOT"
  bash ./scripts/package_release.sh --build-dir "$BUILD_DIR" --out-dir "$DIST_DIR" --jobs 1 >/dev/null
)

archive_count=$(find "$DIST_DIR" -maxdepth 1 -type f \( -name '*.tar.gz' -o -name '*.tgz' \) | wc -l | tr -d ' ')
[[ "$archive_count" -ge 1 ]] || { echo 'expected archive artifact' >&2; exit 1; }

case "$(uname -s)" in
  Linux)
    deb="$(find "$DIST_DIR" -maxdepth 1 -type f -name '*.deb' | head -n 1)"
    [[ -n "$deb" ]] || { echo 'expected deb artifact' >&2; exit 1; }
    listing="$(dpkg-deb -c "$deb")"
    grep -q './usr/local/bin/bridge_daemon' <<<"$listing" || { echo 'deb missing bridge_daemon' >&2; exit 1; }
    grep -q './usr/local/bin/bridge_cli' <<<"$listing" || { echo 'deb missing bridge_cli' >&2; exit 1; }
    grep -q './usr/local/share/ai_bridge/README.md' <<<"$listing" || { echo 'deb missing README' >&2; exit 1; }
    ! grep -q './usr/local/include/' <<<"$listing" || { echo 'deb should not contain development headers' >&2; exit 1; }
    ! grep -q './usr/local/share/ai_bridge/docs/' <<<"$listing" || { echo 'deb should not contain internal docs bundle' >&2; exit 1; }
    ! grep -q './usr/local/share/ai_bridge/scripts/' <<<"$listing" || { echo 'deb should not contain packaging scripts' >&2; exit 1; }
    ;;
  Darwin)
    pkg="$(find "$DIST_DIR" -maxdepth 1 -type f -name '*.pkg' | head -n 1)"
    [[ -n "$pkg" ]] || { echo 'expected pkg artifact' >&2; exit 1; }
    ;;
  *)
    echo 'unsupported platform for native package test' >&2
    exit 1
    ;;
esac
