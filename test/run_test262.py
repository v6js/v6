#!/usr/bin/env python3
import argparse
import concurrent.futures
import json
import os
import platform
import re
import subprocess
import sys
import tempfile
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parent.parent
T262 = ROOT / "test" / "test262"
HARNESS_DIR = T262 / "harness"
TEST_DIR = T262 / "test"
RESULTS_JSON = ROOT / "test" / "test262-results.json"
REPORT_FILE = ROOT / "test" / "coverage.md"

FRONTMATTER_RE = re.compile(r"/\*---(.*?)---\*/", re.DOTALL)

SCORED_CATEGORIES = ["language", "built-ins", "annexB"]
UNSCORED_CATEGORIES = ["intl402", "staging"]
ALL_CATEGORIES = SCORED_CATEGORIES + UNSCORED_CATEGORIES

PRINT_POLYFILL = "function print(s) { console.log(s); }\n"

_harness_cache = {}


def read_harness(name):
  if name not in _harness_cache:
    _harness_cache[name] = (HARNESS_DIR / name).read_text(encoding="utf-8")
  return _harness_cache[name]


def parse_frontmatter(src):
  m = FRONTMATTER_RE.search(src)
  if not m:
    return {}
  try:
    data = yaml.safe_load(m.group(1))
  except yaml.YAMLError:
    return {}
  return data or {}


def variants_for(flags):
  if "module" in flags or "raw" in flags:
    return [None]
  if "onlyStrict" in flags:
    return ["strict"]
  if "noStrict" in flags:
    return ["sloppy"]
  return ["sloppy", "strict"]


def build_source(body, meta, variant):
  flags = set(meta.get("flags") or [])
  if "raw" in flags:
    return body, ".js"

  parts = []
  if variant == "strict":
    parts.append('"use strict";\n')
  if "async" in flags:
    parts.append(PRINT_POLYFILL)
  parts.append(read_harness("assert.js"))
  parts.append(read_harness("sta.js"))
  if "async" in flags:
    parts.append(read_harness("doneprintHandle.js"))
  for inc in meta.get("includes") or []:
    parts.append(read_harness(inc))
  parts.append(body)
  ext = ".mjs" if "module" in flags else ".js"
  return "\n".join(parts), ext


def expected_negative(meta):
  neg = meta.get("negative")
  if not neg:
    return None
  return neg.get("type"), neg.get("phase")


def classify(meta, flags, returncode, output):
  neg = expected_negative(meta)
  if neg:
    err_type, phase = neg
    if returncode == 0:
      return False
    if err_type and err_type in output:
      return True
    if err_type == "SyntaxError" and re.search(r"^error: .+:\d+:", output, re.MULTILINE):
      return True
    return False
  if "async" in flags:
    return returncode == 0 and "Test262:AsyncTestComplete" in output
  return returncode == 0


def run_variant(v6_bin, test_path, meta, variant):
  body = test_path.read_text(encoding="utf-8", errors="replace")
  flags = set(meta.get("flags") or [])
  src, ext = build_source(body, meta, variant)

  fd, tmp_path = tempfile.mkstemp(suffix=ext, dir=str(test_path.parent),
                                   prefix=".v6t262_")
  try:
    with os.fdopen(fd, "w", encoding="utf-8") as f:
      f.write(src)
    try:
      proc = subprocess.run(
          [v6_bin, tmp_path],
          capture_output=True,
          text=True,
          timeout=15,
      )
      output = (proc.stdout or "") + (proc.stderr or "")
      ok = classify(meta, flags, proc.returncode, output)
    except subprocess.TimeoutExpired:
      ok = False
  finally:
    try:
      os.unlink(tmp_path)
    except OSError:
      pass
  return ok


def relative_category(test_path):
  rel = test_path.relative_to(TEST_DIR)
  return rel.parts[0]


def area_key(test_path, depth=2):
  rel = test_path.relative_to(TEST_DIR)
  parts = rel.parts[:-1]
  return "/".join(parts[:depth]) if parts else rel.parts[0]


def discover_tests():
  tests = []
  for cat in ALL_CATEGORIES:
    cat_dir = TEST_DIR / cat
    if not cat_dir.exists():
      continue
    for p in cat_dir.rglob("*.js"):
      if p.name.endswith("_FIXTURE.js"):
        continue
      tests.append(p)
  return tests


def worker(v6_bin, test_path):
  src = test_path.read_text(encoding="utf-8", errors="replace")
  meta = parse_frontmatter(src)
  flags = set(meta.get("flags") or [])
  variants = variants_for(flags)
  results = []
  for variant in variants:
    ok = run_variant(v6_bin, test_path, meta, variant)
    results.append(ok)
  passed = all(results)
  return {
      "path": test_path.relative_to(TEST_DIR).as_posix(),
      "category": relative_category(test_path),
      "area": area_key(test_path),
      "features": meta.get("features") or [],
      "passed": passed,
      "runs": len(results),
  }


def sweep_stray_temp_files():
  for p in TEST_DIR.rglob(".v6t262_*"):
    try:
      p.unlink()
    except OSError:
      pass


def restart_daemon(v6_bin_name):
  if platform.system() == "Windows":
    subprocess.run(["taskkill", "/F", "/IM", v6_bin_name, "/T"],
                    capture_output=True)
  else:
    subprocess.run(["pkill", "-f", v6_bin_name], capture_output=True)


def chunked(seq, size):
  for i in range(0, len(seq), size):
    yield seq[i:i + size]


def run_all(v6_bin, workers, tests, batch_size=1000):
  sweep_stray_temp_files()
  v6_bin_name = Path(v6_bin).name
  results = []
  done = 0
  total = len(tests)
  try:
    for batch in chunked(tests, batch_size):
      restart_daemon(v6_bin_name)
      with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as ex:
        futures = [ex.submit(worker, v6_bin, t) for t in batch]
        for fut in concurrent.futures.as_completed(futures):
          results.append(fut.result())
          done += 1
          if done % 1000 == 0 or done == total:
            print(f"{done}/{total}", file=sys.stderr)
  finally:
    restart_daemon(v6_bin_name)
    sweep_stray_temp_files()
  return results


def pct(passed, total):
  if total == 0:
    return 0.0
  return 100.0 * passed / total


def write_report(results):
  scored = [r for r in results if r["category"] in SCORED_CATEGORIES]
  unscored = [r for r in results if r["category"] not in SCORED_CATEGORIES]

  total_scored = len(scored)
  passed_scored = sum(1 for r in scored if r["passed"])
  overall_pct = pct(passed_scored, total_scored)

  lines = []
  lines.append("# test262 coverage")
  lines.append("")
  lines.append(f"**Overall (language + built-ins + annexB): {overall_pct:.2f}% "
               f"({passed_scored}/{total_scored})**")
  lines.append("")

  lines.append("## By category")
  lines.append("")
  lines.append("| Category | Pass | Total | % | Scored |")
  lines.append("|:---|---:|---:|---:|:---:|")
  for cat in ALL_CATEGORIES:
    rows = [r for r in results if r["category"] == cat]
    if not rows:
      continue
    p = sum(1 for r in rows if r["passed"])
    scored_mark = "yes" if cat in SCORED_CATEGORIES else "no"
    lines.append(f"| {cat} | {p} | {len(rows)} | {pct(p, len(rows)):.2f} | {scored_mark} |")
  lines.append("")

  lines.append("## By area (scored categories only)")
  lines.append("")
  lines.append("| Area | Pass | Total | % |")
  lines.append("|:---|---:|---:|---:|")
  areas = {}
  for r in scored:
    areas.setdefault(r["area"], []).append(r)
  for area in sorted(areas):
    rows = areas[area]
    p = sum(1 for r in rows if r["passed"])
    lines.append(f"| {area} | {p} | {len(rows)} | {pct(p, len(rows)):.2f} |")
  lines.append("")

  lines.append("## By declared feature")
  lines.append("")
  lines.append("| Feature | Pass | Total | % |")
  lines.append("|:---|---:|---:|---:|")
  features = {}
  for r in results:
    for feat in r["features"]:
      features.setdefault(feat, []).append(r)
  for feat in sorted(features):
    rows = features[feat]
    p = sum(1 for r in rows if r["passed"])
    lines.append(f"| {feat} | {p} | {len(rows)} | {pct(p, len(rows)):.2f} |")
  lines.append("")

  lines.append("## Unscored categories (intl402, staging)")
  lines.append("")
  lines.append("| Category | Pass | Total | % |")
  lines.append("|:---|---:|---:|---:|")
  for cat in UNSCORED_CATEGORIES:
    rows = [r for r in unscored if r["category"] == cat]
    if not rows:
      continue
    p = sum(1 for r in rows if r["passed"])
    lines.append(f"| {cat} | {p} | {len(rows)} | {pct(p, len(rows)):.2f} |")
  lines.append("")

  REPORT_FILE.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
  ap = argparse.ArgumentParser()
  ap.add_argument("--v6-bin", required=True)
  ap.add_argument("--workers", type=int, default=os.cpu_count() or 4)
  ap.add_argument("--limit", type=int, default=0, help="limit number of tests (0 = all)")
  ap.add_argument("--filter", default="", help="only run tests whose path contains this substring")
  ap.add_argument("--no-report", action="store_true", help="skip writing test/coverage.md")
  ap.add_argument("--batch-size", type=int, default=1000,
                   help="restart the v6 daemon after this many completed test-runs")
  args = ap.parse_args()

  v6_bin = str(Path(args.v6_bin).resolve())
  tests = discover_tests()
  if args.filter:
    tests = [t for t in tests if args.filter in t.as_posix()]
  if args.limit:
    tests = tests[: args.limit]

  results = run_all(v6_bin, args.workers, tests, batch_size=args.batch_size)

  with open(RESULTS_JSON, "w", encoding="utf-8") as f:
    json.dump(results, f)
  print(f"wrote {RESULTS_JSON}", file=sys.stderr)

  if not args.no_report:
    write_report(results)
    print(f"wrote {REPORT_FILE}", file=sys.stderr)


if __name__ == "__main__":
  main()
