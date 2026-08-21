#!/usr/bin/env bash
set -u
V6=build/debug/bin/v6.exe
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
PASS=0
FAIL=0
FAILED_LIST=()

check() {
  local label="$1"
  local expected="$2"
  local actual="$3"
  if [ "$expected" == "$actual" ]; then
    PASS=$((PASS+1))
  else
    FAIL=$((FAIL+1))
    FAILED_LIST+=("$label")
    echo "--- FAIL: $label ---"
    echo "expected: $expected"
    echo "actual:   $actual"
  fi
}

for dir in test/fix/bundler/*/; do
  name=$(basename "$dir")
  entry="${dir}index.js"
  [ -f "$entry" ] || continue

  expected=$("$V6" "$entry" </dev/null 2>&1 | tr -d '\r')

  cjs_out="$TMP/$name.cjs.js"
  "$V6" -b "$entry" --format cjs --outfile "$cjs_out" >/dev/null 2>&1
  node_cjs=$(node "$cjs_out" </dev/null 2>&1 | tr -d '\r')
  check "$name (cjs via node)" "$expected" "$node_cjs"
  v6_cjs=$("$V6" "$cjs_out" </dev/null 2>&1 | tr -d '\r')
  check "$name (cjs via v6)" "$expected" "$v6_cjs"

  esm_out="$TMP/$name.esm.mjs"
  "$V6" -b "$entry" --format esm --outfile "$esm_out" >/dev/null 2>&1
  node_esm=$(node "$esm_out" </dev/null 2>&1 | tr -d '\r')
  check "$name (esm via node)" "$expected" "$node_esm"

  iife_out="$TMP/$name.iife.js"
  "$V6" -b "$entry" --format iife --outfile "$iife_out" >/dev/null 2>&1
  node_iife=$(node "$iife_out" </dev/null 2>&1 | tr -d '\r')
  check "$name (iife via node)" "$expected" "$node_iife"
  v6_iife=$("$V6" "$iife_out" </dev/null 2>&1 | tr -d '\r')
  check "$name (iife via v6)" "$expected" "$v6_iife"
done

echo "PASS=$PASS FAIL=$FAIL"
if [ "$FAIL" -gt 0 ]; then
  printf '%s\n' "${FAILED_LIST[@]}"
  exit 1
fi
