#include "v6/parser.h"

#include "v6/module.h"
#include "v6/internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v6/literal.h"
#include "v6/closures.h"
#include "v6/expr.h"
#include "v6/scope.h"

static int hex_val(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

static size_t encode_utf8(uint32_t cp, char* out) {
  if (cp == 0) {
    out[0] = (char)0xc0;
    out[1] = (char)0x80;
    return 2;
  }
  if (cp <= 0x7f) {
    out[0] = (char)cp;
    return 1;
  }
  if (cp <= 0x7ff) {
    out[0] = (char)(0xc0 | (cp >> 6));
    out[1] = (char)(0x80 | (cp & 0x3f));
    return 2;
  }
  out[0] = (char)(0xe0 | (cp >> 12));
  out[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
  out[2] = (char)(0x80 | (cp & 0x3f));
  return 3;
}

char* decode_string(tok t) {
  char* buf = malloc(t.len > 0 ? t.len : 1);
  size_t j = 0;
  for (size_t i = 1; i + 1 < t.len; i++) {
    char ch = t.start[i];
    if (ch == '\\' && i + 2 < t.len) {
      i++;
      char e = t.start[i];
      if (e == 'n') {
        buf[j++] = '\n';
      } else if (e == 't') {
        buf[j++] = '\t';
      } else if (e == 'r') {
        buf[j++] = '\r';
      } else if (e == '0') {
        j += encode_utf8(0, buf + j);
      } else if (e == 'u' && i + 4 < t.len) {
        int h0 = hex_val(t.start[i + 1]);
        int h1 = hex_val(t.start[i + 2]);
        int h2 = hex_val(t.start[i + 3]);
        int h3 = hex_val(t.start[i + 4]);
        if (h0 >= 0 && h1 >= 0 && h2 >= 0 && h3 >= 0) {
          uint32_t cp = (uint32_t)((h0 << 12) | (h1 << 8) | (h2 << 4) | h3);
          j += encode_utf8(cp, buf + j);
          i += 4;
        } else {
          buf[j++] = e;
        }
      } else if (e == 'x' && i + 2 < t.len) {
        int h0 = hex_val(t.start[i + 1]);
        int h1 = hex_val(t.start[i + 2]);
        if (h0 >= 0 && h1 >= 0) {
          uint32_t cp = (uint32_t)((h0 << 4) | h1);
          j += encode_utf8(cp, buf + j);
          i += 2;
        } else {
          buf[j++] = e;
        }
      } else {
        buf[j++] = e;
      }
    } else {
      buf[j++] = ch;
    }
  }
  buf[j] = '\0';
  return buf;
}

static char* decode_raw_chunk(const char* start, size_t len) {
  char* buf = malloc(len + 1);
  size_t j = 0;
  for (size_t i = 0; i < len; i++) {
    char ch = start[i];
    if (ch == '\\' && i + 1 < len) {
      i++;
      char e = start[i];
      if (e == 'n') {
        buf[j++] = '\n';
      } else if (e == 't') {
        buf[j++] = '\t';
      } else if (e == 'r') {
        buf[j++] = '\r';
      } else if (e == '`') {
        buf[j++] = '`';
      } else if (e == '$') {
        buf[j++] = '$';
      } else if (e == 'u' && i + 4 < len) {
        int h0 = hex_val(start[i + 1]);
        int h1 = hex_val(start[i + 2]);
        int h2 = hex_val(start[i + 3]);
        int h3 = hex_val(start[i + 4]);
        if (h0 >= 0 && h1 >= 0 && h2 >= 0 && h3 >= 0) {
          uint32_t cp = (uint32_t)((h0 << 12) | (h1 << 8) | (h2 << 4) | h3);
          j += encode_utf8(cp, buf + j);
          i += 4;
        } else {
          buf[j++] = e;
        }
      } else {
        buf[j++] = e;
      }
    } else {
      buf[j++] = ch;
    }
  }
  buf[j] = '\0';
  return buf;
}

static char* raw_chunk_copy(const char* start, size_t len) {
  char* buf = malloc(len + 1);
  memcpy(buf, start, len);
  buf[len] = '\0';
  return buf;
}

char* dup_tok(tok t) {
  char* s = malloc(t.len + 1);
  memcpy(s, t.start, t.len);
  s[t.len] = '\0';
  return s;
}

static char* format_num_key(double n) {
  char buf[64];
  if (n == (double)(long long)n) {
    snprintf(buf, sizeof(buf), "%lld", (long long)n);
  } else {
    snprintf(buf, sizeof(buf), "%g", n);
  }
  char* s = malloc(strlen(buf) + 1);
  strcpy(s, buf);
  return s;
}

static void parse_object_entry(parser* p, compiler* c) {
  if (match(p, tok_ellipsis)) {
    op_emit(c->m, op_dup);
    parse_expr(p, c);
    uint16_t spread_idx =
        cf_methodref(c->cf, "V6Object", "spreadFrom", "(LV6Value;)V");
    op_emit2(c->m, op_invokevirtual, spread_idx);
    return;
  }

  int is_async = 0;
  if (check(p, tok_kw_async)) {
    lexer save_lex = p->lex;
    tok save_cur = p->cur;
    tok save_prev = p->prev;
    advance(p);
    if (check(p, tok_colon) || check(p, tok_lparen) || check(p, tok_comma) ||
        check(p, tok_rbrace)) {
      p->lex = save_lex;
      p->cur = save_cur;
      p->prev = save_prev;
    } else {
      is_async = 1;
    }
  }

  int is_gen = match(p, tok_star);

  int is_getter = 0, is_setter = 0;
  if (!is_gen && !is_async && (check(p, tok_kw_get) || check(p, tok_kw_set))) {
    tok_kind modifier = p->cur.kind;
    lexer save_lex = p->lex;
    tok save_cur = p->cur;
    tok save_prev = p->prev;
    advance(p);
    if (check(p, tok_colon) || check(p, tok_lparen) || check(p, tok_comma) ||
        check(p, tok_rbrace)) {
      p->lex = save_lex;
      p->cur = save_cur;
      p->prev = save_prev;
    } else {
      is_getter = modifier == tok_kw_get;
      is_setter = modifier == tok_kw_set;
    }
  }

  int computed = 0;
  int shorthand_ok = 0;
  tok ident_tok;
  char* key = NULL;
  if (match(p, tok_lbracket)) {
    computed = 1;
  } else if (check(p, tok_str)) {
    tok t = p->cur;
    advance(p);
    key = decode_string(t);
  } else if (check(p, tok_num)) {
    tok t = p->cur;
    advance(p);
    key = format_num_key(t.num);
  } else if (check(p, tok_ident)) {
    ident_tok = p->cur;
    advance(p);
    key = dup_tok(ident_tok);
    shorthand_ok = 1;
  } else if (match_property_name(p)) {
    key = dup_tok(p->prev);
  } else {
    return;
  }

  uint16_t key_idx = 0;
  uint16_t computed_key_slot = 0;
  if (computed) {
    parse_expr(p, c);
    uint16_t tostring_idx =
        cf_methodref(c->cf, "V6Value", "toString", "()Ljava/lang/String;");
    op_emit2(c->m, op_invokevirtual, tostring_idx);
    if (!expect(p, tok_rbracket))
      return;
    computed_key_slot = c->next_local_slot++;
    emit_astore(c->m, computed_key_slot);
  } else {
    key_idx = cf_string(c->cf, key);
    free(key);
  }

  if (is_getter || is_setter) {
    if (!check(p, tok_lparen)) {
      error_at(p, "expected '('");
      return;
    }
    op_emit(c->m, op_dup);
    if (computed)
      emit_aload(c->m, computed_key_slot);
    else
      op_emit2(c->m, op_ldc_w, key_idx);
    compile_closure_value(p, c, 0, 1, NULL);
    uint16_t ascall_idx =
        cf_methodref(c->cf, "V6Value", "asCallable", "()LV6Callable;");
    op_emit2(c->m, op_invokevirtual, ascall_idx);
    uint16_t def_idx = cf_methodref(c->cf, "V6Object",
                                    is_getter ? "defineGetter" : "defineSetter",
                                    "(Ljava/lang/String;LV6Callable;)V");
    op_emit2(c->m, op_invokevirtual, def_idx);
    return;
  }

  op_emit(c->m, op_dup);
  if (computed)
    emit_aload(c->m, computed_key_slot);
  else
    op_emit2(c->m, op_ldc_w, key_idx);

  if (check(p, tok_lparen)) {
    compile_closure_value(p, c, 0, 1, NULL);
    if (is_gen && is_async) {
      error_at(p, "async generator methods are not supported");
      return;
    } else if (is_gen) {
      emit_wrap_generator(c);
    } else if (is_async) {
      emit_wrap_async(c);
    }
  } else if (match(p, tok_colon)) {
    parse_expr(p, c);
  } else if (shorthand_ok && !computed) {
    var_ref vr = resolve_var(c, ident_tok.start, ident_tok.len);
    if (vr.kind == var_not_found) {
      error_at(p, "undeclared variable");
      return;
    }
    emit_var_read_ref(c, vr);
  } else {
    error_at(p, "expected ':'");
    return;
  }
  uint16_t set_idx =
      cf_methodref(c->cf, "V6Object", "set", "(Ljava/lang/String;LV6Value;)V");
  op_emit2(c->m, op_invokevirtual, set_idx);
}

void parse_object_literal(parser* p, compiler* c) {
  uint16_t ctor_idx = cf_methodref(c->cf, "V6Object", "<init>", "()V");
  op_emit2(c->m, op_new, object_class(c->cf));
  op_emit(c->m, op_dup);
  op_emit2(c->m, op_invokespecial, ctor_idx);

  if (!check(p, tok_rbrace)) {
    parse_object_entry(p, c);
    while (match(p, tok_comma)) {
      if (check(p, tok_rbrace))
        break;
      parse_object_entry(p, c);
    }
  }
  expect(p, tok_rbrace);

  emit_box_object_ref(c);
}

void parse_array_literal(parser* p, compiler* c) {
  uint16_t array_cls = cf_class(c->cf, "V6Array");
  uint16_t ctor_idx = cf_methodref(c->cf, "V6Array", "<init>", "()V");
  op_emit2(c->m, op_new, array_cls);
  op_emit(c->m, op_dup);
  op_emit2(c->m, op_invokespecial, ctor_idx);

  if (!check(p, tok_rbracket)) {
    uint16_t push_idx = cf_methodref(c->cf, "V6Object", "push", "(LV6Value;)V");
    uint16_t pushall_idx =
        cf_methodref(c->cf, "V6Object", "pushAll", "(LV6Value;)V");
    for (;;) {
      if (check(p, tok_rbracket))
        break;
      op_emit(c->m, op_dup);
      if (match(p, tok_ellipsis)) {
        parse_expr(p, c);
        op_emit2(c->m, op_invokevirtual, pushall_idx);
      } else {
        parse_expr(p, c);
        op_emit2(c->m, op_invokevirtual, push_idx);
      }
      if (!match(p, tok_comma))
        break;
    }
  }
  expect(p, tok_rbracket);

  emit_box_object_ref(c);
}

void emit_string_value(compiler* c, const char* s) {
  uint16_t str_idx = cf_string(c->cf, s);
  op_emit2(c->m, op_new, value_class(c->cf));
  op_emit(c->m, op_dup);
  op_emit(c->m, op_iconst_5);
  op_emit(c->m, op_dconst_0);
  op_emit2(c->m, op_ldc_w, str_idx);
  op_emit2(c->m, op_invokespecial, value_ctor(c->cf));
}

void emit_throw_reference_error(compiler* c, const char* name, size_t len) {
  char msg[256];
  snprintf(msg, sizeof(msg), "ReferenceError: %.*s is not defined", (int)len,
           name);
  emit_string_value(c, msg);
  uint16_t throw_cls = cf_class(c->cf, "V6Throw");
  uint16_t throw_ctor =
      cf_methodref(c->cf, "V6Throw", "<init>", "(LV6Value;)V");
  op_emit2(c->m, op_new, throw_cls);
  op_emit(c->m, op_dup_x1);
  op_emit(c->m, op_swap);
  op_emit2(c->m, op_invokespecial, throw_ctor);
  op_emit(c->m, op_athrow);
}

void parse_template_literal(parser* p, compiler* c) {
  tok t = p->cur;
  advance(p);
  const char* s = t.start;
  size_t len = t.len;

  emit_string_value(c, "");
  uint16_t add_idx =
      cf_methodref(c->cf, "V6Value", "add", "(LV6Value;LV6Value;)LV6Value;");

  size_t i = 1;
  size_t chunk_start = 1;
  while (i < len - 1) {
    if (s[i] == '\\' && i + 1 < len - 1) {
      i += 2;
      continue;
    }
    if (s[i] == '$' && i + 1 < len - 1 && s[i + 1] == '{') {
      if (i > chunk_start) {
        char* dec = decode_raw_chunk(s + chunk_start, i - chunk_start);
        emit_string_value(c, dec);
        free(dec);
        op_emit2(c->m, op_invokestatic, add_idx);
      }

      size_t expr_start = i + 2;
      int depth = 1;
      size_t j = expr_start;
      while (j < len - 1 && depth > 0) {
        if (s[j] == '{') {
          depth++;
        } else if (s[j] == '}') {
          depth--;
          if (depth == 0)
            break;
        } else if (s[j] == '"' || s[j] == '\'') {
          char q = s[j];
          j++;
          while (j < len - 1 && s[j] != q) {
            if (s[j] == '\\' && j + 1 < len - 1)
              j++;
            j++;
          }
        }
        j++;
      }

      parser ep;
      parser_init(&ep, s + expr_start);
      parse_expr(&ep, c);
      op_emit2(c->m, op_invokestatic, add_idx);

      i = j + 1;
      chunk_start = i;
      continue;
    }
    i++;
  }

  if (i > chunk_start) {
    char* dec = decode_raw_chunk(s + chunk_start, i - chunk_start);
    emit_string_value(c, dec);
    free(dec);
    op_emit2(c->m, op_invokestatic, add_idx);
  }
}

void emit_tagged_template_call(parser* p, compiler* c) {
  uint16_t fn_slot = c->next_local_slot++;
  uint16_t this_slot = c->next_local_slot++;
  emit_astore(c->m, fn_slot);
  emit_astore(c->m, this_slot);

  tok t = p->cur;
  advance(p);
  const char* s = t.start;
  size_t len = t.len;

  uint16_t arr_cls = cf_class(c->cf, "V6Array");
  uint16_t arr_ctor = cf_methodref(c->cf, "V6Array", "<init>", "()V");
  uint16_t push_idx = cf_methodref(c->cf, "V6Object", "push", "(LV6Value;)V");
  uint16_t set_idx =
      cf_methodref(c->cf, "V6Object", "set", "(Ljava/lang/String;LV6Value;)V");
  uint16_t tovalarr_idx =
      cf_methodref(c->cf, "V6Object", "toValueArray", "()[LV6Value;");

  op_emit2(c->m, op_new, arr_cls);
  op_emit(c->m, op_dup);
  op_emit2(c->m, op_invokespecial, arr_ctor);
  uint16_t strings_slot = c->next_local_slot++;
  emit_astore(c->m, strings_slot);

  op_emit2(c->m, op_new, arr_cls);
  op_emit(c->m, op_dup);
  op_emit2(c->m, op_invokespecial, arr_ctor);
  uint16_t raw_slot = c->next_local_slot++;
  emit_astore(c->m, raw_slot);

  op_emit2(c->m, op_new, arr_cls);
  op_emit(c->m, op_dup);
  op_emit2(c->m, op_invokespecial, arr_ctor);
  uint16_t callargs_slot = c->next_local_slot++;
  emit_astore(c->m, callargs_slot);

  emit_aload(c->m, callargs_slot);
  emit_aload(c->m, strings_slot);
  emit_box_object_ref(c);
  op_emit2(c->m, op_invokevirtual, push_idx);

  size_t i = 1;
  size_t chunk_start = 1;
  while (i < len - 1) {
    if (s[i] == '\\' && i + 1 < len - 1) {
      i += 2;
      continue;
    }
    if (s[i] == '$' && i + 1 < len - 1 && s[i + 1] == '{') {
      char* dec = decode_raw_chunk(s + chunk_start, i - chunk_start);
      emit_aload(c->m, strings_slot);
      emit_string_value(c, dec);
      free(dec);
      op_emit2(c->m, op_invokevirtual, push_idx);

      char* raw = raw_chunk_copy(s + chunk_start, i - chunk_start);
      emit_aload(c->m, raw_slot);
      emit_string_value(c, raw);
      free(raw);
      op_emit2(c->m, op_invokevirtual, push_idx);

      size_t expr_start = i + 2;
      int depth = 1;
      size_t j = expr_start;
      while (j < len - 1 && depth > 0) {
        if (s[j] == '{') {
          depth++;
        } else if (s[j] == '}') {
          depth--;
          if (depth == 0)
            break;
        } else if (s[j] == '"' || s[j] == '\'') {
          char q = s[j];
          j++;
          while (j < len - 1 && s[j] != q) {
            if (s[j] == '\\' && j + 1 < len - 1)
              j++;
            j++;
          }
        }
        j++;
      }

      parser ep;
      parser_init(&ep, s + expr_start);
      emit_aload(c->m, callargs_slot);
      parse_expr(&ep, c);
      op_emit2(c->m, op_invokevirtual, push_idx);

      i = j + 1;
      chunk_start = i;
      continue;
    }
    i++;
  }

  char* dec = decode_raw_chunk(s + chunk_start, i - chunk_start);
  emit_aload(c->m, strings_slot);
  emit_string_value(c, dec);
  free(dec);
  op_emit2(c->m, op_invokevirtual, push_idx);

  char* raw = raw_chunk_copy(s + chunk_start, i - chunk_start);
  emit_aload(c->m, raw_slot);
  emit_string_value(c, raw);
  free(raw);
  op_emit2(c->m, op_invokevirtual, push_idx);

  uint16_t raw_str = cf_string(c->cf, "raw");
  emit_aload(c->m, strings_slot);
  op_emit2(c->m, op_ldc_w, raw_str);
  emit_aload(c->m, raw_slot);
  emit_box_object_ref(c);
  op_emit2(c->m, op_invokevirtual, set_idx);

  emit_aload(c->m, fn_slot);
  emit_aload(c->m, this_slot);
  emit_aload(c->m, callargs_slot);
  op_emit2(c->m, op_invokevirtual, tovalarr_idx);
  uint16_t call_idx =
      cf_methodref(c->cf, "V6Value", "call", "(LV6Value;[LV6Value;)LV6Value;");
  op_emit2(c->m, op_invokevirtual, call_idx);
}

static void decode_regex(tok t, char** out_source, char** out_flags) {
  const char* s = t.start;
  size_t i = 1;
  int in_class = 0;
  while (i < t.len && (in_class || s[i] != '/')) {
    if (s[i] == '\\' && i + 1 < t.len) {
      i += 2;
      continue;
    }
    if (s[i] == '[')
      in_class = 1;
    else if (s[i] == ']')
      in_class = 0;
    i++;
  }
  size_t source_len = i - 1;
  char* source = malloc(source_len + 1);
  memcpy(source, s + 1, source_len);
  source[source_len] = '\0';
  size_t flags_start = i + 1;
  size_t flags_len = (flags_start <= t.len) ? t.len - flags_start : 0;
  char* flags = malloc(flags_len + 1);
  if (flags_len)
    memcpy(flags, s + flags_start, flags_len);
  flags[flags_len] = '\0';
  *out_source = source;
  *out_flags = flags;
}

void emit_regex_literal(parser* p, compiler* c, tok t) {
  char* source;
  char* flags;
  decode_regex(t, &source, &flags);

  var_ref vr = resolve_var(c, "RegExp", 6);
  if (vr.kind == var_not_found) {
    error_at(p, "RegExp is not defined");
    free(source);
    free(flags);
    return;
  }
  emit_var_read_ref(c, vr);
  uint16_t cls_val_slot = c->next_local_slot++;
  emit_astore(c->m, cls_val_slot);

  emit_iconst(c->m, 2);
  op_emit2(c->m, op_anewarray, value_class(c->cf));
  op_emit(c->m, op_dup);
  emit_iconst(c->m, 0);
  emit_string_value(c, source);
  op_emit(c->m, op_aastore);
  op_emit(c->m, op_dup);
  emit_iconst(c->m, 1);
  emit_string_value(c, flags);
  op_emit(c->m, op_aastore);
  uint16_t args_slot = c->next_local_slot++;
  emit_astore(c->m, args_slot);

  emit_aload(c->m, cls_val_slot);
  emit_aload(c->m, args_slot);
  uint16_t construct_idx = cf_methodref(c->cf, "V6Value", "construct",
                                        "(LV6Value;[LV6Value;)LV6Value;");
  op_emit2(c->m, op_invokestatic, construct_idx);

  free(source);
  free(flags);
}
