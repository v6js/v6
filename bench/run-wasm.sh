#!/usr/bin/env bash
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -W 2>/dev/null || pwd)"
WASM_DIR="$ROOT_DIR/bench/wasm"
REPORT_FILE="$ROOT_DIR/bench/report-wasm.md"

if [ "${OS:-}" = "Windows_NT" ]; then
  EXE=".exe"
else
  EXE=""
fi

V6_BIN="$ROOT_DIR/build/release/bin/v6$EXE"
TMP_DIR="$ROOT_DIR/build/release/bench-wasm-tmp"

mkdir -p "$TMP_DIR"
rm -f "$TMP_DIR"/*.md

if [ ! -f "$V6_BIN" ]; then
  echo "release v6 binary not found at $V6_BIN; run 'make BUILD_TYPE=release all' first" >&2
  exit 1
fi

if ! command -v wasmtime >/dev/null 2>&1; then
  echo "wasmtime not found on PATH" >&2
  exit 1
fi

NODE_VERSION="$(node --version 2>/dev/null | sed 's/^v//')"
V6_VERSION="$("$V6_BIN" --version 2>/dev/null | sed -n 's/^v6 \([0-9.]*\).*/\1/p')"
WASMTIME_VERSION="$(wasmtime --version 2>/dev/null | sed -n 's/^wasmtime \([0-9.]*\).*/\1/p')"

NODE_LABEL="nodejs v$NODE_VERSION"
V6_LABEL="v6 v$V6_VERSION"
WASMTIME_LABEL="wasmtime v$WASMTIME_VERSION"

cat > "$REPORT_FILE" <<EOF
# WASM/WASI benchmarks

Release build (\`make BUILD_TYPE=release\`), no AOT. \`cli/*\` benchmarks run
the \`.wasm\` file directly (\`wasmtime run file.wasm\` vs \`v6 file.wasm\`, WASI
fully enabled by default on both, v6 routed through its persistent daemon
like any other CLI invocation). \`js/*\` benchmarks load the same \`.wasm\` via
the \`WebAssembly\` API from a JS driver (\`node file.js\` vs \`v6 file.js\`, same
daemon-accelerated invocation as \`bench/report.md\`). hyperfine, 6 warmups,
5+ min runs per comparison, run with nothing else competing for CPU.

EOF

echo "==> CLI WASI benchmarks: wasmtime vs v6 (v6 file.wasm)"
for name in fib primes wasi_io memcpy_bulk table_dispatch dotproduct dotproduct_simd; do
  wasm="$WASM_DIR/$name.wasm"
  md="$TMP_DIR/cli-$name.md"

  echo "==> Benchmarking cli/$name"
  hyperfine \
    --shell=none \
    --warmup 6 \
    --min-runs 5 \
    --ignore-failure \
    --export-markdown "$md" \
    -n "$WASMTIME_LABEL" "wasmtime run $wasm" \
    -n "$V6_LABEL" "$V6_BIN $wasm"

  {
    echo "## cli/$name.wasm"
    echo
    cat "$md"
    echo
  } >> "$REPORT_FILE"
done

echo "==> JS WebAssembly-API benchmarks: node vs v6"
for name in fib primes memcpy_bulk table_dispatch dotproduct dotproduct_simd; do
  fixture="$WASM_DIR/$name.js"
  md="$TMP_DIR/js-$name.md"

  echo "==> Benchmarking js/$name"
  hyperfine \
    --shell=none \
    --warmup 6 \
    --min-runs 5 \
    --ignore-failure \
    --export-markdown "$md" \
    -n "$NODE_LABEL" "node $fixture" \
    -n "$V6_LABEL" "$V6_BIN $fixture"

  {
    echo "## js/$name.js"
    echo
    cat "$md"
    echo
  } >> "$REPORT_FILE"
done

echo "Done. Report written to $REPORT_FILE"
