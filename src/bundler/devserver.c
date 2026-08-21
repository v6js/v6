#ifndef _WIN32
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#endif

#include "v6/bundler_devserver.h"
#include "v6/bundler_assets.h"
#include "v6/bundler_emit.h"
#include "v6/bundler_fsutil.h"
#include "v6/bundler_graph.h"
#include "v6/bundler_hmr.h"
#include "v6/bundler_http.h"
#include "v6/bundler_report.h"
#include "v6/bundler_thread.h"
#include "v6/bundler_watch.h"
#include "v6/color.h"
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

typedef struct watch_thread_ctx {
  const char* entry;
  const char* global_name;
  const char* outfile;
  v6_bundler_verbosity verbosity;
  v6_bundler_hmr_clients* clients;
} watch_thread_ctx;

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

static void print_rebuild_status(const v6_bundler_graph* g, size_t out_len,
                                 v6_bundler_verbosity verbosity) {
  if (verbosity == v6_bundler_verbosity_quiet)
    return;
  int c = v6_color_enabled_out();
  char size_buf[32];
  v6_bundler_format_size((double)out_len, size_buf, sizeof(size_buf));
  printf("%s%s%12s%s %d module%s, %s\n", v6_c_bold(c), v6_c_green(c), "Bundled",
        v6_c_reset(c), g->count, g->count == 1 ? "" : "s", size_buf);
  fflush(stdout);
}

static void watch_thread_fn(void* arg) {
  watch_thread_ctx* wc = (watch_thread_ctx*)arg;
  v6_bundler_hmr_snapshot snap;
  v6_bundler_hmr_snapshot_init(&snap);

  for (;;) {
    if (wc->verbosity != v6_bundler_verbosity_quiet)
      v6_bundler_clear_screen();

    v6_bundler_graph g;
    v6_bundler_graph_init(&g);
    int rc = v6_bundler_graph_build(&g, wc->entry);
    if (rc != 0) {
      for (int i = 0; i < g.error_count; i++)
        fprintf(stderr, "error: %s\n", g.errors[i]);
      v6_bundler_graph_free(&g);
      v6_bundler_sleep_ms(500);
      continue;
    }

    char dir[1024];
    path_dirname(wc->outfile, dir, sizeof(dir));
    v6_bundler_mkdir_p(dir);
    v6_bundler_process_assets(&g, dir);

    v6_bundler_emit_options eopts;
    eopts.format = v6_bundler_fmt_dev;
    eopts.global_name = wc->global_name;
    size_t out_len = 0;
    char* output = v6_bundler_emit(&g, &eopts, &out_len);
    v6_bundler_write_file(wc->outfile, output, out_len);
    print_rebuild_status(&g, out_len, wc->verbosity);
    free(output);

    size_t patch_len = 0;
    char* patch = v6_bundler_hmr_compute_patch(&snap, &g, &patch_len);
    if (patch) {
      if (wc->verbosity != v6_bundler_verbosity_quiet) {
        int hc = v6_color_enabled_out();
        printf("%s%s%12s%s pushing module update\n", v6_c_bold(hc), v6_c_cyan(hc),
              "HMR", v6_c_reset(hc));
        fflush(stdout);
      }
      v6_bundler_hmr_broadcast(wc->clients, patch);
      free(patch);
    }
    v6_bundler_hmr_snapshot_capture(&snap, &g);

    char** dirs = NULL;
    int dir_count = 0;
    collect_watch_dirs(&g, &dirs, &dir_count);
    v6_bundler_graph_free(&g);

    v6_bundler_watcher* w = v6_bundler_watcher_create();
    for (int i = 0; i < dir_count; i++)
      v6_bundler_watcher_add_dir(w, dirs[i]);
    free_watch_dirs(dirs, dir_count);

    if (wc->verbosity != v6_bundler_verbosity_quiet) {
      printf("Watching %d director%s for changes...\n", dir_count,
            dir_count == 1 ? "y" : "ies");
      fflush(stdout);
    }

    int changed = v6_bundler_watcher_wait(w, -1);
    v6_bundler_watcher_free(w);
    if (changed < 0)
      break;

    v6_bundler_sleep_ms(50);
  }

  v6_bundler_hmr_snapshot_free(&snap);
}

int v6_bundler_devserver_run(const char* entry, v6_cli_options* opts,
                         const char* outfile, int port) {
  char serve_dir[1024];
  path_dirname(outfile, serve_dir, sizeof(serve_dir));
  v6_bundler_mkdir_p(serve_dir);

  char index_path[1200];
  snprintf(index_path, sizeof(index_path), "%s/index.html", serve_dir);
  if (!path_is_regular_file(index_path)) {
    const char* base = strrchr(outfile, '/');
    const char* bbase = strrchr(outfile, '\\');
    if (bbase && (!base || bbase > base))
      base = bbase;
    base = base ? base + 1 : outfile;

    char html[512];
    snprintf(html, sizeof(html),
            "<!doctype html>\n<html>\n<head><meta charset=\"utf-8\"></head>\n"
            "<body>\n<script src=\"%s\"></script>\n</body>\n</html>\n",
            base);
    FILE* f = fopen(index_path, "wb");
    if (f) {
      fwrite(html, 1, strlen(html), f);
      fclose(f);
    }
  }

  v6_bundler_hmr_clients* clients = v6_bundler_hmr_clients_create();

  v6_bundler_verbosity verbosity = v6_bundler_verbosity_normal;
  if (opts->bundle_quiet)
    verbosity = v6_bundler_verbosity_quiet;
  else if (opts->bundle_verbose)
    verbosity = v6_bundler_verbosity_verbose;

  watch_thread_ctx* wc = malloc(sizeof(watch_thread_ctx));
  wc->entry = entry;
  wc->global_name = opts->bundle_global_name;
  wc->outfile = outfile;
  wc->verbosity = verbosity;
  wc->clients = clients;
  v6_bundler_thread_start(watch_thread_fn, wc);

  return v6_bundler_http_serve(serve_dir, port, clients);
}
