#pragma once

#include <stddef.h>

typedef struct v6_bundler_html_script_ref {
  char entry_path[1200];
  int is_module;
} v6_bundler_html_script_ref;

#define v6_bundler_html_max_scripts 32

int v6_bundler_process_html(const char* html_path, const char* outdir,
                            const char* global_name, int dev_mode);
int v6_bundler_html_scan_scripts(const char* html_path,
                                 v6_bundler_html_script_ref* out_scripts,
                                 int max_scripts, int* out_count);
int v6_bundler_html_resolve_root(const char* html_path, char* out,
                                 size_t out_size);
