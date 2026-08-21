#include "v6/cli.h"
#include "v6/bundle_assets.h"
#include "v6/bundle_graph.h"
#include "v6/bundle_emit.h"
#include "v6/bundle_fsutil.h"
#include "v6/bundle_html.h"
#include "v6/bundle_watch.h"
#include "v6/module.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define bundle_sleep_ms(ms) Sleep((DWORD)(ms))
#else
#include <unistd.h>
#define bundle_sleep_ms(ms) usleep((useconds_t)(ms) * 1000)
#endif

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

static int collect_watch_dirs(bundle_graph* g, char*** out_dirs, int* out_count) {
  char** dirs = malloc(sizeof(char*) * (size_t)(g->count > 0 ? g->count : 1));
  int count = 0;
  for (int i = 0; i < g->count; i++) {
    char dir[1024];
    path_dirname(g->modules[i]->abs_path, dir, sizeof(dir));
    int dup = 0;
    for (int j = 0; j < count; j++) {
      if (strcmp(dirs[j], dir) == 0) {
        dup = 1;
        break;
      }
    }
    if (!dup) {
      size_t n = strlen(dir);
      dirs[count] = malloc(n + 1);
      memcpy(dirs[count], dir, n + 1);
      count++;
    }
  }
  *out_dirs = dirs;
  *out_count = count;
  return 0;
}

static void free_watch_dirs(char** dirs, int count) {
  for (int i = 0; i < count; i++)
    free(dirs[i]);
  free(dirs);
}

static int build_js_once(const char* entry, bundle_format fmt,
                         v6_cli_options* opts, const char* outfile,
                         char*** out_dirs, int* out_dir_count) {
  bundle_graph g;
  bundle_graph_init(&g);
  int rc = bundle_graph_build(&g, entry);
  if (rc != 0) {
    for (int i = 0; i < g.error_count; i++)
      fprintf(stderr, "error: %s\n", g.errors[i]);
    bundle_graph_free(&g);
    return 1;
  }

  char dir[1024];
  path_dirname(outfile, dir, sizeof(dir));
  bundle_mkdir_p(dir);

  if (bundle_process_assets(&g, dir) != 0) {
    fprintf(stderr, "error: failed to write asset files under %s/assets\n", dir);
    bundle_graph_free(&g);
    return 1;
  }

  bundle_emit_options eopts;
  eopts.format = fmt;
  eopts.global_name = opts->bundle_global_name;

  size_t out_len = 0;
  char* output = bundle_emit(&g, &eopts, &out_len);

  if (bundle_write_file(outfile, output, out_len) != 0) {
    fprintf(stderr, "error: cannot write %s\n", outfile);
    free(output);
    bundle_graph_free(&g);
    return 1;
  }

  printf("bundled %d module%s -> %s (%zu bytes)\n", g.count,
         g.count == 1 ? "" : "s", outfile, out_len);
  fflush(stdout);

  free(output);

  if (out_dirs)
    collect_watch_dirs(&g, out_dirs, out_dir_count);

  bundle_graph_free(&g);
  return 0;
}

static int run_watch_loop(const char* entry, bundle_format fmt,
                          v6_cli_options* opts, const char* outfile) {
  for (;;) {
    char** dirs = NULL;
    int dir_count = 0;
    build_js_once(entry, fmt, opts, outfile, &dirs, &dir_count);

    bundle_watcher* w = bundle_watcher_create();
    for (int i = 0; i < dir_count; i++)
      bundle_watcher_add_dir(w, dirs[i]);
    free_watch_dirs(dirs, dir_count);

    printf("watching %d director%s for changes...\n", dir_count,
           dir_count == 1 ? "y" : "ies");
    fflush(stdout);

    int changed = bundle_watcher_wait(w, -1);
    bundle_watcher_free(w);
    if (changed < 0)
      return 1;

    bundle_sleep_ms(50);
  }
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

  if (opts->bundle_serve) {
    fprintf(stderr, "error: --serve is not implemented yet\n");
    return 1;
  }

  int is_html = has_suffix(entry, ".html") || has_suffix(entry, ".htm");

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

  if (opts->bundle_watch)
    return run_watch_loop(entry, fmt, opts, outfile);

  return build_js_once(entry, fmt, opts, outfile, NULL, NULL);
}
