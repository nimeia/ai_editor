#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build"
CONFIG="Release"
OUT_DIR="dist"
GENERATOR="TGZ"
RUN_TESTS=0
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
    --out-dir)
      OUT_DIR="$2"
      shift 2
      ;;
    --generator)
      GENERATOR="$2"
      shift 2
      ;;
    --run-tests)
      RUN_TESTS=1
      shift
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

mkdir -p "$OUT_DIR"
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$CONFIG"
cmake --build "$BUILD_DIR" --parallel "$JOBS"
if [[ "$RUN_TESTS" -eq 1 ]]; then
  ctest --test-dir "$BUILD_DIR" --output-on-failure
fi
cpack --config "$BUILD_DIR/CPackConfig.cmake" -G "$GENERATOR" -B "$OUT_DIR"

checksum_cmd=""
if command -v sha256sum >/dev/null 2>&1; then
  checksum_cmd="sha256sum"
elif command -v shasum >/dev/null 2>&1; then
  checksum_cmd="shasum -a 256"
fi

if [[ -n "$checksum_cmd" ]]; then
  rm -f "$OUT_DIR/SHA256SUMS.txt"
  while IFS= read -r -d '' archive; do
    eval "$checksum_cmd \"$archive\"" >> "$OUT_DIR/SHA256SUMS.txt"
  done < <(find "$OUT_DIR" -maxdepth 1 -type f \( -name '*.tar.gz' -o -name '*.zip' -o -name '*.tgz' \) -print0 | sort -z)
  echo "wrote $OUT_DIR/SHA256SUMS.txt"
fi

echo "release artifacts:"
find "$OUT_DIR" -maxdepth 1 -type f | sort
