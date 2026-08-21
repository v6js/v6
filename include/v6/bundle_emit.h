#pragma once

#include "v6/bundle_graph.h"
#include "v6/bundle_strbuf.h"

#include <stddef.h>

typedef enum {
  bundle_fmt_esm,
  bundle_fmt_cjs,
  bundle_fmt_iife,
  bundle_fmt_dev,
} bundle_format;

typedef struct bundle_emit_options {
  bundle_format format;
  const char* global_name;
} bundle_emit_options;

char* bundle_emit(bundle_graph* g, const bundle_emit_options* opts,
                  size_t* out_len);
void bundle_emit_one_module(bundle_strbuf* b, bundle_module* m);
