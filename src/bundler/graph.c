#ifndef _WIN32
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#endif

#include "v6/bundle_graph.h"
#include "v6/bundle_scan.h"
#include "v6/module.h"
#include "v6/parser.h"
#include "v6/ast_parse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define v6_getcwd _getcwd
#else
#include <unistd.h>
#define v6_getcwd getcwd
#endif

bundle_module_kind bundle_classify_path(const char* path) {
  const char* dot = strrchr(path, '.');
  if (!dot)
    return bundle_mod_js;
  if (strcmp(dot, ".json") == 0)
    return bundle_mod_json;
  if (strcmp(dot, ".css") == 0)
    return bundle_mod_css;
  if (strcmp(dot, ".js") == 0 || strcmp(dot, ".mjs") == 0 ||
      strcmp(dot, ".cjs") == 0)
    return bundle_mod_js;
  return bundle_mod_asset;
}

static char* read_file_text(const char* path, size_t* out_len) {
  FILE* f = fopen(path, "rb");
  if (!f)
    return NULL;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (n < 0) {
    fclose(f);
    return NULL;
  }
  char* buf = malloc((size_t)n + 1);
  size_t got = fread(buf, 1, (size_t)n, f);
  buf[got] = '\0';
  fclose(f);
  if (out_len)
    *out_len = got;
  return buf;
}

void bundle_graph_init(bundle_graph* g) {
  bundle_arena_init(&g->arena);
  bundle_intern_init(&g->intern, &g->arena);
  g->modules = NULL;
  g->count = 0;
  g->cap = 0;
  g->entry = NULL;
  g->errors = NULL;
  g->error_count = 0;
  g->error_cap = 0;
}

void bundle_graph_free(bundle_graph* g) {
  for (int i = 0; i < g->count; i++) {
    if (g->modules[i]->kind == bundle_mod_js && g->modules[i]->program) {
      ast_arena_free(&g->modules[i]->ast_arena_storage);
    }
    free(g->modules[i]->source);
  }
  free(g->modules);
  for (int i = 0; i < g->error_count; i++)
    free(g->errors[i]);
  free(g->errors);
  bundle_intern_free(&g->intern);
  bundle_arena_free(&g->arena);
}

static void push_error(bundle_graph* g, const char* msg) {
  if (g->error_count >= g->error_cap) {
    int new_cap = g->error_cap == 0 ? 8 : g->error_cap * 2;
    g->errors = realloc(g->errors, sizeof(char*) * new_cap);
    g->error_cap = new_cap;
  }
  size_t n = strlen(msg);
  char* copy = malloc(n + 1);
  memcpy(copy, msg, n + 1);
  g->errors[g->error_count++] = copy;
}

static bundle_module* find_module(bundle_graph* g, const char* abs_path) {
  for (int i = 0; i < g->count; i++) {
    if (g->modules[i]->abs_path == abs_path)
      return g->modules[i];
  }
  return NULL;
}

static bundle_module* add_module(bundle_graph* g, const char* abs_path) {
  bundle_module* m = bundle_arena_alloc(&g->arena, sizeof(bundle_module));
  memset(m, 0, sizeof(*m));
  m->abs_path = abs_path;
  m->kind = bundle_classify_path(abs_path);
  if (g->count >= g->cap) {
    int new_cap = g->cap == 0 ? 16 : g->cap * 2;
    g->modules = realloc(g->modules, sizeof(bundle_module*) * new_cap);
    g->cap = new_cap;
  }
  g->modules[g->count++] = m;
  return m;
}

static void add_edge(bundle_graph* g, bundle_module* from, const char* spec,
                     size_t spec_len, int is_require, bundle_module* target) {
  if (from->import_count >= from->import_cap) {
    int new_cap = from->import_cap == 0 ? 4 : from->import_cap * 2;
    bundle_import_edge* items =
        bundle_arena_alloc(&g->arena, sizeof(bundle_import_edge) * new_cap);
    if (from->imports)
      memcpy(items, from->imports,
             sizeof(bundle_import_edge) * from->import_count);
    from->imports = items;
    from->import_cap = new_cap;
  }
  from->imports[from->import_count].specifier =
      bundle_intern(&g->intern, spec, spec_len);
  from->imports[from->import_count].target = target;
  from->imports[from->import_count].is_require = is_require;
  from->import_count++;
}

static bundle_module* load_module(bundle_graph* g, const char* abs_path) {
  bundle_module* existing = find_module(g, abs_path);
  if (existing)
    return existing;

  bundle_module* m = add_module(g, abs_path);

  size_t len = 0;
  char* text = read_file_text(abs_path, &len);
  if (!text) {
    char msg[1200];
    snprintf(msg, sizeof(msg), "cannot read module: %s", abs_path);
    push_error(g, msg);
    return m;
  }
  m->source = text;
  m->source_len = len;

  if (m->kind != bundle_mod_js)
    return m;

  m->exports_count = 0;
  preprocess_exports(m->source, m->exports_list, &m->exports_count);

  ast_arena_init(&m->ast_arena_storage);
  parser p;
  parser_init(&p, m->source);
  ast_node* program = ast_parse_program_from(&m->ast_arena_storage, &p);
  if (p.had_error) {
    char msg[1200];
    snprintf(msg, sizeof(msg), "%s:%d: %s", abs_path, p.err_line, p.err_msg);
    push_error(g, msg);
    return m;
  }
  m->program = program;

  bundle_specifier_list specs;
  bundle_scan_imports(&g->arena, program, &specs);

  char importer_dir[v6_max_path];
  path_dirname(abs_path, importer_dir, sizeof(importer_dir));

  for (int i = 0; i < specs.len; i++) {
    char spec_buf[1024];
    size_t sl = specs.items[i].len;
    if (sl >= sizeof(spec_buf))
      sl = sizeof(spec_buf) - 1;
    memcpy(spec_buf, specs.items[i].text, sl);
    spec_buf[sl] = '\0';

    char resolved[v6_max_path];
    char err[256];
    if (resolve_module_specifier(importer_dir, spec_buf, resolved,
                                 sizeof(resolved), err, sizeof(err)) != 0) {
      char msg[1500];
      snprintf(msg, sizeof(msg), "%s: cannot resolve \"%s\": %s", abs_path,
               spec_buf, err);
      push_error(g, msg);
      add_edge(g, m, spec_buf, sl, specs.items[i].is_require, NULL);
      continue;
    }
    const char* resolved_interned = bundle_intern_cstr(&g->intern, resolved);
    bundle_module* target = load_module(g, resolved_interned);
    add_edge(g, m, spec_buf, sl, specs.items[i].is_require, target);
  }

  return m;
}

int bundle_graph_build(bundle_graph* g, const char* entry_path) {
  char cwd[v6_max_path];
  if (!v6_getcwd(cwd, sizeof(cwd))) {
    push_error(g, "cannot determine current working directory");
    return -1;
  }
  char resolved[v6_max_path];
  char err[256];
  char qualified[v6_max_path];
  int looks_relative = entry_path[0] == '.' || entry_path[0] == '/' ||
                       entry_path[0] == '\\' ||
                       (entry_path[0] != '\0' && entry_path[1] == ':');
  if (!looks_relative) {
    snprintf(qualified, sizeof(qualified), "./%s", entry_path);
    entry_path = qualified;
  }
  if (resolve_module_specifier(cwd, entry_path, resolved, sizeof(resolved), err,
                               sizeof(err)) != 0) {
    char msg[1500];
    snprintf(msg, sizeof(msg), "cannot resolve entry \"%s\": %s", entry_path,
             err);
    push_error(g, msg);
    return -1;
  }
  const char* entry_interned = bundle_intern_cstr(&g->intern, resolved);
  g->entry = load_module(g, entry_interned);
  return g->error_count == 0 ? 0 : -1;
}

static void topo_visit(bundle_module* m, bundle_module** out, int* out_count) {
  if (!m || m->visited)
    return;
  m->visited = 1;
  for (int i = 0; i < m->import_count; i++) {
    topo_visit(m->imports[i].target, out, out_count);
  }
  out[(*out_count)++] = m;
  m->order_index = *out_count - 1;
}

void bundle_graph_topo_order(bundle_graph* g, bundle_module*** out_order,
                             int* out_count) {
  for (int i = 0; i < g->count; i++)
    g->modules[i]->visited = 0;
  bundle_module** out = malloc(sizeof(bundle_module*) * (size_t)g->count);
  int count = 0;
  topo_visit(g->entry, out, &count);
  for (int i = 0; i < g->count; i++)
    topo_visit(g->modules[i], out, &count);
  *out_order = out;
  *out_count = count;
}
