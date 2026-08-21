#pragma once

#include "v6/ast.h"
#include "v6/bundle_arena.h"
#include "v6/bundle_intern.h"
#include "v6/export_scan.h"

#include <stddef.h>

typedef enum {
  bundle_mod_js,
  bundle_mod_json,
  bundle_mod_css,
  bundle_mod_asset,
} bundle_module_kind;

typedef struct bundle_module bundle_module;

typedef struct bundle_import_edge {
  const char* specifier;
  bundle_module* target;
  int is_require;
} bundle_import_edge;

struct bundle_module {
  const char* abs_path;
  bundle_module_kind kind;
  char* source;
  size_t source_len;
  ast_arena ast_arena_storage;
  ast_node* program;
  bundle_import_edge* imports;
  int import_count;
  int import_cap;
  export_binding exports_list[v6_max_exports];
  int exports_count;
  const char* asset_url;
  int visited;
  int order_index;
};

typedef struct bundle_graph {
  bundle_module** modules;
  int count;
  int cap;
  bundle_arena arena;
  bundle_intern_table intern;
  bundle_module* entry;
  char** errors;
  int error_count;
  int error_cap;
} bundle_graph;

void bundle_graph_init(bundle_graph* g);
void bundle_graph_free(bundle_graph* g);
int bundle_graph_build(bundle_graph* g, const char* entry_path);
void bundle_graph_topo_order(bundle_graph* g, bundle_module*** out_order,
                             int* out_count);
bundle_module_kind bundle_classify_path(const char* path);
