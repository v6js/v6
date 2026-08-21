#pragma once

#include "v6/bundler_graph.h"
#include "v6/bundler_strbuf.h"

#include <stddef.h>

typedef enum {
  v6_bundler_fmt_esm,
  v6_bundler_fmt_cjs,
  v6_bundler_fmt_iife,
  v6_bundler_fmt_dev,
} v6_bundler_format;

typedef struct v6_bundler_emit_options {
  v6_bundler_format format;
  const char* global_name;
} v6_bundler_emit_options;

char* v6_bundler_emit(v6_bundler_graph* g, const v6_bundler_emit_options* opts,
                      size_t* out_len);
void v6_bundler_emit_one_module(v6_bundler_strbuf* b, v6_bundler_module* m);
