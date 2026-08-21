#include "v6/cli.h"
#include "v6/bundle_build.h"
#include "v6/bundle_devserver.h"
#include "v6/bundle_html.h"
#include "v6/module.h"

#include <stdio.h>
#include <string.h>

static int parse_format(const char* s, bundle_format* out) {
  if (!s || strcmp(s, "esm") == 0) {
    *out = bundle_fmt_esm;
    return 0;
  }
  if (strcmp(s, "cjs") == 0) {
    *out = bundle_fmt_cjs;
    return 0;
  }
  if (strcmp(s, "iife") == 0) {
    *out = bundle_fmt_iife;
    return 0;
  }
  return -1;
}

static const char* pick_default_entry(void) {
  static const char* candidates[] = {
      "index.html", "index.js", "src/index.js", "index.mjs", "src/index.mjs",
      NULL,
  };
  for (int i = 0; candidates[i]; i++) {
    if (path_is_regular_file(candidates[i]))
      return candidates[i];
  }
  return NULL;
}

static int has_suffix(const char* s, const char* suffix) {
  size_t ls = strlen(s), lsuf = strlen(suffix);
  if (lsuf > ls)
    return 0;
  return strcmp(s + ls - lsuf, suffix) == 0;
}

int v6_cli_run_bundle(v6_cli_options* opts) {
  const char* entry = opts->bundle_entry;
  if (!entry)
    entry = pick_default_entry();
  if (!entry) {
    fprintf(stderr, "error: no entry file given and none of index.js, "
                    "src/index.js, index.mjs, src/index.mjs exist\n");
    return 1;
  }

  int is_html = has_suffix(entry, ".html") || has_suffix(entry, ".htm");

  if (opts->bundle_serve && is_html) {
    fprintf(stderr, "error: --serve is not implemented yet for html entries\n");
    return 1;
  }
  if (opts->bundle_watch && is_html) {
    fprintf(stderr, "error: --watch is not implemented yet for html entries\n");
    return 1;
  }

  if (is_html) {
    const char* outdir = opts->bundle_outdir ? opts->bundle_outdir : "dist";
    return bundle_process_html(entry, outdir, opts->bundle_global_name);
  }

  bundle_format fmt;
  if (parse_format(opts->bundle_format, &fmt) != 0) {
    fprintf(stderr,
            "error: unknown --format \"%s\" (expected esm, cjs, or iife)\n",
            opts->bundle_format);
    return 1;
  }

  const char* outfile =
      opts->bundle_outfile ? opts->bundle_outfile : "dist/bundle.js";

  if (opts->bundle_serve) {
    int port = opts->bundle_serve_port > 0 ? opts->bundle_serve_port : 5173;
    return bundle_devserver_run(entry, opts, outfile, port);
  }

  if (opts->bundle_watch) {
    return bundle_run_watch_loop(entry, fmt, opts->bundle_global_name, outfile,
                                 NULL, NULL);
  }

  return bundle_build_js_once(entry, fmt, opts->bundle_global_name, outfile,
                              NULL, NULL);
}
