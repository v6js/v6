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

html_serve_dir="$TMP/html-serve-test"
mkdir -p "$html_serve_dir/src"
cp "$html_dir/index.html" "$html_serve_dir/"
cp "$html_dir/src/main.js" "$html_dir/src/math.js" "$html_dir/src/styles.css" "$html_serve_dir/src/"
abs_v6="$(pwd)/$V6"
cd "$html_serve_dir"
"$abs_v6" -b index.html --serve --port 5988 >"$TMP/html-serve.log" 2>&1 &
html_serve_pid=$!
cd - >/dev/null
sleep 1.5
html_serve_page=$(curl -s http://127.0.0.1:5988/ 2>/dev/null)
check_pattern "html-devserver (index.html has hmr client script)" \
  '__v6_hmr__' "$html_serve_page"
check_pattern "html-devserver (script tag is unwrapped, no type=module)" \
  '<script src="main\.[0-9a-f]{8}\.js"></script>' "$html_serve_page"
html_serve_bundle=$(curl -s "http://127.0.0.1:5988/$(echo "$html_serve_page" | grep -oE 'main\.[0-9a-f]{8}\.js')" 2>/dev/null)
check_pattern "html-devserver (script uses bare-global dev format)" \
  '^var __v6_modules = ' "$html_serve_bundle"
kill "$html_serve_pid" >/dev/null 2>&1 || true
wait "$html_serve_pid" 2>/dev/null
sleep 0.5

chunk_dir="test/fix/bundler/html-chunking"
chunk_out="$TMP/html-chunking-dist"
"$V6" -b "$chunk_dir/index.html" --outdir "$chunk_out" >/dev/null 2>&1
shared_count=$(find "$chunk_out" -maxdepth 1 -name "shared.*.js" | wc -l)
check "html-chunking (exactly one shared chunk written)" "1" "$shared_count"
shared_file=$(find "$chunk_out" -maxdepth 1 -name "shared.*.js")
a_file=$(find "$chunk_out" -maxdepth 1 -name "a.*.js")
b_file=$(find "$chunk_out" -maxdepth 1 -name "b.*.js")
check_not_pattern "html-chunking (module ids contain no absolute path)" \
  '[A-Za-z]:[/\\]|^/home|^/Users' "$(cat "$shared_file" "$a_file" "$b_file")"
check_not_pattern "html-chunking (per-script output does not re-emit shared module body)" \
  'function greet' "$(cat "$a_file" "$b_file")"
chunk_page=$(cat "$chunk_out/index.html")
check_pattern "html-chunking (shared chunk script tag injected before others)" \
  'shared\.[0-9a-f]{8}\.js"></script><script src="a\.' "$chunk_page"
shared_base=$(basename "$shared_file")
a_base=$(basename "$a_file")
b_base=$(basename "$b_file")
chunk_node_out=$(cd "$chunk_out" && node -e "
  const vm = require('vm');
  const fs = require('fs');
  const sandbox = {};
  sandbox.globalThis = sandbox;
  const logs = [];
  sandbox.console = { log: (...a) => logs.push(a.join(' ')) };
  vm.createContext(sandbox);
  vm.runInContext(fs.readFileSync('$shared_base', 'utf8'), sandbox);
  vm.runInContext(fs.readFileSync('$a_base', 'utf8'), sandbox);
  vm.runInContext(fs.readFileSync('$b_base', 'utf8'), sandbox);
  console.log(logs.join('|'));
" </dev/null 2>&1 | tr -d '\r')
check "html-chunking (shared module executed once, both scripts see it via node)" \
  "a: hello a|b: hello b" "$chunk_node_out"

abs_v6="$(pwd)/$V6"

define_out="$TMP/ext-define.js"
"$V6" -b test/fix/bundler/ext-define/main.js \
  --define 'process.env.NODE_ENV="production"' --define __DEBUG__=true \
  --outfile "$define_out" --format cjs -q >/dev/null 2>&1
define_node_out=$(node "$define_out" </dev/null 2>&1 | tr -d '\r')
check "ext-define (process.env.NODE_ENV and __DEBUG__ substituted)" \
  "prod mode, debug=true" "$define_node_out"

alias_rel_out="alias_out.js"
(cd test/fix/bundler/ext-alias &&
  MSYS_NO_PATHCONV=1 "$abs_v6" -b main.js --alias @/=src/ \
    --outfile "$alias_rel_out" --format cjs -q >/dev/null 2>&1)
alias_node_out=$(node "test/fix/bundler/ext-alias/$alias_rel_out" </dev/null 2>&1 |
  tr -d '\r')
check "ext-alias (@/ resolves to src/ via node)" "HI!" "$alias_node_out"
rm -f "test/fix/bundler/ext-alias/$alias_rel_out"

banner_out="$TMP/ext-banner.js"
"$V6" -b test/fix/bundler/basic-cjs/index.js --banner "/* my banner */" \
  --outfile "$banner_out" --format cjs -q >/dev/null 2>&1
check "ext-banner (banner text prepended to output)" "1" \
  "$(head -1 "$banner_out" | grep -c "my banner")"

public_out_dir="$TMP/ext-public-dist"
mkdir -p "$public_out_dir"
"$V6" -b test/fix/bundler/ext-public/main.js \
  --public-dir test/fix/bundler/ext-public/public \
  --outfile "$public_out_dir/bundle.js" --format cjs -q >/dev/null 2>&1
check "ext-public (public dir contents copied into outdir)" "1" \
  "$([ -f "$public_out_dir/favicon.txt" ] && echo 1 || echo 0)"

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
