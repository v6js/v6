#include "v6/parser.h"

#include "v6/module.h"
#include "v6/internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v6/import.h"
#include "v6/literal.h"
#include "v6/scope.h"

typedef struct {
  const char* name;
  const char* cls;
  const char* field;
} node_builtin_ref;

static int specifier_ends_with(const char* spec, const char* suffix) {
  size_t slen = strlen(spec);
  size_t suflen = strlen(suffix);
  return slen >= suflen && strcmp(spec + slen - suflen, suffix) == 0;
}

static const char b64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char* base64_encode(const unsigned char* data, size_t len,
                           size_t* out_len) {
  size_t olen = ((len + 2) / 3) * 4;
  char* out = malloc(olen + 1);
  size_t i = 0, j = 0;
  while (i + 3 <= len) {
    uint32_t n =
        ((uint32_t)data[i] << 16) | ((uint32_t)data[i + 1] << 8) | data[i + 2];
    out[j++] = b64_chars[(n >> 18) & 0x3F];
    out[j++] = b64_chars[(n >> 12) & 0x3F];
    out[j++] = b64_chars[(n >> 6) & 0x3F];
    out[j++] = b64_chars[n & 0x3F];
    i += 3;
  }
  size_t rem = len - i;
  if (rem == 1) {
    uint32_t n = (uint32_t)data[i] << 16;
    out[j++] = b64_chars[(n >> 18) & 0x3F];
    out[j++] = b64_chars[(n >> 12) & 0x3F];
    out[j++] = '=';
    out[j++] = '=';
  } else if (rem == 2) {
    uint32_t n = ((uint32_t)data[i] << 16) | ((uint32_t)data[i + 1] << 8);
    out[j++] = b64_chars[(n >> 18) & 0x3F];
    out[j++] = b64_chars[(n >> 12) & 0x3F];
    out[j++] = b64_chars[(n >> 6) & 0x3F];
    out[j++] = '=';
  }
  out[j] = '\0';
  *out_len = j;
  return out;
}

static const node_builtin_ref v6_node_builtin_table[] = {
    {"path", "V6Path", "MODULE"},
    {"buffer", "V6Builtins", "NODE_BUFFER"},
    {"util", "V6Util", "MODULE"},
    {"os", "V6Os", "MODULE"},
    {"tty", "V6Tty", "MODULE"},
    {"fs", "V6Fs", "MODULE"},
    {"events", "V6Builtins", "NODE_EVENTS"},
    {"assert", "V6Builtins", "NODE_ASSERT"},
    {"querystring", "V6Builtins", "NODE_QUERYSTRING"},
    {"perf_hooks", "V6PerfHooks", "MODULE"},
    {"dns", "V6Dns", "MODULE"},
    {"string_decoder", "V6Builtins", "NODE_STRING_DECODER"},
    {"url", "V6Builtins", "NODE_URL"},
    {"zlib", "V6Zlib", "MODULE"},
    {"crypto", "V6Crypto", "MODULE"},
    {"stream", "V6SparseModules", "NODE_STREAM"},
    {"child_process", "V6SparseModules", "NODE_CHILD_PROCESS"},
    {"net", "V6SparseModules", "NODE_NET"},
    {"http", "V6SparseModules", "NODE_HTTP"},
    {"https", "V6SparseModules", "NODE_HTTPS"},
    {"tls", "V6SparseModules", "NODE_TLS"},
    {"readline", "V6SparseModules", "NODE_READLINE"},
    {"worker_threads", "V6SparseModules", "NODE_WORKER_THREADS"},
    {"cluster", "V6SparseModules", "NODE_CLUSTER"},
    {"repl", "V6SparseModules", "NODE_REPL"},
    {"timers", "V6SparseModules", "NODE_TIMERS"},
    {"dgram", "V6SparseModules", "NODE_DGRAM"},
    {"http2", "V6SparseModules", "NODE_HTTP2"},
    {"v8", "V6SparseModules", "NODE_V8"},
    {"module", "V6SparseModules", "NODE_MODULE"},
    {"diagnostics_channel", "V6SparseModules", "NODE_DIAGNOSTICS_CHANNEL"},
    {"async_hooks", "V6SparseModules", "NODE_ASYNC_HOOKS"},
    {"inspector", "V6SparseModules", "NODE_INSPECTOR"},
    {"trace_events", "V6SparseModules", "NODE_TRACE_EVENTS"},
    {"wasi", "V6SparseModules", "NODE_WASI"},
};

static int emit_node_builtin_ref(compiler* c, const char* specifier) {
  if (strncmp(specifier, "node:", 5) == 0)
    specifier += 5;
  size_t n = sizeof(v6_node_builtin_table) / sizeof(v6_node_builtin_table[0]);
  for (size_t i = 0; i < n; i++) {
    if (strcmp(specifier, v6_node_builtin_table[i].name) == 0) {
      uint16_t fidx = cf_fieldref(c->cf, v6_node_builtin_table[i].cls,
                                  v6_node_builtin_table[i].field, "LV6Value;");
      op_emit2(c->m, op_getstatic, fidx);
      return 1;
    }
  }
  return 0;
}

static int java_specifier_is_class(const char* spec) {
  const char* last_dot = strrchr(spec, '.');
  const char* seg = last_dot ? last_dot + 1 : spec;
  return seg[0] >= 'A' && seg[0] <= 'Z';
}

static int emit_java_import_ref(compiler* c, const char* specifier) {
  if (strncmp(specifier, "java:", 5) != 0)
    return 0;
  const char* fqcn = specifier + 5;
  uint16_t str_idx = cf_string(c->cf, fqcn);
  op_emit2(c->m, op_ldc_w, str_idx);
  if (java_specifier_is_class(fqcn)) {
    uint16_t midx = cf_methodref(c->cf, "V6JavaInterop", "classFor",
                                 "(Ljava/lang/String;)LV6Value;");
    op_emit2(c->m, op_invokestatic, midx);
  } else {
    uint16_t midx = cf_methodref(c->cf, "V6JavaInterop", "packageFor",
                                 "(Ljava/lang/String;)LV6Value;");
    op_emit2(c->m, op_invokestatic, midx);
  }
  return 1;
}

static int sniff_module_kind(const char* importer_dir, const char* specifier) {
  char abs_path[v6_max_path];
  char err[256];
  if (resolve_module_specifier(importer_dir, specifier, abs_path,
                               sizeof(abs_path), err, sizeof(err)) != 0)
    return 0;
  FILE* f = fopen(abs_path, "rb");
  if (!f)
    return 0;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  char* src = malloc((size_t)n + 1);
  fread(src, 1, (size_t)n, f);
  src[n] = '\0';
  fclose(f);

  lexer lx;
  lex_init(&lx, src);
  lx.auto_regex = 1;
  int is_esm = 0;
  tok t = lex_next(&lx);
  while (t.kind != tok_eof) {
    if (t.kind == tok_kw_export) {
      is_esm = 1;
      break;
    }
    t = lex_next(&lx);
  }
  free(src);
  return is_esm ? 0 : 1;
}

static compiled_module* get_or_compile_module(module_ctx* modctx,
                                              const char* importer_dir,
                                              const char* specifier, int kind,
                                              parser* p) {
  char abs_path[v6_max_path];
  char err[256];
  if (resolve_module_specifier(importer_dir, specifier, abs_path,
                               sizeof(abs_path), err, sizeof(err)) != 0) {
    error_at(p, err);
    return NULL;
  }
  for (int i = 0; i < modctx->count; i++) {
    if (modctx->modules[i].kind == kind &&
        strcmp(modctx->modules[i].abs_path, abs_path) == 0)
      return &modctx->modules[i];
  }
  if (modctx->count >= v6_max_modules) {
    error_at(p, "too many modules");
    return NULL;
  }
  int idx = modctx->count++;
  compiled_module* mod = &modctx->modules[idx];
  snprintf(mod->abs_path, sizeof(mod->abs_path), "%s", abs_path);
  mod->kind = kind;
  snprintf(mod->class_name, sizeof(mod->class_name), "Mod%d", idx);
  mod->state = 1;
  mod->cf = malloc(sizeof(class_file));
  cf_init(mod->cf, mod->class_name, "java/lang/Object");

  FILE* f = fopen(abs_path, "rb");
  if (!f) {
    error_at(p, "cannot read module file");
    mod->state = 2;
    return mod;
  }
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  char* modsrc = malloc((size_t)n + 1);
  fread(modsrc, 1, (size_t)n, f);
  modsrc[n] = '\0';
  fclose(f);

  size_t abs_len = strlen(abs_path);
  if (abs_len >= 5 && strcmp(abs_path + abs_len - 5, ".json") == 0) {
    method* json_m = cf_method(mod->cf, acc_public | acc_static,
                               "moduleExports", "()LV6Value;");
    json_m->max_stack = 2;
    size_t total = strlen(modsrc);
    const size_t chunk_limit = 60000;
    size_t pos = 0;
    int first = 1;
    uint16_t concat_idx =
        cf_methodref(mod->cf, "java/lang/String", "concat",
                     "(Ljava/lang/String;)Ljava/lang/String;");
    while (pos < total || first) {
      size_t take = total - pos;
      if (take > chunk_limit) {
        take = chunk_limit;
        while (take > 0 && (((uint8_t)modsrc[pos + take]) & 0xc0) == 0x80)
          take--;
      }
      char save = modsrc[pos + take];
      modsrc[pos + take] = '\0';
      uint16_t str_idx = cf_string(mod->cf, modsrc + pos);
      modsrc[pos + take] = save;
      op_emit2(json_m, op_ldc_w, str_idx);
      if (!first)
        op_emit2(json_m, op_invokevirtual, concat_idx);
      first = 0;
      pos += take;
    }
    uint16_t parse_idx = cf_methodref(mod->cf, "V6Json", "parse",
                                      "(Ljava/lang/String;)LV6Value;");
    op_emit2(json_m, op_invokestatic, parse_idx);
    op_emit(json_m, op_areturn);
    free(modsrc);
    mod->state = 2;
    return mod;
  }

  if (abs_len >= 5 && strcmp(abs_path + abs_len - 5, ".wasm") == 0) {
    method* wasm_m = cf_method(mod->cf, acc_public | acc_static,
                               "moduleExports", "()LV6Value;");
    wasm_m->max_stack = 3;

    size_t b64_len;
    char* b64 =
        base64_encode((const unsigned char*)modsrc, (size_t)n, &b64_len);

    uint16_t decoder_get_idx =
        cf_methodref(mod->cf, "java/util/Base64", "getDecoder",
                     "()Ljava/util/Base64$Decoder;");
    op_emit2(wasm_m, op_invokestatic, decoder_get_idx);

    const size_t chunk_limit = 60000;
    size_t pos = 0;
    int first = 1;
    uint16_t concat_idx =
        cf_methodref(mod->cf, "java/lang/String", "concat",
                     "(Ljava/lang/String;)Ljava/lang/String;");
    while (pos < b64_len || first) {
      size_t take = b64_len - pos;
      if (take > chunk_limit)
        take = chunk_limit;
      char save = b64[pos + take];
      b64[pos + take] = '\0';
      uint16_t str_idx = cf_string(mod->cf, b64 + pos);
      b64[pos + take] = save;
      op_emit2(wasm_m, op_ldc_w, str_idx);
      if (!first)
        op_emit2(wasm_m, op_invokevirtual, concat_idx);
      first = 0;
      pos += take;
    }
    free(b64);

    uint16_t decode_idx = cf_methodref(mod->cf, "java/util/Base64$Decoder",
                                       "decode", "(Ljava/lang/String;)[B");
    op_emit2(wasm_m, op_invokevirtual, decode_idx);

    uint16_t inst_idx = cf_methodref(mod->cf, "V6WasmGlobal",
                                     "instantiateBytesSync", "([B)LV6Value;");
    op_emit2(wasm_m, op_invokestatic, inst_idx);
    op_emit(wasm_m, op_areturn);

    free(modsrc);
    mod->state = 2;
    return mod;
  }

  char moddir[v6_max_path];
  path_dirname(abs_path, moddir, sizeof(moddir));

  compile_result r = compile_module_impl(mod->cf, mod->class_name, modsrc,
                                         moddir, modctx, 0, kind == 1);
  free(modsrc);
  mod->state = 2;
  if (!r.ok) {
    char combined[1024];
    snprintf(combined, sizeof(combined), "%s:%d: %s", abs_path, r.line,
             r.message);
    error_at(p, combined);
  }
  return mod;
}

void emit_require_expr(parser* p, compiler* c) {
  advance(p);
  if (!check(p, tok_str)) {
    skip_balanced(p, tok_lparen, tok_rparen);
    char msg[] = "Error: dynamic require() arguments are not supported";
    emit_string_value(c, msg);
    uint16_t throw_cls = cf_class(c->cf, "V6Throw");
    uint16_t throw_ctor =
        cf_methodref(c->cf, "V6Throw", "<init>", "(LV6Value;)V");
    op_emit2(c->m, op_new, throw_cls);
    op_emit(c->m, op_dup_x1);
    op_emit(c->m, op_swap);
    op_emit2(c->m, op_invokespecial, throw_ctor);
    op_emit(c->m, op_athrow);
    return;
  }
  tok spec_tok = p->cur;
  advance(p);
  if (!expect(p, tok_rparen))
    return;
  char* spec = decode_string(spec_tok);
  if (emit_node_builtin_ref(c, spec) || emit_java_import_ref(c, spec)) {
    free(spec);
    return;
  }
  if (!c->modctx) {
    error_at(p, "require() is not supported in this context");
    free(spec);
    return;
  }
  char abs_path[v6_max_path];
  char resolve_err[256];
  if (resolve_module_specifier(c->module_dir, spec, abs_path, sizeof(abs_path),
                               resolve_err, sizeof(resolve_err)) != 0) {
    char msg[320];
    snprintf(msg, sizeof(msg), "Error: Cannot find module '%s'", spec);
    free(spec);
    emit_string_value(c, msg);
    uint16_t throw_cls = cf_class(c->cf, "V6Throw");
    uint16_t throw_ctor =
        cf_methodref(c->cf, "V6Throw", "<init>", "(LV6Value;)V");
    op_emit2(c->m, op_new, throw_cls);
    op_emit(c->m, op_dup_x1);
    op_emit(c->m, op_swap);
    op_emit2(c->m, op_invokespecial, throw_ctor);
    op_emit(c->m, op_athrow);
    return;
  }
  compiled_module* mod =
      get_or_compile_module(c->modctx, c->module_dir, spec, 1, p);
  free(spec);
  if (!mod)
    return;
  uint16_t req_idx =
      cf_methodref(c->cf, mod->class_name, "moduleExports", "()LV6Value;");
  op_emit2(c->m, op_invokestatic, req_idx);
}

static void declare_or_assign_module_binding(compiler* c, tok name) {
  if (c->brace_depth == 0) {
    local* le = find_local_entry(c, name.start, name.len);
    if (le) {
      var_ref vr;
      vr.kind = var_local;
      vr.index = le->slot;
      emit_var_write_ref(c, vr);
      op_emit(c->m, op_pop);
      return;
    }
  }
  uint16_t slot = next_declared_slot(c);
  emit_var_declare(c, slot);
  add_local(c, name, slot, 0, 0);
}

void parse_import_stmt(parser* p, compiler* c) {
  if (!c->modctx) {
    error_at(p, "imports are not supported in this context");
    return;
  }

  if (check(p, tok_str)) {
    tok spec_tok = p->cur;
    advance(p);
    char* spec = decode_string(spec_tok);
    if (emit_node_builtin_ref(c, spec) || emit_java_import_ref(c, spec)) {
      free(spec);
      op_emit(c->m, op_pop);
      expect_semi(p);
      return;
    }
    int sniffed_kind = specifier_ends_with(spec, ".wasm")
                           ? 1
                           : sniff_module_kind(c->module_dir, spec);
    compiled_module* mod =
        get_or_compile_module(c->modctx, c->module_dir, spec, sniffed_kind, p);
    free(spec);
    expect_semi(p);
    if (!mod)
      return;
    if (mod->kind == 1) {
      uint16_t exp_idx =
          cf_methodref(c->cf, mod->class_name, "moduleExports", "()LV6Value;");
      op_emit2(c->m, op_invokestatic, exp_idx);
    } else {
      uint16_t exp_idx =
          cf_methodref(c->cf, mod->class_name, "exports", "()LV6Object;");
      op_emit2(c->m, op_invokestatic, exp_idx);
    }
    op_emit(c->m, op_pop);
    return;
  }

  int has_default = 0;
  tok default_name;
  int has_namespace = 0;
  tok namespace_name;
  int has_named = 0;
  tok named_local[64];
  tok named_key[64];
  int named_count = 0;

  if (check(p, tok_ident)) {
    has_default = 1;
    default_name = p->cur;
    advance(p);
    if (match(p, tok_comma)) {
      if (match(p, tok_star)) {
        if (!expect(p, tok_kw_as))
          return;
        if (!expect(p, tok_ident))
          return;
        has_namespace = 1;
        namespace_name = p->prev;
      } else if (match(p, tok_lbrace)) {
        has_named = 1;
        if (!check(p, tok_rbrace)) {
          for (;;) {
            if (!expect(p, tok_ident))
              return;
            tok key = p->prev;
            tok local = key;
            if (match(p, tok_kw_as)) {
              if (!expect(p, tok_ident))
                return;
              local = p->prev;
            }
            if (named_count < 64) {
              named_key[named_count] = key;
              named_local[named_count] = local;
              named_count++;
            }
            if (!match(p, tok_comma))
              break;
            if (check(p, tok_rbrace))
              break;
          }
        }
        if (!expect(p, tok_rbrace))
          return;
      }
    }
  } else if (match(p, tok_star)) {
    if (!expect(p, tok_kw_as))
      return;
    if (!expect(p, tok_ident))
      return;
    has_namespace = 1;
    namespace_name = p->prev;
  } else if (match(p, tok_lbrace)) {
    has_named = 1;
    if (!check(p, tok_rbrace)) {
      for (;;) {
        if (!expect(p, tok_ident))
          return;
        tok key = p->prev;
        tok local = key;
        if (match(p, tok_kw_as)) {
          if (!expect(p, tok_ident))
            return;
          local = p->prev;
        }
        if (named_count < 64) {
          named_key[named_count] = key;
          named_local[named_count] = local;
          named_count++;
        }
        if (!match(p, tok_comma))
          break;
        if (check(p, tok_rbrace))
          break;
      }
    }
    if (!expect(p, tok_rbrace))
      return;
  }

  if (!expect(p, tok_kw_from))
    return;
  if (!check(p, tok_str)) {
    error_at(p, "expected module specifier string");
    return;
  }
  tok spec_tok = p->cur;
  advance(p);
  char* spec = decode_string(spec_tok);

  int is_value_shape;
  uint16_t exports_slot = c->next_local_slot++;

  if (emit_node_builtin_ref(c, spec) || emit_java_import_ref(c, spec)) {
    free(spec);
    is_value_shape = 1;
    emit_astore(c->m, exports_slot);
  } else {
    int sniffed_kind = specifier_ends_with(spec, ".wasm")
                           ? 1
                           : sniff_module_kind(c->module_dir, spec);
    compiled_module* mod =
        get_or_compile_module(c->modctx, c->module_dir, spec, sniffed_kind, p);
    free(spec);
    if (!mod) {
      expect_semi(p);
      return;
    }
    if (mod->kind == 1) {
      uint16_t exp_idx =
          cf_methodref(c->cf, mod->class_name, "moduleExports", "()LV6Value;");
      op_emit2(c->m, op_invokestatic, exp_idx);
      is_value_shape = 1;
    } else {
      uint16_t exp_idx =
          cf_methodref(c->cf, mod->class_name, "exports", "()LV6Object;");
      op_emit2(c->m, op_invokestatic, exp_idx);
      is_value_shape = 0;
    }
    emit_astore(c->m, exports_slot);
  }
  expect_semi(p);

  uint16_t get_obj_idx =
      cf_methodref(c->cf, "V6Object", "get", "(Ljava/lang/String;)LV6Value;");
  uint16_t get_val_idx = cf_methodref(c->cf, "V6Value", "getProp",
                                      "(Ljava/lang/String;)LV6Value;");
  uint16_t get_idx = is_value_shape ? get_val_idx : get_obj_idx;

  if (has_default) {
    emit_aload(c->m, exports_slot);
    if (!is_value_shape) {
      uint16_t key_idx = cf_string(c->cf, "default");
      op_emit2(c->m, op_ldc_w, key_idx);
      op_emit2(c->m, op_invokevirtual, get_obj_idx);
    }
    declare_or_assign_module_binding(c, default_name);
  }

  if (has_namespace) {
    emit_aload(c->m, exports_slot);
    if (!is_value_shape)
      emit_box_object_ref(c);
    declare_or_assign_module_binding(c, namespace_name);
  }

  if (has_named) {
    for (int i = 0; i < named_count; i++) {
      emit_aload(c->m, exports_slot);
      char* keystr = dup_tok(named_key[i]);
      uint16_t key_idx = cf_string(c->cf, keystr);
      free(keystr);
      op_emit2(c->m, op_ldc_w, key_idx);
      op_emit2(c->m, op_invokevirtual, get_idx);
      declare_or_assign_module_binding(c, named_local[i]);
    }
  }
}
