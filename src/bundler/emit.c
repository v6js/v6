#include "v6/bundler_emit.h"
#include "v6/bundler_strbuf.h"

#include <stdlib.h>
#include <string.h>

static void emit_js_string(v6_bundler_strbuf* b, const char* s, size_t len) {
  v6_bundler_strbuf_append(b, "\"", 1);
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c == '"' || c == '\\') {
      v6_bundler_strbuf_append(b, "\\", 1);
      v6_bundler_strbuf_append(b, (const char*)&c, 1);
    } else if (c == '\n') {
      v6_bundler_strbuf_append_cstr(b, "\\n");
    } else if (c == '\r') {
      v6_bundler_strbuf_append_cstr(b, "\\r");
    } else {
      v6_bundler_strbuf_append(b, (const char*)&c, 1);
    }
  }
  v6_bundler_strbuf_append(b, "\"", 1);
}

static int is_valid_ident(const char* s, size_t len) {
  if (len == 0)
    return 0;
  for (size_t i = 0; i < len; i++) {
    char c = s[i];
    int alpha = (c == '_' || c == '$' || (c >= 'a' && c <= 'z') ||
                 (c >= 'A' && c <= 'Z'));
    int digit = c >= '0' && c <= '9';
    if (i == 0 && !alpha)
      return 0;
    if (i > 0 && !alpha && !digit)
      return 0;
  }
  return 1;
}

static void emit_member_access(v6_bundler_strbuf* b, const char* base,
                               const char* key, size_t key_len) {
  if (is_valid_ident(key, key_len)) {
    v6_bundler_strbuf_append_fmt(b, "%s.", base);
    v6_bundler_strbuf_append(b, key, key_len);
  } else {
    v6_bundler_strbuf_append_fmt(b, "%s[", base);
    emit_js_string(b, key, key_len);
    v6_bundler_strbuf_append_cstr(b, "]");
  }
}

static void emit_require_call(v6_bundler_strbuf* b, const char* spec,
                              size_t len) {
  v6_bundler_strbuf_append_cstr(b, "require(");
  emit_js_string(b, spec, len);
  v6_bundler_strbuf_append_cstr(b, ")");
}

static void emit_member_access_expr(v6_bundler_strbuf* b, const char* spec,
                                    size_t spec_len, const char* key,
                                    size_t key_len) {
  emit_require_call(b, spec, spec_len);
  if (is_valid_ident(key, key_len)) {
    v6_bundler_strbuf_append_cstr(b, ".");
    v6_bundler_strbuf_append(b, key, key_len);
  } else {
    v6_bundler_strbuf_append_cstr(b, "[");
    emit_js_string(b, key, key_len);
    v6_bundler_strbuf_append_cstr(b, "]");
  }
}

static v6_bundler_module*
find_import_target(v6_bundler_module* m, const char* spec, size_t spec_len) {
  for (int i = 0; i < m->import_count; i++) {
    if (strlen(m->imports[i].specifier) == spec_len &&
        memcmp(m->imports[i].specifier, spec, spec_len) == 0)
      return m->imports[i].target;
  }
  return NULL;
}

static void emit_import_replacement(v6_bundler_strbuf* b,
                                    v6_bundler_module* owner, ast_node* n) {
  ast_import_binding* ib = n->import_binding;
  if (!ib || ib->is_bare) {
    emit_require_call(b, n->str, n->str_len);
    v6_bundler_strbuf_append_cstr(b, ";\n");
    return;
  }

  if (ib->has_default) {
    v6_bundler_module* target = find_import_target(owner, n->str, n->str_len);
    v6_bundler_strbuf_append_cstr(b, "var ");
    v6_bundler_strbuf_append(b, ib->default_name, ib->default_len);
    v6_bundler_strbuf_append_cstr(b, " = ");
    if (target && (target->kind == v6_bundler_mod_json ||
                   target->kind == v6_bundler_mod_css ||
                   target->kind == v6_bundler_mod_asset)) {
      emit_require_call(b, n->str, n->str_len);
    } else {
      emit_member_access_expr(b, n->str, n->str_len, "default", 7);
    }
    v6_bundler_strbuf_append_cstr(b, ";\n");
  }
  if (ib->has_namespace) {
    v6_bundler_strbuf_append_cstr(b, "var ");
    v6_bundler_strbuf_append(b, ib->namespace_name, ib->namespace_len);
    v6_bundler_strbuf_append_cstr(b, " = ");
    emit_require_call(b, n->str, n->str_len);
    v6_bundler_strbuf_append_cstr(b, ";\n");
  }
  for (int i = 0; i < ib->named_count; i++) {
    v6_bundler_strbuf_append_cstr(b, "var ");
    v6_bundler_strbuf_append(b, ib->named[i].local, ib->named[i].local_len);
    v6_bundler_strbuf_append_cstr(b, " = ");
    emit_member_access_expr(b, n->str, n->str_len, ib->named[i].key,
                            ib->named[i].key_len);
    v6_bundler_strbuf_append_cstr(b, ";\n");
  }
  if (!ib->has_default && !ib->has_namespace && ib->named_count == 0) {
    emit_require_call(b, n->str, n->str_len);
    v6_bundler_strbuf_append_cstr(b, ";\n");
  }
}

static void emit_module_body(v6_bundler_strbuf* b, v6_bundler_module* m) {
  if (m->kind == v6_bundler_mod_json) {
    v6_bundler_strbuf_append_cstr(b, "module.exports = JSON.parse(");
    emit_js_string(b, m->source, m->source_len);
    v6_bundler_strbuf_append_cstr(b, ");\n");
    return;
  }
  if (m->kind == v6_bundler_mod_css || m->kind == v6_bundler_mod_asset) {
    v6_bundler_strbuf_append_cstr(b, "module.exports = ");
    emit_js_string(b, m->asset_url, strlen(m->asset_url));
    v6_bundler_strbuf_append_cstr(b, ";\n");
    return;
  }

  ast_list* list = &m->program->list;
  for (int i = 0; i < list->len; i++) {
    ast_node* stmt = list->items[i];
    const char* start = stmt->stmt_src_start;
    const char* end = (i + 1 < list->len) ? list->items[i + 1]->stmt_src_start
                                          : m->source + m->source_len;
    if (stmt->kind == ast_import) {
      emit_import_replacement(b, m, stmt);
    } else if (start && end && end >= start) {
      v6_bundler_strbuf_append(b, start, (size_t)(end - start));
    }
  }
  v6_bundler_strbuf_append_cstr(b, "\n");

  for (int i = 0; i < m->exports_count; i++) {
    emit_member_access(b, "exports", m->exports_list[i].export_key,
                       strlen(m->exports_list[i].export_key));
    v6_bundler_strbuf_append_cstr(b, " = ");
    v6_bundler_strbuf_append_cstr(b, m->exports_list[i].local_name);
    v6_bundler_strbuf_append_cstr(b, ";\n");
  }
}

static void emit_module_id(v6_bundler_strbuf* b, const v6_bundler_module* m) {
  emit_js_string(b, m->id, strlen(m->id));
}

void v6_bundler_emit_one_module(v6_bundler_strbuf* b, v6_bundler_module* m) {
  v6_bundler_strbuf_append_cstr(b, "__v6_modules[");
  emit_module_id(b, m);
  v6_bundler_strbuf_append_cstr(b,
                                "] = function(module, exports, require) {\n");
  emit_module_body(b, m);
  v6_bundler_strbuf_append_cstr(b, "};\n");

  v6_bundler_strbuf_append_cstr(b, "__v6_specmap[");
  emit_module_id(b, m);
  v6_bundler_strbuf_append_cstr(b, "] = {");
  for (int j = 0; j < m->import_count; j++) {
    if (j > 0)
      v6_bundler_strbuf_append_cstr(b, ",");
    emit_js_string(b, m->imports[j].specifier, strlen(m->imports[j].specifier));
    v6_bundler_strbuf_append_cstr(b, ":");
    if (m->imports[j].target) {
      emit_module_id(b, m->imports[j].target);
    } else {
      v6_bundler_strbuf_append_cstr(b, "null");
    }
  }
  v6_bundler_strbuf_append_cstr(b, "};\n");
}

static void emit_runtime_preamble(v6_bundler_strbuf* b) {
  v6_bundler_strbuf_append_cstr(
      b, "var __v6_modules = typeof __v6_modules !== \"undefined\" ? "
         "__v6_modules : {};\n"
         "var __v6_specmap = typeof __v6_specmap !== \"undefined\" ? "
         "__v6_specmap : {};\n"
         "var __v6_cache = typeof __v6_cache !== \"undefined\" ? __v6_cache "
         ": {};\n"
         "function __v6_make_require(fromId) {\n"
         "  return function(spec) {\n"
         "    var map = __v6_specmap[fromId] || {};\n"
         "    var toId = map[spec];\n"
         "    if (toId === undefined || toId === null) {\n"
         "      throw new Error(\"cannot resolve \\\"\" + spec + \"\\\" from "
         "module \" + fromId);\n"
         "    }\n"
         "    return __v6_require(toId);\n"
         "  };\n"
         "}\n"
         "function __v6_require(id) {\n"
         "  if (__v6_cache[id]) return __v6_cache[id].exports;\n"
         "  var mod = { exports: {} };\n"
         "  __v6_cache[id] = mod;\n"
         "  __v6_modules[id](mod, mod.exports, __v6_make_require(id));\n"
         "  return mod.exports;\n"
         "}\n"
         "function __v6_hmr_apply(ids) {\n"
         "  for (var i = 0; i < ids.length; i++) delete __v6_cache[ids[i]];\n"
         "  for (var i = 0; i < ids.length; i++) __v6_require(ids[i]);\n"
         "}\n");
}

char* v6_bundler_emit(v6_bundler_graph* g, const v6_bundler_emit_options* opts,
                      size_t* out_len) {
  v6_bundler_module** order;
  int count;
  v6_bundler_graph_topo_order(g, &order, &count);

  v6_bundler_strbuf b;
  v6_bundler_strbuf_init(&b);

  if (opts->format == v6_bundler_fmt_iife) {
    v6_bundler_strbuf_append_cstr(&b, "(function() {\n\"use strict\";\n");
  }

  emit_runtime_preamble(&b);

  for (int i = 0; i < count; i++) {
    v6_bundler_emit_one_module(&b, order[i]);
  }

  free(order);

  v6_bundler_strbuf_append_cstr(&b, "var __v6_entry_exports = __v6_require(");
  emit_module_id(&b, g->entry);
  v6_bundler_strbuf_append_cstr(&b, ");\n");

  if (opts->format == v6_bundler_fmt_cjs) {
    v6_bundler_strbuf_append_cstr(&b, "module.exports = __v6_entry_exports;\n");
  } else if (opts->format == v6_bundler_fmt_esm) {
    v6_bundler_strbuf_append_cstr(&b, "export default __v6_entry_exports;\n");
    for (int i = 0; i < g->entry->exports_count; i++) {
      const char* key = g->entry->exports_list[i].export_key;
      if (strcmp(key, "default") == 0)
        continue;
      if (!is_valid_ident(key, strlen(key)))
        continue;
      v6_bundler_strbuf_append_fmt(
          &b, "export var %s = __v6_entry_exports.%s;\n", key, key);
    }
  } else if (opts->format == v6_bundler_fmt_iife) {
    if (opts->global_name) {
      v6_bundler_strbuf_append_fmt(&b, "globalThis.%s = __v6_entry_exports;\n",
                                   opts->global_name);
    }
    v6_bundler_strbuf_append_cstr(&b, "})();\n");
  }

  return v6_bundler_strbuf_take(&b, out_len);
}

void v6_bundler_emit_runtime_preamble(v6_bundler_strbuf* b) {
  emit_runtime_preamble(b);
}

void v6_bundler_emit_entry_require(v6_bundler_strbuf* b,
                                   v6_bundler_module* entry,
                                   const char* global_name) {
  v6_bundler_strbuf_append_cstr(b, "var __v6_entry_exports = __v6_require(");
  emit_module_id(b, entry);
  v6_bundler_strbuf_append_cstr(b, ");\n");
  if (global_name) {
    v6_bundler_strbuf_append_fmt(b, "globalThis.%s = __v6_entry_exports;\n",
                                 global_name);
  }
}
