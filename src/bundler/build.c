#ifndef _WIN32
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#endif

#include "v6/bundler_build.h"
#include "v6/bundler_assets.h"
#include "v6/bundler_fsutil.h"
#include "v6/bundler_graph.h"
#include "v6/bundler_report.h"
#include "v6/bundler_watch.h"
#include "v6/module.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define v6_bundler_sleep_ms(ms) Sleep((DWORD)(ms))
#else
#include <unistd.h>
#define v6_bundler_sleep_ms(ms) usleep((useconds_t)(ms) * 1000)
#endif

static int collect_watch_dirs(v6_bundler_graph* g, char*** out_dirs, int* out_count) {
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

int v6_bundler_build_js_once(const char* entry, v6_bundler_format fmt, const char* global_name,
                         const char* outfile, const v6_bundler_limits* limits,
                         v6_bundler_verbosity verbosity, char*** out_dirs,
                         int* out_dir_count) {
  v6_bundler_graph g;
  v6_bundler_graph_init(&g);
  int rc = v6_bundler_graph_build(&g, entry);
  if (rc != 0) {
    for (int i = 0; i < g.error_count; i++)
      fprintf(stderr, "error: %s\n", g.errors[i]);
    v6_bundler_graph_free(&g);
    return 1;
  }

  char dir[1024];
  path_dirname(outfile, dir, sizeof(dir));
  v6_bundler_mkdir_p(dir);

  if (v6_bundler_process_assets(&g, dir) != 0) {
    fprintf(stderr, "error: failed to write asset files under %s/assets\n", dir);
    v6_bundler_graph_free(&g);
    return 1;
  }

  v6_bundler_emit_options eopts;
  eopts.format = fmt;
  eopts.global_name = global_name;

  size_t out_len = 0;
  char* output = v6_bundler_emit(&g, &eopts, &out_len);

  if (v6_bundler_write_file(outfile, output, out_len) != 0) {
    fprintf(stderr, "error: cannot write %s\n", outfile);
    free(output);
    v6_bundler_graph_free(&g);
    return 1;
  }

  v6_bundler_print_bundle_summary(&g, outfile, out_len, verbosity);
  fflush(stdout);

  int limits_failed = limits ? v6_bundler_check_limits(&g, out_len, limits, outfile) : 0;

  free(output);

  if (out_dirs)
    collect_watch_dirs(&g, out_dirs, out_dir_count);

  v6_bundler_graph_free(&g);
  return limits_failed ? 1 : 0;
}

int v6_bundler_run_watch_loop(const char* entry, v6_bundler_format fmt, const char* global_name,
                          const char* outfile, const v6_bundler_limits* limits,
                          v6_bundler_verbosity verbosity,
                          v6_bundler_rebuild_cb on_rebuild, void* cb_arg) {
  for (;;) {
    char** dirs = NULL;
    int dir_count = 0;
    int rc = v6_bundler_build_js_once(entry, fmt, global_name, outfile, limits,
                                      verbosity, &dirs, &dir_count);

    if (rc == 0 && on_rebuild)
      on_rebuild(cb_arg);

    v6_bundler_watcher* w = v6_bundler_watcher_create();
    for (int i = 0; i < dir_count; i++)
      v6_bundler_watcher_add_dir(w, dirs[i]);
    free_watch_dirs(dirs, dir_count);

    printf("watching %d director%s for changes...\n", dir_count,
           dir_count == 1 ? "y" : "ies");
    fflush(stdout);

    int changed = v6_bundler_watcher_wait(w, -1);
    v6_bundler_watcher_free(w);
    if (changed < 0)
      return 1;

    v6_bundler_sleep_ms(50);
  }
}
