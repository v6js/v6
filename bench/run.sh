#!/usr/bin/env bash
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -W)"
FIXTURES_DIR="$ROOT_DIR/bench/fixtures"
DATA_DIR="$ROOT_DIR/bench/data"

if [ "${OS:-}" = "Windows_NT" ]; then
  EXE=".exe"
else
  EXE=""
fi

V6_BIN="$ROOT_DIR/build/release/bin/v6$EXE"
JAR_DIR="$ROOT_DIR/build/release/bench-jars"

mkdir -p "$DATA_DIR" "$JAR_DIR"

if [ ! -f "$V6_BIN" ]; then
  echo "release v6 binary not found at $V6_BIN; run 'make release' first" >&2
  exit 1
fi

for fixture in "$FIXTURES_DIR"/*.js; do
  name="$(basename "$fixture" .js)"
  jar="$JAR_DIR/$name.jar"

  echo "==> Compiling AOT jar for $name"
  "$V6_BIN" "$fixture" -o "$jar" >/dev/null

  echo "==> Benchmarking $name"
  hyperfine \
    --warmup 1 \
    --min-runs 3 \
    --export-markdown "$DATA_DIR/$name.md" \
    -n "nodejs" "node \"$fixture\"" \
    -n "v6" "$V6_BIN $fixture" \
    -n "v6-aot" "java -jar \"$jar\""
done

echo "Done. Results in $DATA_DIR"
