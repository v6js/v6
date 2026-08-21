#pragma once

#include "v6/bundle_emit.h"

typedef void (*bundle_rebuild_cb)(void* arg);

int bundle_build_js_once(const char* entry, bundle_format fmt,
                         const char* global_name, const char* outfile,
                         char*** out_dirs, int* out_dir_count);
int bundle_run_watch_loop(const char* entry, bundle_format fmt,
                          const char* global_name, const char* outfile,
                          bundle_rebuild_cb on_rebuild, void* cb_arg);
