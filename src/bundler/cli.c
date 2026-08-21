#include "v6/cli.h"
#include "v6/bundler_build.h"
#include "v6/bundler_devserver.h"
#include "v6/bundler_devserver_html.h"
#include "v6/bundler_ext_alias.h"
#include "v6/bundler_ext_banner.h"
#include "v6/bundler_ext_define.h"
#include "v6/bundler_ext_public.h"
#include "v6/bundler_extension.h"
#include "v6/bundler_html.h"
#include "v6/bundler_report.h"
#include "v6/color.h"
#include "v6/module.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define v6_getcwd _getcwd
#else
#include <unistd.h>
#define v6_getcwd getcwd
#endif

static v6_bundler_extension_set* build_extensions(v6_cli_options* opts) {
  int any = opts->bundle_define_count > 0 || opts->bundle_alias_count > 0 ||
            opts->bundle_banner || opts->bundle_public_dir;
  if (!any)
    return NULL;

  v6_bundler_extension_set* set = malloc(sizeof(v6_bundler_extension_set));
  v6_bundler_extension_set_init(set);

  if (opts->bundle_define_count > 0) {
    v6_bundler_define_state* state = v6_bundler_define_state_create();
    for (int i = 0; i < opts->bundle_define_count; i++) {
      const char* kv = opts->bundle_defines[i];
      const char* eq = strchr(kv, '=');
      if (!eq) {
        fprintf(stderr, "error: --define expects <key>=<value>, got \"%s\"\n",
                kv);
        continue;
      }
      char key[256];
      size_t klen = (size_t)(eq - kv);
      if (klen >= sizeof(key))
        klen = sizeof(key) - 1;
      memcpy(key, kv, klen);
      key[klen] = '\0';
      v6_bundler_define_state_add(state, key, eq + 1);
    }
    v6_bundler_extension_set_add(set, v6_bundler_define_extension(state));
  }

  if (opts->bundle_alias_count > 0) {
    char cwd[1024];
    if (!v6_getcwd(cwd, sizeof(cwd)))
      snprintf(cwd, sizeof(cwd), ".");
    v6_bundler_alias_state* state = v6_bundler_alias_state_create(cwd);
    for (int i = 0; i < opts->bundle_alias_count; i++) {
      const char* kv = opts->bundle_aliases[i];
      const char* eq = strchr(kv, '=');
      if (!eq) {
        fprintf(stderr, "error: --alias expects <from>=<to>, got \"%s\"\n", kv);
        continue;
      }
      char from[256];
      size_t flen = (size_t)(eq - kv);
      if (flen >= sizeof(from))
        flen = sizeof(from) - 1;
      memcpy(from, kv, flen);
      from[flen] = '\0';
      v6_bundler_alias_state_add(state, from, eq + 1);
    }
    v6_bundler_extension_set_add(set, v6_bundler_alias_extension(state));
  }

  if (opts->bundle_banner)
    v6_bundler_extension_set_add(
        set, v6_bundler_banner_extension(opts->bundle_banner));

  if (opts->bundle_public_dir)
    v6_bundler_extension_set_add(
        set, v6_bundler_public_extension(opts->bundle_public_dir));

  return set;
}

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
      "index.html", "index.js",      "src/index.js",
      "index.mjs",  "src/index.mjs", NULL,
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
      fprintf(stderr, "error: invalid --max-size \"%s\"\n",
              opts->bundle_max_size);
      return -1;
    }
  }

  if (opts->bundle_max_deps) {
    char* end;
    long long v = strtoll(opts->bundle_max_deps, &end, 10);
    if (end == opts->bundle_max_deps || v <= 0) {
      fprintf(stderr, "error: invalid --max-deps \"%s\"\n",
              opts->bundle_max_deps);
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
  v6_bundler_extension_set* extensions = build_extensions(opts);

  if (is_html) {
    const char* outdir = opts->bundle_outdir ? opts->bundle_outdir : "dist";

    if (opts->bundle_serve) {
      int port = opts->bundle_serve_port > 0 ? opts->bundle_serve_port : 3000;
      return v6_bundler_devserver_run_html(entry, opts, outdir, port,
                                           extensions);
    }
    if (opts->bundle_watch)
      return v6_bundler_run_watch_loop_html(entry, opts, outdir, extensions);

    int rc = v6_bundler_process_html(entry, outdir, opts->bundle_global_name, 0,
                                     extensions);
    if (rc == 0 && !opts->bundle_quiet) {
      int c = v6_color_enabled_out();
      printf("%s%s%10s%s %s -> %s\n", v6_c_bold(c), v6_c_green(c), "Bundled",
             v6_c_reset(c), entry, outdir);
    }
    return rc;
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
    return v6_bundler_devserver_run(entry, opts, outfile, port, extensions);
  }

  if (opts->bundle_watch) {
    return v6_bundler_run_watch_loop(entry, fmt, opts->bundle_global_name,
                                     outfile, &limits, verbosity, NULL, NULL,
                                     extensions);
  }

  return v6_bundler_build_js_once(entry, fmt, opts->bundle_global_name, outfile,
                                  &limits, verbosity, NULL, NULL, extensions);
}
