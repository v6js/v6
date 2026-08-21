#pragma once

#include "v6/ast.h"
#include "v6/bundle_arena.h"

typedef struct bundle_specifier {
  const char* text;
  size_t len;
  int is_require;
} bundle_specifier;

typedef struct bundle_specifier_list {
  bundle_specifier* items;
  int len;
  int cap;
} bundle_specifier_list;

void bundle_scan_imports(bundle_arena* out_arena, ast_node* program,
                         bundle_specifier_list* out);
