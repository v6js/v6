#pragma once

#include "v6/ast.h"
#include "v6/bundler_arena.h"
#include "v6/bundler_extension.h"
#include "v6/bundler_intern.h"
#include "v6/export_scan.h"

#include <stddef.h>

typedef enum {
  v6_bundler_mod_js,
  v6_bundler_mod_json,
  v6_bundler_mod_css,
  v6_bundler_mod_asset,
} v6_bundler_module_kind;

typedef struct v6_bundler_module v6_bundler_module;

typedef struct v6_bundler_import_edge {
  const char* specifier;
  v6_bundler_module* target;
  int is_require;
} v6_bundler_import_edge;

struct v6_bundler_module {
  const char* abs_path;
  const char* id;
  v6_bundler_module_kind kind;
  char* source;
  size_t source_len;
  ast_arena ast_arena_storage;
  ast_node* program;
  v6_bundler_import_edge* imports;
  int import_count;
  int import_cap;
  export_binding exports_list[v6_max_exports];
  int exports_count;
  const char* asset_url;
  int visited;
  int order_index;
};

typedef struct v6_bundler_graph {
  v6_bundler_module** modules;
  int count;
  int cap;
  v6_bundler_arena arena;
  v6_bundler_intern_table intern;
  v6_bundler_module* entry;
  const char* root_dir;
  char** errors;
  int error_count;
  int error_cap;
  v6_bundler_extension_set* extensions;
} v6_bundler_graph;

void v6_bundler_graph_init(v6_bundler_graph* g);
void v6_bundler_graph_free(v6_bundler_graph* g);
int v6_bundler_graph_build(v6_bundler_graph* g, const char* entry_path);
int v6_bundler_graph_build_with_root(v6_bundler_graph* g,
                                     const char* entry_path,
                                     const char* root_dir_override);
void v6_bundler_graph_topo_order(v6_bundler_graph* g,
                                 v6_bundler_module*** out_order,
                                 int* out_count);
v6_bundler_module_kind v6_bundler_classify_path(const char* path);
