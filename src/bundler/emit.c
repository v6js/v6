#include "v6/bundle_emit.h"
#include "v6/bundle_strbuf.h"

#include <stdlib.h>
#include <string.h>

static void emit_js_string(bundle_strbuf* b, const char* s, size_t len) {
  bundle_strbuf_append(b, "\"", 1);
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c == '"' || c == '\\') {
      bundle_strbuf_append(b, "\\", 1);
      bundle_strbuf_append(b, (const char*)&c, 1);
    } else if (c == '\n') {
      bundle_strbuf_append_cstr(b, "\\n");
    } else if (c == '\r') {
      bundle_strbuf_append_cstr(b, "\\r");
    } else {
      bundle_strbuf_append(b, (const char*)&c, 1);
    }
  }
  bundle_strbuf_append(b, "\"", 1);
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

static void emit_member_access(bundle_strbuf* b, const char* base,
                               const char* key, size_t key_len) {
  if (is_valid_ident(key, key_len)) {
    bundle_strbuf_append_fmt(b, "%s.", base);
    bundle_strbuf_append(b, key, key_len);
  } else {
    bundle_strbuf_append_fmt(b, "%s[", base);
    emit_js_string(b, key, key_len);
    bundle_strbuf_append_cstr(b, "]");
  }
}

static void emit_require_call(bundle_strbuf* b, const char* spec, size_t len) {
  bundle_strbuf_append_cstr(b, "require(");
  emit_js_string(b, spec, len);
  bundle_strbuf_append_cstr(b, ")");
}

static void emit_member_access_expr(bundle_strbuf* b, const char* spec,
                                    size_t spec_len, const char* key,
                                    size_t key_len) {
  emit_require_call(b, spec, spec_len);
  if (is_valid_ident(key, key_len)) {
    bundle_strbuf_append_cstr(b, ".");
    bundle_strbuf_append(b, key, key_len);
  } else {
    bundle_strbuf_append_cstr(b, "[");
    emit_js_string(b, key, key_len);
    bundle_strbuf_append_cstr(b, "]");
  }
}

static bundle_module* find_import_target(bundle_module* m, const char* spec,
                                         size_t spec_len) {
  for (int i = 0; i < m->import_count; i++) {
    if (strlen(m->imports[i].specifier) == spec_len &&
        memcmp(m->imports[i].specifier, spec, spec_len) == 0)
      return m->imports[i].target;
  }
  return NULL;
}

static void emit_import_replacement(bundle_strbuf* b, bundle_module* owner,
                                    ast_node* n) {
  ast_import_binding* ib = n->import_binding;
  if (!ib || ib->is_bare) {
    emit_require_call(b, n->str, n->str_len);
    bundle_strbuf_append_cstr(b, ";\n");
    return;
  }

  if (ib->has_default) {
    bundle_module* target = find_import_target(owner, n->str, n->str_len);
    bundle_strbuf_append_cstr(b, "var ");
    bundle_strbuf_append(b, ib->default_name, ib->default_len);
    bundle_strbuf_append_cstr(b, " = ");
    if (target && (target->kind == bundle_mod_json ||
                   target->kind == bundle_mod_css ||
                   target->kind == bundle_mod_asset)) {
      emit_require_call(b, n->str, n->str_len);
    } else {
      emit_member_access_expr(b, n->str, n->str_len, "default", 7);
    }
    bundle_strbuf_append_cstr(b, ";\n");
  }
  if (ib->has_namespace) {
    bundle_strbuf_append_cstr(b, "var ");
    bundle_strbuf_append(b, ib->namespace_name, ib->namespace_len);
    bundle_strbuf_append_cstr(b, " = ");
    emit_require_call(b, n->str, n->str_len);
    bundle_strbuf_append_cstr(b, ";\n");
  }
  for (int i = 0; i < ib->named_count; i++) {
    bundle_strbuf_append_cstr(b, "var ");
    bundle_strbuf_append(b, ib->named[i].local, ib->named[i].local_len);
    bundle_strbuf_append_cstr(b, " = ");
    emit_member_access_expr(b, n->str, n->str_len, ib->named[i].key,
                            ib->named[i].key_len);
    bundle_strbuf_append_cstr(b, ";\n");
  }
  if (!ib->has_default && !ib->has_namespace && ib->named_count == 0) {
    emit_require_call(b, n->str, n->str_len);
    bundle_strbuf_append_cstr(b, ";\n");
  }
}

static void emit_module_body(bundle_strbuf* b, bundle_module* m) {
  if (m->kind == bundle_mod_json) {
    bundle_strbuf_append_cstr(b, "module.exports = (");
    bundle_strbuf_append(b, m->source, m->source_len);
    bundle_strbuf_append_cstr(b, ");\n");
    return;
  }
  if (m->kind == bundle_mod_css || m->kind == bundle_mod_asset) {
    bundle_strbuf_append_cstr(b, "module.exports = ");
    emit_js_string(b, m->asset_url, strlen(m->asset_url));
    bundle_strbuf_append_cstr(b, ";\n");
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
      bundle_strbuf_append(b, start, (size_t)(end - start));
    }
  }
  bundle_strbuf_append_cstr(b, "\n");

  for (int i = 0; i < m->exports_count; i++) {
    emit_member_access(b, "exports", m->exports_list[i].export_key,
                       strlen(m->exports_list[i].export_key));
    bundle_strbuf_append_cstr(b, " = ");
    bundle_strbuf_append_cstr(b, m->exports_list[i].local_name);
    bundle_strbuf_append_cstr(b, ";\n");
  }
}

static void emit_module_id(bundle_strbuf* b, const bundle_module* m) {
  emit_js_string(b, m->abs_path, strlen(m->abs_path));
}

void bundle_emit_one_module(bundle_strbuf* b, bundle_module* m) {
  bundle_strbuf_append_cstr(b, "__v6_modules[");
  emit_module_id(b, m);
  bundle_strbuf_append_cstr(b, "] = function(module, exports, require) {\n");
  emit_module_body(b, m);
  bundle_strbuf_append_cstr(b, "};\n");

  bundle_strbuf_append_cstr(b, "__v6_specmap[");
  emit_module_id(b, m);
  bundle_strbuf_append_cstr(b, "] = {");
  for (int j = 0; j < m->import_count; j++) {
    if (j > 0)
      bundle_strbuf_append_cstr(b, ",");
    emit_js_string(b, m->imports[j].specifier, strlen(m->imports[j].specifier));
    bundle_strbuf_append_cstr(b, ":");
    if (m->imports[j].target) {
      emit_module_id(b, m->imports[j].target);
    } else {
      bundle_strbuf_append_cstr(b, "null");
    }
  }
  bundle_strbuf_append_cstr(b, "};\n");
}

static void emit_runtime_preamble(bundle_strbuf* b) {
  bundle_strbuf_append_cstr(
      b, "var __v6_modules = {};\n"
         "var __v6_specmap = {};\n"
         "var __v6_cache = {};\n"
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

char* bundle_emit(bundle_graph* g, const bundle_emit_options* opts,
                  size_t* out_len) {
  bundle_module** order;
  int count;
  bundle_graph_topo_order(g, &order, &count);

  bundle_strbuf b;
  bundle_strbuf_init(&b);

  if (opts->format == bundle_fmt_iife) {
    bundle_strbuf_append_cstr(&b, "(function() {\n\"use strict\";\n");
  }

  emit_runtime_preamble(&b);

  for (int i = 0; i < count; i++) {
    bundle_emit_one_module(&b, order[i]);
  }

  free(order);

  bundle_strbuf_append_cstr(&b, "var __v6_entry_exports = __v6_require(");
  emit_module_id(&b, g->entry);
  bundle_strbuf_append_cstr(&b, ");\n");

  if (opts->format == bundle_fmt_cjs) {
    bundle_strbuf_append_cstr(&b, "module.exports = __v6_entry_exports;\n");
  } else if (opts->format == bundle_fmt_esm) {
    bundle_strbuf_append_cstr(&b, "export default __v6_entry_exports;\n");
    for (int i = 0; i < g->entry->exports_count; i++) {
      const char* key = g->entry->exports_list[i].export_key;
      if (strcmp(key, "default") == 0)
        continue;
      if (!is_valid_ident(key, strlen(key)))
        continue;
      bundle_strbuf_append_fmt(&b, "export var %s = __v6_entry_exports.%s;\n",
                               key, key);
    }
  } else if (opts->format == bundle_fmt_iife) {
    if (opts->global_name) {
      bundle_strbuf_append_fmt(&b, "globalThis.%s = __v6_entry_exports;\n",
                               opts->global_name);
    }
    bundle_strbuf_append_cstr(&b, "})();\n");
  }

  return bundle_strbuf_take(&b, out_len);
}
