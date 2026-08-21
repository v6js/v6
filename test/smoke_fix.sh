#!/usr/bin/env bash
set -u
V6=build/debug/bin/v6.exe
PASS=0
FAIL=0
FAILED_LIST=()
while IFS= read -r f; do
  node_out=$(node "$f" </dev/null 2>&1 | tr -d '\r')
  v6_out=$("$V6" "$f" </dev/null 2>&1 | tr -d '\r')
  if [ "$node_out" == "$v6_out" ]; then
    PASS=$((PASS+1))
  else
    FAIL=$((FAIL+1))
    FAILED_LIST+=("$f")
  fi
done < <(find test/fix -name "*.js" \
  ! -path "*/cjs/circ_a.js" ! -path "*/cjs/circ_b.js" ! -path "*/cjs/greet.js" \
  ! -path "*/cjs/increment.js" ! -path "*/cjs/math.js" ! -path "*node_modules*" \
  ! -path "*/esm/circ_a.js" ! -path "*/esm/circ_b.js" ! -path "*/esm/math_utils.js" \
  ! -path "*/esm/side_effect.js" ! -path "*/wasm/cjs_import.js" ! -path "*/bundler/*")
echo "PASS=$PASS FAIL=$FAIL"
if [ "$FAIL" -gt 0 ]; then
  printf '%s\n' "${FAILED_LIST[@]}"
fi
