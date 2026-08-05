#!/usr/bin/env bash
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -W 2>/dev/null || pwd)"
FIXTURES_DIR="$ROOT_DIR/bench/fixtures"
REPORT_FILE="$ROOT_DIR/bench/report.md"

if [ "${OS:-}" = "Windows_NT" ]; then
  EXE=".exe"
else
  EXE=""
fi

V6_BIN="$ROOT_DIR/build/release/bin/v6$EXE"
JAR_DIR="$ROOT_DIR/build/release/bench-jars"
TMP_DIR="$ROOT_DIR/build/release/bench-tmp"

mkdir -p "$JAR_DIR" "$TMP_DIR"
rm -f "$TMP_DIR"/*.md

if [ ! -f "$V6_BIN" ]; then
  echo "release v6 binary not found at $V6_BIN; run 'make release' first" >&2
  exit 1
fi

bash "$ROOT_DIR/bench/setup-data.sh"

NODE_VERSION="$(node --version 2>/dev/null | sed 's/^v//')"
V6_VERSION="$("$V6_BIN" --version 2>/dev/null | sed -n 's/^v6 \([0-9.]*\).*/\1/p')"

NODE_LABEL="nodejs v$NODE_VERSION"
V6_LABEL="v6 v$V6_VERSION"
V6_AOT_LABEL="v6-aot v$V6_VERSION"

: > "$REPORT_FILE"

for fixture in "$FIXTURES_DIR"/*.js; do
  name="$(basename "$fixture" .js)"
  jar="$JAR_DIR/$name.jar"
  md="$TMP_DIR/$name.md"

  echo "==> Compiling AOT jar for $name"
  "$V6_BIN" "$fixture" -o "$jar" >/dev/null 2>&1

  echo "==> Benchmarking $name"
  hyperfine \
    --warmup 6 \
    --min-runs 5 \
    --ignore-failure \
    --export-markdown "$md" \
    -n "$NODE_LABEL" "node $fixture" \
    -n "$V6_LABEL" "$V6_BIN $fixture" \
    -n "$V6_AOT_LABEL" "java -jar $jar"

  {
    echo "## $name"
    echo
    cat "$md"
    echo
  } >> "$REPORT_FILE"
done

echo "Done. Report written to $REPORT_FILE"
