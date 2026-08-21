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

check_js_fixture() {
  local name="$1"
  shift
  local dir="test/fix/optimizer/$name"
  local pre="$dir/pre.js"
  local post="$dir/post.js"
  [ -f "$pre" ] || { FAIL=$((FAIL+1)); FAILED_LIST+=("$name (missing pre.js)"); return; }
  [ -f "$post" ] || { FAIL=$((FAIL+1)); FAILED_LIST+=("$name (missing post.js)"); return; }

  local out="$TMP/$name.out.js"
  "$V6" --optimize "$pre" --outfile "$out" -q "$@" >"$TMP/$name.stderr" 2>&1
  local rc=$?
  if [ $rc -ne 0 ]; then
    FAIL=$((FAIL+1))
    FAILED_LIST+=("$name (optimize failed, rc=$rc)")
    cat "$TMP/$name.stderr"
    return
  fi

  local actual expected
  actual=$(cat "$out" | tr -d '\r')
  expected=$(cat "$post" | tr -d '\r')
  check "$name (matches post.js)" "$expected" "$actual"

  local pre_run post_run
  pre_run=$("$V6" "$pre" </dev/null 2>&1 | tr -d '\r')
  post_run=$("$V6" "$out" </dev/null 2>&1 | tr -d '\r')
  check "$name (pre.js and optimized output behave identically via node/v6)" \
    "$pre_run" "$post_run"
}

check_js_fixture print-roundtrip
check_js_fixture const-fold --opt-const-fold
check_js_fixture const-prop --opt-const-prop
check_js_fixture algebraic-simplify --opt-algebraic-simplify
check_js_fixture dead-code --opt-dead-code
check_js_fixture control-flow-simplify --opt-control-flow
check_js_fixture dead-store --opt-dead-store
check_js_fixture obfuscation --opt-obfuscation
check_js_fixture switch-fold --opt-dead-code
check_js_fixture seq-bool -Oz
check_js_fixture computed-access --opt-algebraic-simplify
check_js_fixture literal-access --opt-const-fold
check_js_fixture math-fold --opt-const-fold
check_js_fixture math-shadow-safety --opt-const-fold
check_js_fixture string-fold --opt-const-fold
check_js_fixture global-wrapper-fold --opt-const-fold
check_js_fixture global-wrapper-shadow-safety --opt-const-fold

for dir in test/fix/optimizer/*/; do
  name=$(basename "$dir")
  [ "$name" == "print-roundtrip" ] && continue
  [ -f "$dir/pre.css" ] || [ -f "$dir/pre.json" ] || continue

  if [ -f "$dir/pre.css" ]; then
    out="$TMP/$name.out.css"
    "$V6" --optimize "$dir/pre.css" --outfile "$out" -q --opt-no-whitespace \
      --opt-no-comments >/dev/null 2>&1
    actual=$(cat "$out" | tr -d '\r')
    expected=$(cat "$dir/post.css" | tr -d '\r')
    check "$name (css matches post.css)" "$expected" "$actual"

    ws_only=$("$V6" --optimize "$dir/pre.css" -q --opt-no-whitespace)
    check "$name (--opt-no-whitespace alone keeps comments)" "1" \
      "$(echo "$ws_only" | grep -c '/\*')"

    comments_only=$("$V6" --optimize "$dir/pre.css" -q --opt-no-comments)
    check "$name (--opt-no-comments alone removes the comment)" "0" \
      "$(echo "$comments_only" | grep -c '/\*')"
    check "$name (--opt-no-comments alone preserves original line count)" \
      "$(wc -l < "$dir/pre.css")" "$(echo "$comments_only" | wc -l)"
  fi

  if [ -f "$dir/pre.json" ]; then
    out="$TMP/$name.out.json"
    "$V6" --optimize "$dir/pre.json" --outfile "$out" -q --opt-no-whitespace >/dev/null 2>&1
    actual=$(cat "$out" | tr -d '\r')
    expected=$(cat "$dir/post.json" | tr -d '\r')
    check "$name (json matches post.json)" "$expected" "$actual"

    unchanged=$("$V6" --optimize "$dir/pre.json" -q | tr -d '\r')
    check "$name (json unchanged with no flags)" \
      "$(cat "$dir/pre.json" | tr -d '\r')" "$unchanged"
  fi
done

echo "PASS=$PASS FAIL=$FAIL"
if [ "$FAIL" -gt 0 ]; then
  printf '%s\n' "${FAILED_LIST[@]}"
  exit 1
fi
