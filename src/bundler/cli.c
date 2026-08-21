#include "v6/cli.h"
#include "v6/bundler_build.h"
#include "v6/bundler_devserver.h"
#include "v6/bundler_html.h"
#include "v6/bundler_report.h"
#include "v6/module.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_format(const char* s, v6_bundler_format* out) {
  if (!s || strcmp(s, "esm") == 0) {
    *out = v6_bundler_fmt_esm;
    return 0;
  }
  if (strcmp(s, "cjs") == 0) {
    *out = v6_bundler_fmt_cjs;
    return 0;
  }
  if (strcmp(s, "iife") == 0) {
    *out = v6_bundler_fmt_iife;
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

static int parse_limits(v6_cli_options* opts, v6_bundler_limits* out) {
  out->max_size = 0;
  out->max_deps = 0;
  out->mode = v6_bundler_limit_warn;

  if (opts->bundle_size_limit) {
    if (strcmp(opts->bundle_size_limit, "warn") == 0) {
      out->mode = v6_bundler_limit_warn;
    } else if (strcmp(opts->bundle_size_limit, "error") == 0) {
      out->mode = v6_bundler_limit_error;
    } else {
      fprintf(stderr,
             "error: unknown --size-limit \"%s\" (expected warn or error)\n",
             opts->bundle_size_limit);
      return -1;
    }
  }

  if (opts->bundle_max_size) {
    if (v6_bundler_parse_size(opts->bundle_max_size, &out->max_size) != 0) {
      fprintf(stderr, "error: invalid --max-size \"%s\"\n", opts->bundle_max_size);
      return -1;
    }
  }

  if (opts->bundle_max_deps) {
    char* end;
    long long v = strtoll(opts->bundle_max_deps, &end, 10);
    if (end == opts->bundle_max_deps || v <= 0) {
      fprintf(stderr, "error: invalid --max-deps \"%s\"\n", opts->bundle_max_deps);
      return -1;
    }
    out->max_deps = v;
  }

  return 0;
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
    return v6_bundler_process_html(entry, outdir, opts->bundle_global_name);
  }

  v6_bundler_format fmt;
  if (parse_format(opts->bundle_format, &fmt) != 0) {
    fprintf(stderr,
            "error: unknown --format \"%s\" (expected esm, cjs, or iife)\n",
            opts->bundle_format);
    return 1;
  }

  const char* outfile =
      opts->bundle_outfile ? opts->bundle_outfile : "dist/bundle.js";

  v6_bundler_limits limits;
  if (parse_limits(opts, &limits) != 0)
    return 1;

  v6_bundler_verbosity verbosity = v6_bundler_verbosity_normal;
  if (opts->bundle_quiet)
    verbosity = v6_bundler_verbosity_quiet;
  else if (opts->bundle_verbose)
    verbosity = v6_bundler_verbosity_verbose;

  if (opts->bundle_serve) {
    int port = opts->bundle_serve_port > 0 ? opts->bundle_serve_port : 3000;
    return v6_bundler_devserver_run(entry, opts, outfile, port);
  }

  if (opts->bundle_watch) {
    return v6_bundler_run_watch_loop(entry, fmt, opts->bundle_global_name, outfile,
                                 &limits, verbosity, NULL, NULL);
  }

  return v6_bundler_build_js_once(entry, fmt, opts->bundle_global_name, outfile,
                              &limits, verbosity, NULL, NULL);
}
