#ifndef _WIN32
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#endif

#include "v6/bundler_devserver_html.h"
#include "v6/bundler_fsutil.h"
#include "v6/bundler_graph.h"
#include "v6/bundler_hmr.h"
#include "v6/bundler_html.h"
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

static void add_dir_if_new(char*** dirs, int* count, int* cap,
                           const char* dir) {
  for (int j = 0; j < *count; j++) {
    if (strcmp((*dirs)[j], dir) == 0)
      return;
  }
  if (*count >= *cap) {
    *cap = *cap == 0 ? 8 : *cap * 2;
    *dirs = realloc(*dirs, sizeof(char*) * (size_t)(*cap));
  }
  size_t n = strlen(dir);
  (*dirs)[*count] = malloc(n + 1);
  memcpy((*dirs)[*count], dir, n + 1);
  (*count)++;
}

static void collect_dirs_from_graph(v6_bundler_graph* g, char*** dirs,
                                    int* count, int* cap) {
  for (int i = 0; i < g->count; i++) {
    char dir[1024];
    path_dirname(g->modules[i]->abs_path, dir, sizeof(dir));
    add_dir_if_new(dirs, count, cap, dir);
  }
}

static void free_dirs(char** dirs, int count) {
  for (int i = 0; i < count; i++)
    free(dirs[i]);
  free(dirs);
}

static int collect_all_watch_dirs(const char* html_path, char*** out_dirs,
                                  int* out_count) {
  char** dirs = NULL;
  int count = 0, cap = 0;

  char html_dir[1024];
  path_dirname(html_path, html_dir, sizeof(html_dir));
  add_dir_if_new(&dirs, &count, &cap, html_dir);

  char root_dir[1024];
  v6_bundler_html_resolve_root(html_path, root_dir, sizeof(root_dir));

  v6_bundler_html_script_ref scripts[v6_bundler_html_max_scripts];
  int script_count = 0;
  v6_bundler_html_scan_scripts(html_path, scripts, v6_bundler_html_max_scripts,
                               &script_count);

  for (int i = 0; i < script_count; i++) {
    v6_bundler_graph g;
    v6_bundler_graph_init(&g);
    if (v6_bundler_graph_build_with_root(&g, scripts[i].entry_path, root_dir) ==
        0)
      collect_dirs_from_graph(&g, &dirs, &count, &cap);
    v6_bundler_graph_free(&g);
  }

  *out_dirs = dirs;
  *out_count = count;
  return script_count;
}

static v6_bundler_verbosity verbosity_from_opts(v6_cli_options* opts) {
  if (opts->bundle_quiet)
    return v6_bundler_verbosity_quiet;
  if (opts->bundle_verbose)
    return v6_bundler_verbosity_verbose;
  return v6_bundler_verbosity_normal;
}

int v6_bundler_run_watch_loop_html(const char* html_path, v6_cli_options* opts,
                                   const char* outdir) {
  v6_bundler_verbosity verbosity = verbosity_from_opts(opts);

  for (;;) {
    if (verbosity != v6_bundler_verbosity_quiet)
      v6_bundler_clear_screen();

    int rc =
        v6_bundler_process_html(html_path, outdir, opts->bundle_global_name, 0);

    if (verbosity != v6_bundler_verbosity_quiet && rc == 0) {
      int c = v6_color_enabled_out();
      printf("%s%s%12s%s %s\n", v6_c_bold(c), v6_c_green(c), "Bundled",
             v6_c_reset(c), html_path);
      fflush(stdout);
    }

    char** dirs = NULL;
    int dir_count = 0;
    collect_all_watch_dirs(html_path, &dirs, &dir_count);

    v6_bundler_watcher* w = v6_bundler_watcher_create();
    for (int i = 0; i < dir_count; i++)
      v6_bundler_watcher_add_dir(w, dirs[i]);
    free_dirs(dirs, dir_count);

    if (verbosity != v6_bundler_verbosity_quiet) {
      printf("Watching %d director%s for changes...\n", dir_count,
             dir_count == 1 ? "y" : "ies");
      fflush(stdout);
    }

    int changed = v6_bundler_watcher_wait(w, -1);
    v6_bundler_watcher_free(w);
    if (changed < 0)
      return 1;

    v6_bundler_sleep_ms(50);
  }
}

typedef struct html_watch_ctx {
  const char* html_path;
  const char* global_name;
  const char* outdir;
  v6_bundler_verbosity verbosity;
  v6_bundler_hmr_clients* clients;
} html_watch_ctx;

static void html_watch_thread_fn(void* arg) {
  html_watch_ctx* wc = (html_watch_ctx*)arg;

  v6_bundler_hmr_snapshot snaps[v6_bundler_html_max_scripts];
  int snap_count = 0;

  for (;;) {
    if (wc->verbosity != v6_bundler_verbosity_quiet)
      v6_bundler_clear_screen();

    v6_bundler_html_script_ref scripts[v6_bundler_html_max_scripts];
    int script_count = 0;
    v6_bundler_html_scan_scripts(wc->html_path, scripts,
                                 v6_bundler_html_max_scripts, &script_count);

    while (snap_count < script_count) {
      v6_bundler_hmr_snapshot_init(&snaps[snap_count]);
      snap_count++;
    }

    int rc =
        v6_bundler_process_html(wc->html_path, wc->outdir, wc->global_name, 1);

    if (wc->verbosity != v6_bundler_verbosity_quiet && rc == 0) {
      int c = v6_color_enabled_out();
      printf("%s%s%12s%s %d script%s\n", v6_c_bold(c), v6_c_green(c), "Bundled",
             v6_c_reset(c), script_count, script_count == 1 ? "" : "s");
      fflush(stdout);
    }

    char root_dir[1024];
    v6_bundler_html_resolve_root(wc->html_path, root_dir, sizeof(root_dir));

    for (int i = 0; i < script_count; i++) {
      v6_bundler_graph g;
      v6_bundler_graph_init(&g);
      if (v6_bundler_graph_build_with_root(&g, scripts[i].entry_path,
                                           root_dir) == 0) {
        size_t patch_len = 0;
        char* patch = v6_bundler_hmr_compute_patch(&snaps[i], &g, &patch_len);
        if (patch) {
          if (wc->verbosity != v6_bundler_verbosity_quiet) {
            int hc = v6_color_enabled_out();
            printf("%s%s%12s%s pushing module update\n", v6_c_bold(hc),
                   v6_c_cyan(hc), "HMR", v6_c_reset(hc));
            fflush(stdout);
          }
          v6_bundler_hmr_broadcast(wc->clients, patch);
          free(patch);
        }
        v6_bundler_hmr_snapshot_capture(&snaps[i], &g);
      }
      v6_bundler_graph_free(&g);
    }

    char** dirs = NULL;
    int dir_count = 0;
    collect_all_watch_dirs(wc->html_path, &dirs, &dir_count);

    v6_bundler_watcher* w = v6_bundler_watcher_create();
    for (int i = 0; i < dir_count; i++)
      v6_bundler_watcher_add_dir(w, dirs[i]);
    free_dirs(dirs, dir_count);

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

  for (int i = 0; i < snap_count; i++)
    v6_bundler_hmr_snapshot_free(&snaps[i]);
}

int v6_bundler_devserver_run_html(const char* html_path, v6_cli_options* opts,
                                  const char* outdir, int port) {
  v6_bundler_mkdir_p(outdir);

  v6_bundler_hmr_clients* clients = v6_bundler_hmr_clients_create();

  html_watch_ctx* wc = malloc(sizeof(html_watch_ctx));
  wc->html_path = html_path;
  wc->global_name = opts->bundle_global_name;
  wc->outdir = outdir;
  wc->verbosity = verbosity_from_opts(opts);
  wc->clients = clients;
  v6_bundler_thread_start(html_watch_thread_fn, wc);

  return v6_bundler_http_serve(outdir, port, clients);
}
