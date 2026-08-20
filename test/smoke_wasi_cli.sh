#!/usr/bin/env bash
set -u
V6=build/debug/bin/v6.exe
PASS=0
FAIL=0
FAILED_LIST=()

check() {
  local name="$1"
  local expected="$2"
  local actual="$3"
  if [ "$actual" == "$expected" ]; then
    PASS=$((PASS+1))
  else
    FAIL=$((FAIL+1))
    FAILED_LIST+=("$name")
  fi
}

out=$("$V6" test/fix/wasm/cli/noargs.wasm </dev/null 2>&1)
check "cli/noargs.wasm" "42" "$out"

out=$("$V6" test/fix/wasm/cli/wasi_hello.wasm </dev/null 2>&1)
check "cli/wasi_hello.wasm" "hello from wasi" "$out"

out=$("$V6" test/fix/wasm/cli/wasi_random.wasm </dev/null 2>&1)
check "cli/wasi_random.wasm (allowed)" "got random" "$out"

out=$("$V6" --no-wasi-random test/fix/wasm/cli/wasi_random.wasm </dev/null 2>&1)
check "cli/wasi_random.wasm (--no-wasi-random)" \
  "error: LinkError: import 'wasi_snapshot_preview1.random_get' not found" "$out"

echo "PASS=$PASS FAIL=$FAIL"
if [ "$FAIL" -gt 0 ]; then
  printf '%s\n' "${FAILED_LIST[@]}"
  exit 1
fi
