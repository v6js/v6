#pragma once

#include "v6/bundler_graph.h"

#include <stddef.h>

typedef enum {
  v6_bundler_limit_warn,
  v6_bundler_limit_error,
} v6_bundler_limit_mode;

typedef enum {
  v6_bundler_verbosity_quiet,
  v6_bundler_verbosity_normal,
  v6_bundler_verbosity_verbose,
} v6_bundler_verbosity;

typedef struct v6_bundler_limits {
  long long max_size;
  long long max_deps;
  v6_bundler_limit_mode mode;
} v6_bundler_limits;

void v6_bundler_format_size(double bytes, char* out, size_t out_size);
int v6_bundler_parse_size(const char* s, long long* out_bytes);
void v6_bundler_clear_screen(void);

void v6_bundler_print_bundle_summary(const v6_bundler_graph* g,
                                     const char* outfile, size_t out_len,
                                     v6_bundler_verbosity verbosity);
int v6_bundler_check_limits(const v6_bundler_graph* g, size_t out_len,
                            const v6_bundler_limits* limits,
                            const char* outfile);
