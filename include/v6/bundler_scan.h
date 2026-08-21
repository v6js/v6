#pragma once

#include "v6/ast.h"
#include "v6/bundler_arena.h"

typedef struct v6_bundler_specifier {
  const char* text;
  size_t len;
  int is_require;
} v6_bundler_specifier;

typedef struct v6_bundler_specifier_list {
  v6_bundler_specifier* items;
  int len;
  int cap;
} v6_bundler_specifier_list;

void v6_bundler_scan_imports(v6_bundler_arena* out_arena, ast_node* program,
                             v6_bundler_specifier_list* out);
