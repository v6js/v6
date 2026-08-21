#pragma once

#include "v6/bundle_graph.h"

#include <stddef.h>

typedef enum {
  bundle_fmt_esm,
  bundle_fmt_cjs,
  bundle_fmt_iife,
} bundle_format;

typedef struct bundle_emit_options {
  bundle_format format;
  const char* global_name;
} bundle_emit_options;

char* bundle_emit(bundle_graph* g, const bundle_emit_options* opts,
                  size_t* out_len);
