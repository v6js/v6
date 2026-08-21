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

check_pattern() {
  local label="$1"
  local pattern="$2"
  local actual="$3"
  if echo "$actual" | grep -qE "$pattern"; then
    PASS=$((PASS+1))
  else
    FAIL=$((FAIL+1))
    FAILED_LIST+=("$label")
    echo "--- FAIL: $label ---"
    echo "expected pattern: $pattern"
    echo "actual: $actual"
  fi
}

check_not_pattern() {
  local label="$1"
  local pattern="$2"
  local actual="$3"
  if echo "$actual" | grep -qE "$pattern"; then
    FAIL=$((FAIL+1))
    FAILED_LIST+=("$label")
    echo "--- FAIL: $label ---"
    echo "unexpected pattern present: $pattern"
    echo "actual: $actual"
  else
    PASS=$((PASS+1))
  fi
}

html_dir="test/fix/bundler/html-entry"
html_out="$TMP/html-entry-dist"
"$V6" -b "$html_dir/index.html" --outdir "$html_out" >/dev/null 2>&1
if [ -f "$html_out/index.html" ]; then
  html_body=$(cat "$html_out/index.html")
  check_pattern "html-entry (index.html has module script)" \
    'src="main\.[0-9a-f]{8}\.js" type="module"' "$html_body"
  check_pattern "html-entry (index.html has stylesheet link)" \
    'href="assets/styles\.[0-9a-f]{8}\.css"' "$html_body"
  js_file=$(find "$html_out" -maxdepth 1 -name "main.*.js")
  node_html_out=$(node "$js_file" </dev/null 2>&1 | tr -d '\r')
  check "html-entry (bundled script via node)" "2+3=5" "$node_html_out"
else
  FAIL=$((FAIL+1))
  FAILED_LIST+=("html-entry (dist/index.html not written)")
fi

serve_dir="$TMP/serve-test"
mkdir -p "$serve_dir"
abs_v6="$(pwd)/$V6"
cp test/fix/bundler/basic-cjs/index.js test/fix/bundler/basic-cjs/greet.js "$serve_dir/"
cd "$serve_dir"
"$abs_v6" -b index.js --serve --port 5987 >"$TMP/serve.log" 2>&1 &
serve_pid=$!
cd - >/dev/null
sleep 1.5
serve_page=$(curl -s http://127.0.0.1:5987/ 2>/dev/null)
check_pattern "devserver (index.html has hmr client script)" \
  '__v6_hmr__' "$serve_page"
serve_bundle=$(curl -s http://127.0.0.1:5987/bundle.js 2>/dev/null)
check_pattern "devserver (bundle.js exposes global module registry, not wrapped)" \
  '^var __v6_modules = ' "$serve_bundle"
check_not_pattern "devserver (bundle.js has no CJS/ESM export tail)" \
  'module\.exports = __v6_entry_exports|export default' "$serve_bundle"
kill "$serve_pid" >/dev/null 2>&1 || true
wait "$serve_pid" 2>/dev/null
sleep 0.5

for dir in test/fix/bundler/*/; do
  name=$(basename "$dir")
  entry="${dir}index.js"
  [ -f "$entry" ] || continue

  if [ "$name" == "assets" ]; then
    for fmt in cjs esm iife; do
      ext=js
      [ "$fmt" == "esm" ] && ext=mjs
      out="$TMP/$name.$fmt.$ext"
      "$V6" -b "$entry" --format "$fmt" --outfile "$out" >/dev/null 2>&1
      node_out=$(node "$out" </dev/null 2>&1 | tr -d '\r')
      check_pattern "$name ($fmt via node, css url)" '/assets/styles\.[0-9a-f]{8}\.css' "$node_out"
      check_pattern "$name ($fmt via node, png url)" '/assets/icon\.[0-9a-f]{8}\.png' "$node_out"
    done
    continue
  fi

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
