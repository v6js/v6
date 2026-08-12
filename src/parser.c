#include "v6/parser.h"

#include "v6/module.h"
#include "v6/internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v6/literal.h"
#include "v6/scope.h"
#include "v6/stmt.h"

void advance(parser* p) {
  p->prev = p->cur;
  p->cur = lex_next(&p->lex);
}

void parser_init(parser* p, const char* src) {
  lex_init(&p->lex, src);
  p->had_error = 0;
  p->err_msg[0] = '\0';
  p->err_line = 0;
  advance(p);
}

void error_at(parser* p, const char* msg) {
  if (p->had_error)
    return;
  p->had_error = 1;
  p->err_line = p->cur.line;
  size_t n = strlen(msg);
  if (n >= sizeof(p->err_msg))
    n = sizeof(p->err_msg) - 1;
  memcpy(p->err_msg, msg, n);
  p->err_msg[n] = '\0';
}

int check(parser* p, tok_kind k) {
  return p->cur.kind == k;
}

int match(parser* p, tok_kind k) {
  if (!check(p, k))
    return 0;
  advance(p);
  return 1;
}

static const char* tok_name(tok_kind k) {
  switch (k) {
  case tok_semi:
    return ";";
  case tok_colon:
    return ":";
  case tok_lparen:
    return "(";
  case tok_rparen:
    return ")";
  case tok_lbrace:
    return "{";
  case tok_rbrace:
    return "}";
  case tok_lbracket:
    return "[";
  case tok_rbracket:
    return "]";
  case tok_ident:
    return "identifier";
  default:
    return "token";
  }
}

int expect(parser* p, tok_kind k) {
  if (match(p, k))
    return 1;
  char msg[64];
  snprintf(msg, sizeof(msg), "expected '%s'", tok_name(k));
  error_at(p, msg);
  return 0;
}

int expect_semi(parser* p) {
  if (match(p, tok_semi))
    return 1;
  if (check(p, tok_rbrace) || check(p, tok_eof))
    return 1;
  if (p->cur.line > p->prev.line)
    return 1;
  char msg[64];
  snprintf(msg, sizeof(msg), "expected '%s'", tok_name(tok_semi));
  error_at(p, msg);
  return 0;
}

static int is_keyword_kind(tok_kind k) {
  switch (k) {
  case tok_kw_var:
  case tok_kw_let:
  case tok_kw_const:
  case tok_kw_function:
  case tok_kw_return:
  case tok_kw_if:
  case tok_kw_else:
  case tok_kw_while:
  case tok_kw_do:
  case tok_kw_for:
  case tok_kw_true:
  case tok_kw_false:
  case tok_kw_null:
  case tok_kw_undefined:
  case tok_kw_break:
  case tok_kw_continue:
  case tok_kw_switch:
  case tok_kw_case:
  case tok_kw_default:
  case tok_kw_typeof:
  case tok_kw_void:
  case tok_kw_delete:
  case tok_kw_in:
  case tok_kw_of:
  case tok_kw_this:
  case tok_kw_new:
  case tok_kw_class:
  case tok_kw_extends:
  case tok_kw_super:
  case tok_kw_try:
  case tok_kw_catch:
  case tok_kw_finally:
  case tok_kw_throw:
  case tok_kw_static:
  case tok_kw_instanceof:
  case tok_kw_get:
  case tok_kw_set:
  case tok_kw_async:
  case tok_kw_await:
  case tok_kw_yield:
  case tok_kw_import:
  case tok_kw_export:
  case tok_kw_from:
  case tok_kw_as:
    return 1;
  default:
    return 0;
  }
}

int is_contextual_ident(tok_kind k) {
  return k == tok_ident || k == tok_kw_async || k == tok_kw_get ||
         k == tok_kw_set || k == tok_kw_undefined;
}

int match_property_name(parser* p) {
  if (check(p, tok_ident) || is_keyword_kind(p->cur.kind)) {
    advance(p);
    return 1;
  }
  return 0;
}

void skip_balanced(parser* p, tok_kind open, tok_kind close) {
  int save_auto_regex = p->lex.auto_regex;
  p->lex.auto_regex = 1;
  int depth = 1;
  while (depth > 0 && !check(p, tok_eof)) {
    if (check(p, open))
      depth++;
    else if (check(p, close))
      depth--;
    advance(p);
  }
  p->lex.auto_regex = save_auto_regex;
}

#define v6_max_exports 128

typedef struct export_binding {
  char local_name[64];
  char export_key[64];
} export_binding;

static void blank_range(char* src, size_t start, size_t end) {
  for (size_t i = start; i < end; i++)
    if (src[i] != '\n')
      src[i] = ' ';
}

static void record_export(export_binding* bindings, int* count, tok local,
                          tok key) {
  if (*count >= v6_max_exports)
    return;
  size_t nlen = local.len < 63 ? local.len : 63;
  memcpy(bindings[*count].local_name, local.start, nlen);
  bindings[*count].local_name[nlen] = '\0';
  size_t klen = key.len < 63 ? key.len : 63;
  memcpy(bindings[*count].export_key, key.start, klen);
  bindings[*count].export_key[klen] = '\0';
  (*count)++;
}

static void preprocess_exports(char* src, export_binding* bindings,
                               int* count) {
  lexer lx;
  lex_init(&lx, src);
  lx.auto_regex = 1;
  int depth = 0;
  tok t = lex_next(&lx);
  while (t.kind != tok_eof) {
    if (t.kind == tok_lbrace) {
      depth++;
      t = lex_next(&lx);
      continue;
    }
    if (t.kind == tok_rbrace) {
      depth--;
      t = lex_next(&lx);
      continue;
    }
    if (depth != 0 || t.kind != tok_kw_export) {
      t = lex_next(&lx);
      continue;
    }

    size_t export_start = (size_t)(t.start - src);
    tok next = lex_next(&lx);

    if (next.kind == tok_kw_default) {
      size_t default_end = (size_t)(next.start - src) + next.len;
      tok after = lex_next(&lx);
      if (after.kind == tok_kw_function || after.kind == tok_kw_class) {
        tok maybe = lex_next(&lx);
        if (maybe.kind == tok_star)
          maybe = lex_next(&lx);
        if (maybe.kind == tok_ident) {
          tok key = maybe;
          key.start = "default";
          key.len = 7;
          record_export(bindings, count, maybe, key);
        }
        blank_range(src, export_start, default_end);
        t = lex_next(&lx);
        continue;
      }
      const char* tmpl = "let $dflt$   =";
      memcpy(src + export_start, tmpl, 14);
      tok synth;
      synth.start = "$dflt$";
      synth.len = 6;
      tok key;
      key.start = "default";
      key.len = 7;
      record_export(bindings, count, synth, key);
      t = after;
      continue;
    }

    if (next.kind == tok_kw_function || next.kind == tok_kw_class) {
      blank_range(src, export_start, export_start + 6);
      tok maybe = lex_next(&lx);
      if (maybe.kind == tok_star)
        maybe = lex_next(&lx);
      if (maybe.kind == tok_ident)
        record_export(bindings, count, maybe, maybe);
      t = lex_next(&lx);
      continue;
    }

    if (next.kind == tok_kw_async) {
      blank_range(src, export_start, export_start + 6);
      tok fnkw = lex_next(&lx);
      if (fnkw.kind == tok_kw_function) {
        tok maybe = lex_next(&lx);
        if (maybe.kind == tok_star)
          maybe = lex_next(&lx);
        if (maybe.kind == tok_ident)
          record_export(bindings, count, maybe, maybe);
      }
      t = lex_next(&lx);
      continue;
    }

    if (next.kind == tok_kw_var || next.kind == tok_kw_let ||
        next.kind == tok_kw_const) {
      blank_range(src, export_start, export_start + 6);
      for (;;) {
        tok name_tok = lex_next(&lx);
        if (name_tok.kind == tok_ident)
          record_export(bindings, count, name_tok, name_tok);
        int ed = 0;
        tok tt = lex_next(&lx);
        while (tt.kind != tok_eof) {
          if (tt.kind == tok_lparen || tt.kind == tok_lbracket ||
              tt.kind == tok_lbrace) {
            ed++;
          } else if (tt.kind == tok_rparen || tt.kind == tok_rbracket ||
                     tt.kind == tok_rbrace) {
            if (ed == 0)
              break;
            ed--;
          } else if (ed == 0 && (tt.kind == tok_comma || tt.kind == tok_semi)) {
            break;
          }
          tt = lex_next(&lx);
        }
        if (tt.kind != tok_comma) {
          t = lex_next(&lx);
          break;
        }
      }
      continue;
    }

    if (next.kind == tok_lbrace) {
      int ed = 1;
      tok inner = lex_next(&lx);
      while (ed > 0 && inner.kind != tok_eof) {
        if (inner.kind == tok_ident) {
          tok name_tok = inner;
          tok peek = lex_next(&lx);
          tok export_name_tok = name_tok;
          if (peek.kind == tok_kw_as) {
            export_name_tok = lex_next(&lx);
            inner = lex_next(&lx);
          } else {
            inner = peek;
          }
          record_export(bindings, count, name_tok, export_name_tok);
          continue;
        }
        if (inner.kind == tok_lbrace)
          ed++;
        else if (inner.kind == tok_rbrace) {
          ed--;
          if (ed == 0)
            break;
        }
        inner = lex_next(&lx);
      }
      tok after_brace = lex_next(&lx);
      size_t stmt_end;
      if (after_brace.kind == tok_semi) {
        stmt_end = (size_t)(after_brace.start - src) + after_brace.len;
        t = lex_next(&lx);
      } else {
        stmt_end = (size_t)(after_brace.start - src);
        t = after_brace;
      }
      blank_range(src, export_start, stmt_end);
      continue;
    }

    blank_range(src, export_start, export_start + 6);
    t = next;
  }
}

static void bind_builtin_cls(compiler* c, const char* name, const char* cls,
                             const char* field) {
  uint16_t slot = c->next_local_slot++;
  uint16_t fidx = cf_fieldref(c->cf, cls, field, "LV6Value;");
  op_emit2(c->m, op_getstatic, fidx);
  emit_var_declare(c, slot);
  tok t;
  t.kind = tok_ident;
  t.start = name;
  t.len = strlen(name);
  t.line = 0;
  t.num = 0;
  add_local(c, t, slot, 1, 0);
}

static void bind_builtin(compiler* c, const char* name, const char* field) {
  bind_builtin_cls(c, name, "V6Builtins", field);
}

static int is_ident_char(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '_' || c == '$';
}

static int src_has_ident(const char* src, const char* name) {
  size_t name_len = strlen(name);
  const char* p = src;
  while ((p = strstr(p, name)) != NULL) {
    int before_ok = (p == src) || !is_ident_char(p[-1]);
    int after_ok = !is_ident_char(p[name_len]);
    if (before_ok && after_ok)
      return 1;
    p += 1;
  }
  return 0;
}

static const char* g_entry_script_path = "";

compile_result compile_module_impl(class_file* cf, const char* this_class_name,
                                   const char* user_src, const char* module_dir,
                                   module_ctx* modctx, int is_entry,
                                   int is_cjs) {
  size_t user_len = strlen(user_src);
  char* src = malloc(user_len + 1);
  memcpy(src, user_src, user_len + 1);

  export_binding exports_list[v6_max_exports];
  int exports_count = 0;
  if (!is_cjs)
    preprocess_exports(src, exports_list, &exports_count);

  method* main_m;
  if (is_entry) {
    main_m = cf_method(cf, acc_public | acc_static, "main",
                       "([Ljava/lang/String;)V");
  } else if (is_cjs) {
    main_m =
        cf_method(cf, acc_public | acc_static, "moduleExports", "()LV6Value;");
    cf_field(cf, acc_public | acc_static, "MODULE_CACHE", "LV6Object;");
  } else {
    main_m = cf_method(cf, acc_public | acc_static, "exports", "()LV6Object;");
    cf_field(cf, acc_public | acc_static, "EXPORTS_CACHE", "LV6Object;");
  }
  main_m->max_stack = 64;

  int lambda_counter = 0;

  compiler c;
  c.cf = cf;
  c.m = main_m;
  c.parent = NULL;
  c.lambda_counter = &lambda_counter;
  c.is_arrow = 0;
  c.param_count = 0;
  c.locals = malloc(sizeof(local) * v6_initial_locals);
  c.local_count = 0;
  c.local_cap = v6_initial_locals;
  c.scratch_slot = 1;
  c.next_local_slot = 3;
  c.upvalues = malloc(sizeof(upvalue) * v6_initial_upvalues);
  c.upvalue_count = 0;
  c.upvalue_cap = v6_initial_upvalues;
  c.break_depth = 0;
  c.continue_depth = 0;
  c.catch_depth = 0;
  c.brace_depth = 0;
  c.super_name = NULL;
  c.super_len = 0;
  c.class_name = NULL;
  c.class_name_len = 0;
  c.pending_field_count = 0;
  c.label_count = 0;
  c.pending_label_count = 0;
  c.finally_depth = 0;
  c.is_async_gen = 0;
  c.pending_async_gen = 0;
  c.is_module = !is_entry;
  c.this_class_name = this_class_name;
  c.modctx = modctx;
  c.module_dir = module_dir;

  c.box_locals = 1;

  if (src_has_ident(src, "Error"))
    bind_builtin(&c, "Error", "ERROR");
  if (src_has_ident(src, "TypeError"))
    bind_builtin(&c, "TypeError", "TYPE_ERROR");
  if (src_has_ident(src, "RangeError"))
    bind_builtin(&c, "RangeError", "RANGE_ERROR");
  if (src_has_ident(src, "SyntaxError"))
    bind_builtin(&c, "SyntaxError", "SYNTAX_ERROR");
  if (src_has_ident(src, "ReferenceError"))
    bind_builtin(&c, "ReferenceError", "REFERENCE_ERROR");
  if (src_has_ident(src, "EvalError"))
    bind_builtin(&c, "EvalError", "EVAL_ERROR");
  if (src_has_ident(src, "URIError"))
    bind_builtin(&c, "URIError", "URI_ERROR");
  if (src_has_ident(src, "DOMException"))
    bind_builtin(&c, "DOMException", "DOM_EXCEPTION");
  if (src_has_ident(src, "console"))
    bind_builtin(&c, "console", "CONSOLE");
  if (src_has_ident(src, "Math"))
    bind_builtin(&c, "Math", "MATH");
  if (src_has_ident(src, "Object"))
    bind_builtin(&c, "Object", "OBJECT");
  if (src_has_ident(src, "Array"))
    bind_builtin(&c, "Array", "ARRAY");
  if (src_has_ident(src, "atob"))
    bind_builtin(&c, "atob", "ATOB");
  if (src_has_ident(src, "btoa"))
    bind_builtin(&c, "btoa", "BTOA");
  if (src_has_ident(src, "Uint8Array"))
    bind_builtin(&c, "Uint8Array", "UINT8ARRAY_CTOR");
  if (src_has_ident(src, "encodeURIComponent"))
    bind_builtin(&c, "encodeURIComponent", "ENCODE_URI_COMPONENT");
  if (src_has_ident(src, "decodeURIComponent"))
    bind_builtin(&c, "decodeURIComponent", "DECODE_URI_COMPONENT");
  if (src_has_ident(src, "encodeURI"))
    bind_builtin(&c, "encodeURI", "ENCODE_URI");
  if (src_has_ident(src, "decodeURI"))
    bind_builtin(&c, "decodeURI", "DECODE_URI");
  if (src_has_ident(src, "eval"))
    bind_builtin(&c, "eval", "EVAL_STUB");
  if (src_has_ident(src, "__v6CaptureCallSites"))
    bind_builtin_cls(&c, "__v6CaptureCallSites", "V6CaptureCallSites",
                     "CAPTURE_CALL_SITES");
  if (src_has_ident(src, "__v6ReplEcho"))
    bind_builtin(&c, "__v6ReplEcho", "REPL_ECHO");
  if (src_has_ident(src, "Number"))
    bind_builtin(&c, "Number", "NUMBER");
  if (src_has_ident(src, "parseInt"))
    bind_builtin(&c, "parseInt", "PARSE_INT");
  if (src_has_ident(src, "BigInt"))
    bind_builtin(&c, "BigInt", "BIGINT");
  if (src_has_ident(src, "parseFloat"))
    bind_builtin(&c, "parseFloat", "PARSE_FLOAT");
  if (src_has_ident(src, "isNaN"))
    bind_builtin(&c, "isNaN", "IS_NAN");
  if (src_has_ident(src, "isFinite"))
    bind_builtin(&c, "isFinite", "IS_FINITE");
  if (src_has_ident(src, "NaN"))
    bind_builtin(&c, "NaN", "NAN_VALUE");
  if (src_has_ident(src, "Infinity"))
    bind_builtin(&c, "Infinity", "INFINITY_VALUE");
  if (src_has_ident(src, "Map"))
    bind_builtin(&c, "Map", "MAP");
  if (src_has_ident(src, "Set"))
    bind_builtin(&c, "Set", "SET");
  if (src_has_ident(src, "WeakMap"))
    bind_builtin(&c, "WeakMap", "WEAK_MAP");
  if (src_has_ident(src, "WeakSet"))
    bind_builtin(&c, "WeakSet", "WEAK_SET");
  if (src_has_ident(src, "Symbol"))
    bind_builtin(&c, "Symbol", "SYMBOL");
  if (src_has_ident(src, "Promise"))
    bind_builtin(&c, "Promise", "PROMISE");
  bind_builtin(&c, "RegExp", "REGEXP");
  if (src_has_ident(src, "Function"))
    bind_builtin(&c, "Function", "FUNCTION");
  if (src_has_ident(src, "String"))
    bind_builtin(&c, "String", "STRING");
  if (src_has_ident(src, "Boolean"))
    bind_builtin(&c, "Boolean", "BOOLEAN");
  if (src_has_ident(src, "Date"))
    bind_builtin(&c, "Date", "DATE");
  if (src_has_ident(src, "JSON"))
    bind_builtin(&c, "JSON", "JSON");
  if (src_has_ident(src, "Buffer"))
    bind_builtin(&c, "Buffer", "BUFFER");
  if (src_has_ident(src, "process"))
    bind_builtin(&c, "process", "PROCESS");
  if (src_has_ident(src, "URL"))
    bind_builtin(&c, "URL", "URL_CTOR");
  if (src_has_ident(src, "URLSearchParams"))
    bind_builtin(&c, "URLSearchParams", "URL_SEARCH_PARAMS_CTOR");
  if (src_has_ident(src, "setTimeout"))
    bind_builtin(&c, "setTimeout", "SET_TIMEOUT");
  if (src_has_ident(src, "clearTimeout"))
    bind_builtin(&c, "clearTimeout", "CLEAR_TIMEOUT");
  if (src_has_ident(src, "setInterval"))
    bind_builtin(&c, "setInterval", "SET_INTERVAL");
  if (src_has_ident(src, "clearInterval"))
    bind_builtin(&c, "clearInterval", "CLEAR_INTERVAL");
  if (src_has_ident(src, "setImmediate"))
    bind_builtin(&c, "setImmediate", "SET_IMMEDIATE");
  if (src_has_ident(src, "clearImmediate"))
    bind_builtin(&c, "clearImmediate", "CLEAR_IMMEDIATE");
  if (src_has_ident(src, "queueMicrotask"))
    bind_builtin(&c, "queueMicrotask", "QUEUE_MICROTASK");
  if (src_has_ident(src, "global"))
    bind_builtin(&c, "global", "GLOBAL_OBJECT");
  if (src_has_ident(src, "globalThis"))
    bind_builtin(&c, "globalThis", "GLOBAL_OBJECT");
  if (src_has_ident(src, "Event"))
    bind_builtin_cls(&c, "Event", "V6WebGlobals", "EVENT_CTOR");
  if (src_has_ident(src, "CustomEvent"))
    bind_builtin_cls(&c, "CustomEvent", "V6WebGlobals", "CUSTOM_EVENT_CTOR");
  if (src_has_ident(src, "EventTarget"))
    bind_builtin_cls(&c, "EventTarget", "V6WebGlobals", "EVENT_TARGET_CTOR");
  if (src_has_ident(src, "AbortSignal"))
    bind_builtin_cls(&c, "AbortSignal", "V6WebGlobals", "ABORT_SIGNAL_CTOR");
  if (src_has_ident(src, "AbortController"))
    bind_builtin_cls(&c, "AbortController", "V6WebGlobals",
                     "ABORT_CONTROLLER_CTOR");
  if (src_has_ident(src, "structuredClone"))
    bind_builtin_cls(&c, "structuredClone", "V6WebGlobals", "STRUCTURED_CLONE");
  if (src_has_ident(src, "TextEncoder"))
    bind_builtin_cls(&c, "TextEncoder", "V6WebGlobals", "TEXT_ENCODER_CTOR");
  if (src_has_ident(src, "TextDecoder"))
    bind_builtin_cls(&c, "TextDecoder", "V6WebGlobals", "TEXT_DECODER_CTOR");
  if (src_has_ident(src, "ReadableStream"))
    bind_builtin_cls(&c, "ReadableStream", "V6WebGlobals",
                     "READABLE_STREAM_CTOR");
  if (src_has_ident(src, "WritableStream"))
    bind_builtin_cls(&c, "WritableStream", "V6WebGlobals",
                     "WRITABLE_STREAM_CTOR");
  if (src_has_ident(src, "TransformStream"))
    bind_builtin_cls(&c, "TransformStream", "V6WebGlobals",
                     "TRANSFORM_STREAM_CTOR");
  if (src_has_ident(src, "CountQueuingStrategy"))
    bind_builtin_cls(&c, "CountQueuingStrategy", "V6WebGlobals",
                     "COUNT_QUEUING_STRATEGY_CTOR");
  if (src_has_ident(src, "ByteLengthQueuingStrategy"))
    bind_builtin_cls(&c, "ByteLengthQueuingStrategy", "V6WebGlobals",
                     "BYTE_LENGTH_QUEUING_STRATEGY_CTOR");
  if (src_has_ident(src, "ArrayBuffer"))
    bind_builtin_cls(&c, "ArrayBuffer", "V6WebGlobals", "ARRAY_BUFFER_CTOR");
  if (src_has_ident(src, "Blob"))
    bind_builtin_cls(&c, "Blob", "V6WebGlobals", "BLOB_CTOR");
  if (src_has_ident(src, "File"))
    bind_builtin_cls(&c, "File", "V6WebGlobals", "FILE_CTOR");
  if (src_has_ident(src, "FormData"))
    bind_builtin_cls(&c, "FormData", "V6WebGlobals", "FORM_DATA_CTOR");
  if (src_has_ident(src, "TextEncoderStream"))
    bind_builtin_cls(&c, "TextEncoderStream", "V6WebGlobals",
                     "TEXT_ENCODER_STREAM_CTOR");
  if (src_has_ident(src, "TextDecoderStream"))
    bind_builtin_cls(&c, "TextDecoderStream", "V6WebGlobals",
                     "TEXT_DECODER_STREAM_CTOR");
  if (src_has_ident(src, "CompressionStream"))
    bind_builtin_cls(&c, "CompressionStream", "V6WebGlobals",
                     "COMPRESSION_STREAM_CTOR");
  if (src_has_ident(src, "DecompressionStream"))
    bind_builtin_cls(&c, "DecompressionStream", "V6WebGlobals",
                     "DECOMPRESSION_STREAM_CTOR");
  if (src_has_ident(src, "Headers"))
    bind_builtin_cls(&c, "Headers", "V6WebGlobals", "HEADERS_CTOR");
  if (src_has_ident(src, "Request"))
    bind_builtin_cls(&c, "Request", "V6WebGlobals", "REQUEST_CTOR");
  if (src_has_ident(src, "Response"))
    bind_builtin_cls(&c, "Response", "V6WebGlobals", "RESPONSE_CTOR");
  if (src_has_ident(src, "fetch"))
    bind_builtin_cls(&c, "fetch", "V6WebGlobals", "FETCH");
  if (src_has_ident(src, "WebSocket"))
    bind_builtin_cls(&c, "WebSocket", "V6WebGlobals", "WEBSOCKET_CTOR");
  if (src_has_ident(src, "EventSource"))
    bind_builtin_cls(&c, "EventSource", "V6WebGlobals", "EVENT_SOURCE_CTOR");
  if (src_has_ident(src, "CryptoKey"))
    bind_builtin_cls(&c, "CryptoKey", "V6WebGlobals", "CRYPTO_KEY_CTOR");
  if (src_has_ident(src, "crypto"))
    bind_builtin_cls(&c, "crypto", "V6WebGlobals", "WEB_CRYPTO");
  if (src_has_ident(src, "MessageEvent"))
    bind_builtin_cls(&c, "MessageEvent", "V6WebGlobals", "MESSAGE_EVENT_CTOR");
  if (src_has_ident(src, "MessagePort"))
    bind_builtin_cls(&c, "MessagePort", "V6WebGlobals", "MESSAGE_PORT_CTOR");
  if (src_has_ident(src, "MessageChannel"))
    bind_builtin_cls(&c, "MessageChannel", "V6WebGlobals",
                     "MESSAGE_CHANNEL_CTOR");
  if (src_has_ident(src, "BroadcastChannel"))
    bind_builtin_cls(&c, "BroadcastChannel", "V6WebGlobals",
                     "BROADCAST_CHANNEL_CTOR");
  if (src_has_ident(src, "self"))
    bind_builtin_cls(&c, "self", "V6WebGlobals", "WORKER_SELF");
  if (src_has_ident(src, "Worker"))
    bind_builtin_cls(&c, "Worker", "V6WebGlobals", "WEB_WORKER_CTOR");
  if (src_has_ident(src, "performance"))
    bind_builtin_cls(&c, "performance", "V6WebGlobals", "PERFORMANCE");
  if (src_has_ident(src, "navigator"))
    bind_builtin_cls(&c, "navigator", "V6WebGlobals", "NAVIGATOR");

  if (is_entry) {
    uint16_t setargv_idx =
        cf_methodref(cf, "V6Process", "setArgv", "([Ljava/lang/String;)V");
    emit_aload(main_m, 0);
    op_emit2(main_m, op_invokestatic, setargv_idx);

    uint16_t setpath_idx =
        cf_methodref(cf, "V6Process", "setScriptPath", "(Ljava/lang/String;)V");
    uint16_t path_str = cf_string(cf, g_entry_script_path);
    op_emit2(main_m, op_ldc_w, path_str);
    op_emit2(main_m, op_invokestatic, setpath_idx);
  }

  uint16_t this_slot = c.next_local_slot++;
  emit_undef(cf, main_m);
  emit_var_declare(&c, this_slot);
  tok this_tok;
  this_tok.kind = tok_kw_this;
  this_tok.start = "this";
  this_tok.len = 4;
  this_tok.line = 0;
  this_tok.num = 0;
  add_local(&c, this_tok, this_slot, 1, 0);

  uint16_t module_local_slot = 0;
  uint16_t exports_local_slot = 0;

  if (is_cjs && is_entry) {
    uint16_t obj_cls = cf_class(cf, "V6Object");
    uint16_t obj_ctor = cf_methodref(cf, "V6Object", "<init>", "()V");
    uint16_t set_idx =
        cf_methodref(cf, "V6Object", "set", "(Ljava/lang/String;LV6Value;)V");
    uint16_t exports_key = cf_string(cf, "exports");
    op_emit2(main_m, op_new, obj_cls);
    op_emit(main_m, op_dup);
    op_emit2(main_m, op_invokespecial, obj_ctor);
    op_emit(main_m, op_dup);
    op_emit2(main_m, op_ldc_w, exports_key);
    op_emit2(main_m, op_new, obj_cls);
    op_emit(main_m, op_dup);
    op_emit2(main_m, op_invokespecial, obj_ctor);
    emit_box_object_ref(&c);
    op_emit2(main_m, op_invokevirtual, set_idx);
    module_local_slot = c.next_local_slot++;
    emit_box_object_ref(&c);
    emit_var_declare(&c, module_local_slot);
    tok module_tok;
    module_tok.kind = tok_ident;
    module_tok.start = "module";
    module_tok.len = 6;
    module_tok.line = 0;
    module_tok.num = 0;
    add_local(&c, module_tok, module_local_slot, 0, 0);

    var_ref module_vr;
    module_vr.kind = var_local;
    module_vr.index = module_local_slot;
    emit_var_read_ref(&c, module_vr);
    op_emit2(main_m, op_ldc_w, exports_key);
    uint16_t getprop_idx_early =
        cf_methodref(cf, "V6Value", "getProp", "(Ljava/lang/String;)LV6Value;");
    op_emit2(main_m, op_invokevirtual, getprop_idx_early);
    exports_local_slot = c.next_local_slot++;
    emit_var_declare(&c, exports_local_slot);
    tok exports_tok;
    exports_tok.kind = tok_ident;
    exports_tok.start = "exports";
    exports_tok.len = 7;
    exports_tok.line = 0;
    exports_tok.num = 0;
    add_local(&c, exports_tok, exports_local_slot, 0, 0);
  } else if (is_cjs) {
    uint16_t cache_field =
        cf_fieldref(cf, this_class_name, "MODULE_CACHE", "LV6Object;");
    uint16_t get_idx =
        cf_methodref(cf, "V6Object", "get", "(Ljava/lang/String;)LV6Value;");
    uint16_t exports_key = cf_string(cf, "exports");
    op_emit2(main_m, op_getstatic, cache_field);
    op_emit(main_m, op_dup);
    size_t null_jump = op_pos(main_m);
    op_emit2(main_m, op_ifnull, 0);
    op_emit2(main_m, op_ldc_w, exports_key);
    op_emit2(main_m, op_invokevirtual, get_idx);
    op_emit(main_m, op_areturn);
    size_t create_pos = op_pos(main_m);
    op_patch2(main_m, (uint16_t)(null_jump + 1),
              (uint16_t)(create_pos - null_jump));
    op_emit(main_m, op_pop);
    uint16_t obj_cls = cf_class(cf, "V6Object");
    uint16_t obj_ctor = cf_methodref(cf, "V6Object", "<init>", "()V");
    uint16_t set_idx =
        cf_methodref(cf, "V6Object", "set", "(Ljava/lang/String;LV6Value;)V");
    op_emit2(main_m, op_new, obj_cls);
    op_emit(main_m, op_dup);
    op_emit2(main_m, op_invokespecial, obj_ctor);
    op_emit(main_m, op_dup);
    op_emit2(main_m, op_putstatic, cache_field);
    op_emit(main_m, op_dup);
    op_emit2(main_m, op_ldc_w, exports_key);
    op_emit2(main_m, op_new, obj_cls);
    op_emit(main_m, op_dup);
    op_emit2(main_m, op_invokespecial, obj_ctor);
    emit_box_object_ref(&c);
    op_emit2(main_m, op_invokevirtual, set_idx);
    module_local_slot = c.next_local_slot++;
    emit_box_object_ref(&c);
    emit_var_declare(&c, module_local_slot);
    tok module_tok;
    module_tok.kind = tok_ident;
    module_tok.start = "module";
    module_tok.len = 6;
    module_tok.line = 0;
    module_tok.num = 0;
    add_local(&c, module_tok, module_local_slot, 0, 0);

    var_ref module_vr;
    module_vr.kind = var_local;
    module_vr.index = module_local_slot;
    emit_var_read_ref(&c, module_vr);
    op_emit2(main_m, op_ldc_w, exports_key);
    uint16_t getprop_idx_early =
        cf_methodref(cf, "V6Value", "getProp", "(Ljava/lang/String;)LV6Value;");
    op_emit2(main_m, op_invokevirtual, getprop_idx_early);
    exports_local_slot = c.next_local_slot++;
    emit_var_declare(&c, exports_local_slot);
    tok exports_tok;
    exports_tok.kind = tok_ident;
    exports_tok.start = "exports";
    exports_tok.len = 7;
    exports_tok.line = 0;
    exports_tok.num = 0;
    add_local(&c, exports_tok, exports_local_slot, 0, 0);
  } else if (!is_entry) {
    uint16_t cache_field =
        cf_fieldref(cf, this_class_name, "EXPORTS_CACHE", "LV6Object;");
    op_emit2(main_m, op_getstatic, cache_field);
    op_emit(main_m, op_dup);
    size_t null_jump = op_pos(main_m);
    op_emit2(main_m, op_ifnull, 0);
    op_emit(main_m, op_areturn);
    size_t create_pos = op_pos(main_m);
    op_patch2(main_m, (uint16_t)(null_jump + 1),
              (uint16_t)(create_pos - null_jump));
    op_emit(main_m, op_pop);
    uint16_t obj_cls = cf_class(cf, "V6Object");
    uint16_t obj_ctor = cf_methodref(cf, "V6Object", "<init>", "()V");
    op_emit2(main_m, op_new, obj_cls);
    op_emit(main_m, op_dup);
    op_emit2(main_m, op_invokespecial, obj_ctor);
    op_emit(main_m, op_dup);
    op_emit2(main_m, op_putstatic, cache_field);
    c.exports_slot = c.next_local_slot++;
    emit_astore(main_m, c.exports_slot);
  }

  {
    uint16_t dirname_slot = c.next_local_slot++;
    emit_string_value(&c, module_dir);
    emit_var_declare(&c, dirname_slot);
    tok dirname_tok;
    dirname_tok.kind = tok_ident;
    dirname_tok.start = "__dirname";
    dirname_tok.len = 9;
    dirname_tok.line = 0;
    dirname_tok.num = 0;
    add_local(&c, dirname_tok, dirname_slot, 0, 0);
  }

  parser p;
  parser_init(&p, src);

  prescan_decls(&p, &c, src, 1);

  parse_program(&p, &c);

  if (is_entry) {
    uint16_t run_idx = cf_methodref(cf, "V6EventLoop", "run", "()V");
    op_emit2(main_m, op_invokestatic, run_idx);
    op_emit(main_m, op_return);
  } else if (is_cjs) {
    var_ref module_vr;
    module_vr.kind = var_local;
    module_vr.index = module_local_slot;
    emit_var_read_ref(&c, module_vr);
    uint16_t exports_key = cf_string(cf, "exports");
    op_emit2(main_m, op_ldc_w, exports_key);
    uint16_t getprop_idx =
        cf_methodref(cf, "V6Value", "getProp", "(Ljava/lang/String;)LV6Value;");
    op_emit2(main_m, op_invokevirtual, getprop_idx);
    op_emit(main_m, op_areturn);
  } else {
    for (int i = 0; i < exports_count; i++) {
      var_ref vr = resolve_var(&c, exports_list[i].local_name,
                               strlen(exports_list[i].local_name));
      if (vr.kind == var_not_found)
        continue;
      emit_aload(main_m, c.exports_slot);
      uint16_t key_idx = cf_string(cf, exports_list[i].export_key);
      op_emit2(main_m, op_ldc_w, key_idx);
      emit_var_read_ref(&c, vr);
      uint16_t set_idx =
          cf_methodref(cf, "V6Object", "set", "(Ljava/lang/String;LV6Value;)V");
      op_emit2(main_m, op_invokevirtual, set_idx);
    }
    emit_aload(main_m, c.exports_slot);
    op_emit(main_m, op_areturn);
  }
  (void)exports_local_slot;
  main_m->max_locals = c.next_local_slot;

  compile_result r;
  r.ok = p.had_error ? 0 : 1;
  r.line = p.err_line;
  if (r.line < 1)
    r.line = 1;
  memcpy(r.message, p.err_msg, sizeof(r.message));
  free(src);
  return r;
}

static int entry_is_cjs(const char* src) {
  lexer lx;
  lex_init(&lx, src);
  lx.auto_regex = 1;
  tok t = lex_next(&lx);
  while (t.kind != tok_eof) {
    if (t.kind == tok_kw_export)
      return 0;
    t = lex_next(&lx);
  }
  return 1;
}

compile_result compile_program(const char* src, class_file* cf,
                               const char* entry_path, module_ctx* modctx) {
  char dir[v6_max_path];
  if (entry_path) {
    path_dirname(entry_path, dir, sizeof(dir));
  } else {
    snprintf(dir, sizeof(dir), ".");
  }
  g_entry_script_path = entry_path ? entry_path : "";
  if (modctx)
    module_ctx_init(modctx);
  return compile_module_impl(cf, "Main", src, dir, modctx, 1,
                             entry_is_cjs(src));
}
