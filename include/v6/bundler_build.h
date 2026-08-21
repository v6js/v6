#pragma once

#include "v6/bundler_emit.h"
#include "v6/bundler_report.h"

typedef void (*v6_bundler_rebuild_cb)(void* arg);

int v6_bundler_build_js_once(const char* entry, v6_bundler_format fmt,
                             const char* global_name, const char* outfile,
                             const v6_bundler_limits* limits,
                             v6_bundler_verbosity verbosity, char*** out_dirs,
                             int* out_dir_count);
int v6_bundler_run_watch_loop(const char* entry, v6_bundler_format fmt,
                              const char* global_name, const char* outfile,
                              const v6_bundler_limits* limits,
                              v6_bundler_verbosity verbosity,
                              v6_bundler_rebuild_cb on_rebuild, void* cb_arg);
