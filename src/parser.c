#include "v6/parser.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void advance(parser* p) {
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

static void error_at(parser* p, const char* msg) {
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

static int check(parser* p, tok_kind k) {
  return p->cur.kind == k;
}

static int match(parser* p, tok_kind k) {
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

static int expect(parser* p, tok_kind k) {
  if (match(p, k))
    return 1;
  char msg[64];
  snprintf(msg, sizeof(msg), "expected '%s'", tok_name(k));
  error_at(p, msg);
  return 0;
}

static uint16_t value_class(class_file* cf) {
  return cf_class(cf, "V6Value");
}

static uint16_t value_ctor(class_file* cf) {
  return cf_methodref(cf, "V6Value", "<init>", "(IDLjava/lang/Object;)V");
}

static void emit_dstore(method* m, uint16_t slot) {
  switch (slot) {
  case 0:
    op_emit(m, op_dstore_0);
    return;
  case 1:
    op_emit(m, op_dstore_1);
    return;
  case 2:
    op_emit(m, op_dstore_2);
    return;
  case 3:
    op_emit(m, op_dstore_3);
    return;
  default:
    op_emit1(m, op_dstore, (uint8_t)slot);
    return;
  }
}

static void emit_dload(method* m, uint16_t slot) {
  switch (slot) {
  case 0:
    op_emit(m, op_dload_0);
    return;
  case 1:
    op_emit(m, op_dload_1);
    return;
  case 2:
    op_emit(m, op_dload_2);
    return;
  case 3:
    op_emit(m, op_dload_3);
    return;
  default:
    op_emit1(m, op_dload, (uint8_t)slot);
    return;
  }
}

static void emit_aload(method* m, uint16_t slot) {
  switch (slot) {
  case 0:
    op_emit(m, op_aload_0);
    return;
  case 1:
    op_emit(m, op_aload_1);
    return;
  case 2:
    op_emit(m, op_aload_2);
    return;
  case 3:
    op_emit(m, op_aload_3);
    return;
  default:
    op_emit1(m, op_aload, (uint8_t)slot);
    return;
  }
}

static void emit_astore(method* m, uint16_t slot) {
  switch (slot) {
  case 0:
    op_emit(m, op_astore_0);
    return;
  case 1:
    op_emit(m, op_astore_1);
    return;
  case 2:
    op_emit(m, op_astore_2);
    return;
  case 3:
    op_emit(m, op_astore_3);
    return;
  default:
    op_emit1(m, op_astore, (uint8_t)slot);
    return;
  }
}

static void emit_box_const(class_file* cf, method* m, uint8_t tag_op,
                           uint8_t num_op) {
  op_emit2(m, op_new, value_class(cf));
  op_emit(m, op_dup);
  op_emit(m, tag_op);
  op_emit(m, num_op);
  op_emit(m, op_aconst_null);
  op_emit2(m, op_invokespecial, value_ctor(cf));
}

static void emit_box_tag(compiler* c, uint8_t tag_op) {
  emit_dstore(c->m, c->scratch_slot);
  op_emit2(c->m, op_new, value_class(c->cf));
  op_emit(c->m, op_dup);
  op_emit(c->m, tag_op);
  emit_dload(c->m, c->scratch_slot);
  op_emit(c->m, op_aconst_null);
  op_emit2(c->m, op_invokespecial, value_ctor(c->cf));
}

static void emit_to_number(compiler* c) {
  uint16_t idx = cf_methodref(c->cf, "V6Value", "toNumber", "()D");
  op_emit2(c->m, op_invokevirtual, idx);
}

static void emit_truthy(compiler* c) {
  uint16_t idx = cf_methodref(c->cf, "V6Value", "truthy", "()Z");
  op_emit2(c->m, op_invokevirtual, idx);
}

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

static char* decode_string(tok t) {
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

static char* dup_tok(tok t) {
  char* s = malloc(t.len + 1);
  memcpy(s, t.start, t.len);
  s[t.len] = '\0';
  return s;
}

static local* find_local_entry(compiler* c, const char* name, size_t len) {
  for (int i = c->local_count - 1; i >= 0; i--) {
    if (c->locals[i].dead)
      continue;
    if (c->locals[i].len == len && memcmp(c->locals[i].name, name, len) == 0)
      return &c->locals[i];
  }
  return NULL;
}

static int find_slot(compiler* c, const char* name, size_t len, uint16_t* out) {
  for (int i = 0; i < c->param_count; i++) {
    if (c->params[i].len == len && memcmp(c->params[i].name, name, len) == 0) {
      *out = (uint16_t)i;
      return 1;
    }
  }
  local* le = find_local_entry(c, name, len);
  if (le) {
    *out = le->slot;
    return 1;
  }
  return 0;
}

static void add_local(compiler* c, tok name, uint16_t slot, int is_var,
                      int is_const) {
  c->locals[c->local_count].name = name.start;
  c->locals[c->local_count].len = name.len;
  c->locals[c->local_count].slot = slot;
  c->locals[c->local_count].is_var = is_var;
  c->locals[c->local_count].is_const = is_const;
  c->locals[c->local_count].dead = 0;
  c->local_count++;
}

static fn_entry* find_fn(compiler* c, const char* name, size_t len) {
  for (size_t i = 0; i < c->fn_count; i++) {
    if (c->fns[i].len == len && memcmp(c->fns[i].name, name, len) == 0)
      return &c->fns[i];
  }
  return NULL;
}

static void build_descriptor(char* buf, int param_count) {
  char* p = buf;
  *p++ = '(';
  for (int i = 0; i < param_count; i++) {
    memcpy(p, "LV6Value;", 9);
    p += 9;
  }
  *p++ = ')';
  memcpy(p, "LV6Value;", 9);
  p += 9;
  *p = '\0';
}

static void build_print_fn(compiler* c) {
  method* m = cf_method(c->cf, acc_static, "print", "(LV6Value;)LV6Value;");
  m->max_stack = 8;
  m->max_locals = 1;

  uint16_t out_idx =
      cf_fieldref(c->cf, "java/lang/System", "out", "Ljava/io/PrintStream;");
  uint16_t tostring_idx =
      cf_methodref(c->cf, "V6Value", "toString", "()Ljava/lang/String;");
  uint16_t println_idx = cf_methodref(c->cf, "java/io/PrintStream", "println",
                                      "(Ljava/lang/String;)V");

  op_emit2(m, op_getstatic, out_idx);
  op_emit(m, op_aload_0);
  op_emit2(m, op_invokevirtual, tostring_idx);
  op_emit2(m, op_invokevirtual, println_idx);
  emit_box_const(c->cf, m, op_iconst_3, op_dconst_0);
  op_emit(m, op_areturn);

  c->fns[c->fn_count].name = "print";
  c->fns[c->fn_count].len = 5;
  c->fns[c->fn_count].param_count = 1;
  c->fn_count++;
}

static void parse_expr(parser* p, compiler* c);
static void parse_object_literal(parser* p, compiler* c);
static void parse_array_literal(parser* p, compiler* c);
static void parse_primary(parser* p, compiler* c);

static uint16_t object_class(class_file* cf) {
  return cf_class(cf, "V6Object");
}

static void emit_deref_to_object(compiler* c) {
  uint16_t ref_idx =
      cf_methodref(c->cf, "V6Value", "ref", "()Ljava/lang/Object;");
  op_emit2(c->m, op_invokevirtual, ref_idx);
  op_emit2(c->m, op_checkcast, object_class(c->cf));
}

static void emit_box_object_ref(compiler* c) {
  emit_astore(c->m, c->scratch_slot);
  op_emit2(c->m, op_new, value_class(c->cf));
  op_emit(c->m, op_dup);
  op_emit(c->m, op_iconst_4);
  op_emit(c->m, op_dconst_0);
  emit_aload(c->m, c->scratch_slot);
  op_emit2(c->m, op_invokespecial, value_ctor(c->cf));
}

static void parse_object_entry(parser* p, compiler* c) {
  char* key;
  if (check(p, tok_str)) {
    tok t = p->cur;
    advance(p);
    key = decode_string(t);
  } else if (expect(p, tok_ident)) {
    key = dup_tok(p->prev);
  } else {
    return;
  }
  if (!expect(p, tok_colon)) {
    free(key);
    return;
  }

  uint16_t key_idx = cf_string(c->cf, key);
  free(key);

  op_emit(c->m, op_dup);
  op_emit2(c->m, op_ldc_w, key_idx);
  parse_expr(p, c);
  uint16_t set_idx =
      cf_methodref(c->cf, "V6Object", "set", "(Ljava/lang/String;LV6Value;)V");
  op_emit2(c->m, op_invokevirtual, set_idx);
}

static void parse_object_literal(parser* p, compiler* c) {
  uint16_t ctor_idx = cf_methodref(c->cf, "V6Object", "<init>", "(Z)V");
  op_emit2(c->m, op_new, object_class(c->cf));
  op_emit(c->m, op_dup);
  op_emit(c->m, op_iconst_0);
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

static void parse_array_literal(parser* p, compiler* c) {
  uint16_t ctor_idx = cf_methodref(c->cf, "V6Object", "<init>", "(Z)V");
  op_emit2(c->m, op_new, object_class(c->cf));
  op_emit(c->m, op_dup);
  op_emit(c->m, op_iconst_1);
  op_emit2(c->m, op_invokespecial, ctor_idx);

  if (!check(p, tok_rbracket)) {
    uint16_t push_idx = cf_methodref(c->cf, "V6Object", "push", "(LV6Value;)V");
    op_emit(c->m, op_dup);
    parse_expr(p, c);
    op_emit2(c->m, op_invokevirtual, push_idx);
    while (match(p, tok_comma)) {
      if (check(p, tok_rbracket))
        break;
      op_emit(c->m, op_dup);
      parse_expr(p, c);
      op_emit2(c->m, op_invokevirtual, push_idx);
    }
  }
  expect(p, tok_rbracket);

  emit_box_object_ref(c);
}

static void parse_postfix(parser* p, compiler* c) {
  parse_primary(p, c);
  for (;;) {
    if (check(p, tok_dot) || check(p, tok_lbracket)) {
      emit_deref_to_object(c);
      if (match(p, tok_dot)) {
        if (!expect(p, tok_ident))
          return;
        char* key = dup_tok(p->prev);
        uint16_t key_idx = cf_string(c->cf, key);
        free(key);
        op_emit2(c->m, op_ldc_w, key_idx);
      } else {
        advance(p);
        parse_expr(p, c);
        uint16_t tostring_idx =
            cf_methodref(c->cf, "V6Value", "toString", "()Ljava/lang/String;");
        op_emit2(c->m, op_invokevirtual, tostring_idx);
        expect(p, tok_rbracket);
      }

      if (match(p, tok_assign)) {
        parse_expr(p, c);
        op_emit(c->m, op_dup_x2);
        uint16_t set_idx = cf_methodref(c->cf, "V6Object", "set",
                                        "(Ljava/lang/String;LV6Value;)V");
        op_emit2(c->m, op_invokevirtual, set_idx);
        return;
      }

      uint16_t get_idx = cf_methodref(c->cf, "V6Object", "get",
                                      "(Ljava/lang/String;)LV6Value;");
      op_emit2(c->m, op_invokevirtual, get_idx);
    } else {
      break;
    }
  }
}

static void parse_call(parser* p, compiler* c, tok name) {
  fn_entry* fn = find_fn(c, name.start, name.len);
  if (!fn) {
    error_at(p, "call to undeclared function");
    return;
  }

  int argc = 0;
  if (!check(p, tok_rparen)) {
    parse_expr(p, c);
    argc++;
    while (match(p, tok_comma)) {
      parse_expr(p, c);
      argc++;
    }
  }
  if (!expect(p, tok_rparen))
    return;

  if (argc != fn->param_count) {
    error_at(p, "wrong number of arguments");
    return;
  }

  char desc[16 + 9 * v6_max_params];
  build_descriptor(desc, fn->param_count);
  uint16_t idx = cf_methodref(c->cf, "Main", fn->name, desc);
  op_emit2(c->m, op_invokestatic, idx);
}

static void parse_primary(parser* p, compiler* c) {
  if (match(p, tok_num)) {
    uint16_t idx = cf_double(c->cf, p->prev.num);
    op_emit2(c->m, op_new, value_class(c->cf));
    op_emit(c->m, op_dup);
    op_emit(c->m, op_iconst_0);
    op_emit2(c->m, op_ldc2_w, idx);
    op_emit(c->m, op_aconst_null);
    op_emit2(c->m, op_invokespecial, value_ctor(c->cf));
    return;
  }

  if (match(p, tok_kw_true)) {
    emit_box_const(c->cf, c->m, op_iconst_1, op_dconst_1);
    return;
  }

  if (match(p, tok_kw_false)) {
    emit_box_const(c->cf, c->m, op_iconst_1, op_dconst_0);
    return;
  }

  if (match(p, tok_kw_null)) {
    emit_box_const(c->cf, c->m, op_iconst_2, op_dconst_0);
    return;
  }

  if (match(p, tok_kw_undefined)) {
    emit_box_const(c->cf, c->m, op_iconst_3, op_dconst_0);
    return;
  }

  if (check(p, tok_str)) {
    tok t = p->cur;
    advance(p);
    char* s = decode_string(t);
    uint16_t str_idx = cf_string(c->cf, s);
    free(s);

    op_emit2(c->m, op_new, value_class(c->cf));
    op_emit(c->m, op_dup);
    op_emit(c->m, op_iconst_5);
    op_emit(c->m, op_dconst_0);
    op_emit2(c->m, op_ldc_w, str_idx);
    op_emit2(c->m, op_invokespecial, value_ctor(c->cf));
    return;
  }

  if (check(p, tok_plus_plus) || check(p, tok_minus_minus)) {
    int is_inc = check(p, tok_plus_plus);
    advance(p);
    if (!expect(p, tok_ident))
      return;
    tok name = p->prev;
    uint16_t slot;
    if (!find_slot(c, name.start, name.len, &slot)) {
      error_at(p, "undeclared variable");
      return;
    }
    local* le = find_local_entry(c, name.start, name.len);
    if (le && le->is_const) {
      error_at(p, "assignment to constant variable");
      return;
    }
    emit_aload(c->m, slot);
    emit_to_number(c);
    op_emit(c->m, op_dconst_1);
    op_emit(c->m, is_inc ? op_dadd : op_dsub);
    emit_box_tag(c, op_iconst_0);
    op_emit(c->m, op_dup);
    emit_astore(c->m, slot);
    return;
  }

  if (match(p, tok_ident)) {
    tok name = p->prev;

    if (match(p, tok_lparen)) {
      parse_call(p, c, name);
      return;
    }

    if (check(p, tok_dot) || check(p, tok_lbracket)) {
      uint16_t slot;
      if (!find_slot(c, name.start, name.len, &slot)) {
        error_at(p, "undeclared variable");
        return;
      }
      emit_aload(c->m, slot);
      return;
    }

    if (check(p, tok_assign) || check(p, tok_plus_eq) ||
        check(p, tok_minus_eq) || check(p, tok_star_eq) ||
        check(p, tok_slash_eq) || check(p, tok_percent_eq)) {
      tok_kind op = p->cur.kind;
      advance(p);

      uint16_t slot;
      if (!find_slot(c, name.start, name.len, &slot)) {
        error_at(p, "undeclared variable");
        return;
      }
      local* le = find_local_entry(c, name.start, name.len);
      if (le && le->is_const) {
        error_at(p, "assignment to constant variable");
        return;
      }

      if (op == tok_assign) {
        parse_expr(p, c);
      } else if (op == tok_plus_eq) {
        emit_aload(c->m, slot);
        parse_expr(p, c);
        uint16_t idx = cf_methodref(c->cf, "V6Value", "add",
                                    "(LV6Value;LV6Value;)LV6Value;");
        op_emit2(c->m, op_invokestatic, idx);
      } else {
        emit_aload(c->m, slot);
        emit_to_number(c);
        parse_expr(p, c);
        emit_to_number(c);
        uint8_t bop = op == tok_minus_eq
                          ? op_dsub
                          : (op == tok_star_eq
                                 ? op_dmul
                                 : (op == tok_slash_eq ? op_ddiv : op_drem));
        op_emit(c->m, bop);
        emit_box_tag(c, op_iconst_0);
      }

      op_emit(c->m, op_dup);
      emit_astore(c->m, slot);
      return;
    }

    if (check(p, tok_plus_plus) || check(p, tok_minus_minus)) {
      int is_inc = check(p, tok_plus_plus);
      advance(p);
      uint16_t slot;
      if (!find_slot(c, name.start, name.len, &slot)) {
        error_at(p, "undeclared variable");
        return;
      }
      local* le = find_local_entry(c, name.start, name.len);
      if (le && le->is_const) {
        error_at(p, "assignment to constant variable");
        return;
      }
      emit_aload(c->m, slot);
      op_emit(c->m, op_dup);
      emit_to_number(c);
      op_emit(c->m, op_dconst_1);
      op_emit(c->m, is_inc ? op_dadd : op_dsub);
      emit_box_tag(c, op_iconst_0);
      emit_astore(c->m, slot);
      return;
    }

    uint16_t slot;
    if (!find_slot(c, name.start, name.len, &slot)) {
      error_at(p, "undeclared variable");
      return;
    }
    emit_aload(c->m, slot);
    return;
  }

  if (match(p, tok_lparen)) {
    parse_expr(p, c);
    if (!match(p, tok_rparen))
      error_at(p, "expected ')'");
    return;
  }

  if (match(p, tok_lbrace)) {
    parse_object_literal(p, c);
    return;
  }

  if (match(p, tok_lbracket)) {
    parse_array_literal(p, c);
    return;
  }

  error_at(p, "expected expression");
  advance(p);
}

static void parse_unary(parser* p, compiler* c) {
  if (match(p, tok_minus)) {
    parse_unary(p, c);
    emit_to_number(c);
    op_emit(c->m, op_dneg);
    emit_box_tag(c, op_iconst_0);
    return;
  }
  if (match(p, tok_bang)) {
    parse_unary(p, c);
    emit_truthy(c);
    op_emit(c->m, op_iconst_1);
    op_emit(c->m, op_ixor);
    op_emit(c->m, op_i2d);
    emit_box_tag(c, op_iconst_1);
    return;
  }
  parse_postfix(p, c);
}

static void parse_mul(parser* p, compiler* c) {
  parse_unary(p, c);
  while (check(p, tok_star) || check(p, tok_slash) || check(p, tok_percent)) {
    tok_kind k = p->cur.kind;
    advance(p);
    emit_to_number(c);
    parse_unary(p, c);
    emit_to_number(c);
    uint8_t op = k == tok_star ? op_dmul : (k == tok_slash ? op_ddiv : op_drem);
    op_emit(c->m, op);
    emit_box_tag(c, op_iconst_0);
  }
}

static void parse_add(parser* p, compiler* c) {
  parse_mul(p, c);
  while (check(p, tok_plus) || check(p, tok_minus)) {
    tok_kind k = p->cur.kind;
    advance(p);
    if (k == tok_plus) {
      parse_mul(p, c);
      uint16_t idx = cf_methodref(c->cf, "V6Value", "add",
                                  "(LV6Value;LV6Value;)LV6Value;");
      op_emit2(c->m, op_invokestatic, idx);
    } else {
      emit_to_number(c);
      parse_mul(p, c);
      emit_to_number(c);
      op_emit(c->m, op_dsub);
      emit_box_tag(c, op_iconst_0);
    }
  }
}

static void parse_cmp(parser* p, compiler* c) {
  parse_add(p, c);
  while (check(p, tok_lt) || check(p, tok_gt) || check(p, tok_le) ||
         check(p, tok_ge)) {
    tok_kind k = p->cur.kind;
    advance(p);
    parse_add(p, c);
    const char* name = k == tok_lt   ? "lt"
                       : k == tok_le ? "le"
                       : k == tok_gt ? "gt"
                                     : "ge";
    uint16_t idx =
        cf_methodref(c->cf, "V6Value", name, "(LV6Value;LV6Value;)Z");
    op_emit2(c->m, op_invokestatic, idx);
    op_emit(c->m, op_i2d);
    emit_box_tag(c, op_iconst_1);
  }
}

static void parse_eq(parser* p, compiler* c) {
  parse_cmp(p, c);
  while (check(p, tok_eq) || check(p, tok_neq) || check(p, tok_eq_strict) ||
         check(p, tok_neq_strict)) {
    tok_kind k = p->cur.kind;
    advance(p);
    parse_cmp(p, c);
    int strict = k == tok_eq_strict || k == tok_neq_strict;
    int negate = k == tok_neq || k == tok_neq_strict;
    uint16_t idx =
        cf_methodref(c->cf, "V6Value", strict ? "strictEquals" : "looseEquals",
                     "(LV6Value;LV6Value;)Z");
    op_emit2(c->m, op_invokestatic, idx);
    if (negate) {
      op_emit(c->m, op_iconst_1);
      op_emit(c->m, op_ixor);
    }
    op_emit(c->m, op_i2d);
    emit_box_tag(c, op_iconst_1);
  }
}

static void parse_and(parser* p, compiler* c) {
  parse_eq(p, c);
  while (match(p, tok_amp_amp)) {
    op_emit(c->m, op_dup);
    emit_truthy(c);
    size_t is_left_pos = op_pos(c->m);
    op_emit2(c->m, op_ifeq, 0);
    op_emit(c->m, op_pop);
    parse_eq(p, c);
    size_t end_pos = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(is_left_pos + 1),
              (uint16_t)(end_pos - is_left_pos));
  }
}

static void parse_or(parser* p, compiler* c) {
  parse_and(p, c);
  while (match(p, tok_pipe_pipe)) {
    op_emit(c->m, op_dup);
    emit_truthy(c);
    size_t is_left_pos = op_pos(c->m);
    op_emit2(c->m, op_ifne, 0);
    op_emit(c->m, op_pop);
    parse_and(p, c);
    size_t end_pos = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(is_left_pos + 1),
              (uint16_t)(end_pos - is_left_pos));
  }
}

static void parse_ternary(parser* p, compiler* c) {
  parse_or(p, c);
  if (match(p, tok_question)) {
    emit_truthy(c);
    size_t else_jump = op_pos(c->m);
    op_emit2(c->m, op_ifeq, 0);

    parse_expr(p, c);
    size_t end_jump = op_pos(c->m);
    op_emit2(c->m, op_goto, 0);

    size_t else_pos = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(else_jump + 1),
              (uint16_t)(else_pos - else_jump));

    expect(p, tok_colon);
    parse_expr(p, c);

    size_t end_pos = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(end_jump + 1), (uint16_t)(end_pos - end_jump));
  }
}

static void parse_expr(parser* p, compiler* c) {
  parse_ternary(p, c);
}

int compile_expr(parser* p, compiler* c) {
  parse_expr(p, c);
  return p->had_error ? -1 : 0;
}

static void parse_stmt(parser* p, compiler* c);
static void prescan_decls(compiler* c, const char* src, int hoist_functions);

static void parse_var_decl(parser* p, compiler* c, tok_kind kind) {
  if (!expect(p, tok_ident))
    return;
  tok name = p->prev;

  if (kind == tok_kw_var) {
    uint16_t slot;
    if (!find_slot(c, name.start, name.len, &slot)) {
      error_at(p, "internal: hoisted var missing");
      return;
    }
    if (match(p, tok_assign)) {
      parse_expr(p, c);
      emit_astore(c->m, slot);
    }
    return;
  }

  if (match(p, tok_assign))
    parse_expr(p, c);
  else
    emit_box_const(c->cf, c->m, op_iconst_3, op_dconst_0);

  uint16_t slot = c->next_local_slot++;
  emit_astore(c->m, slot);
  add_local(c, name, slot, 0, kind == tok_kw_const);
}

static void parse_block(parser* p, compiler* c) {
  int saved_count = c->local_count;
  while (!check(p, tok_rbrace) && !check(p, tok_eof))
    parse_stmt(p, c);
  expect(p, tok_rbrace);
  for (int i = saved_count; i < c->local_count; i++) {
    if (!c->locals[i].is_var)
      c->locals[i].dead = 1;
  }
}

static void parse_if(parser* p, compiler* c) {
  expect(p, tok_lparen);
  parse_expr(p, c);
  expect(p, tok_rparen);
  emit_truthy(c);

  size_t else_jump = op_pos(c->m);
  op_emit2(c->m, op_ifeq, 0);

  parse_stmt(p, c);

  size_t end_jump = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);

  size_t else_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(else_jump + 1), (uint16_t)(else_pos - else_jump));

  if (match(p, tok_kw_else))
    parse_stmt(p, c);

  size_t end_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(end_jump + 1), (uint16_t)(end_pos - end_jump));
}

static void push_loop(compiler* c, size_t continue_target) {
  c->breaks[c->break_depth].count = 0;
  c->break_depth++;
  c->continues[c->continue_depth] = continue_target;
  c->continue_depth++;
}

static void patch_breaks(compiler* c, size_t end_pos) {
  break_ctx* bc = &c->breaks[c->break_depth - 1];
  for (size_t i = 0; i < bc->count; i++)
    op_patch2(c->m, (uint16_t)(bc->jumps[i] + 1),
              (uint16_t)(end_pos - bc->jumps[i]));
}

static void pop_loop(compiler* c, size_t end_pos) {
  patch_breaks(c, end_pos);
  c->break_depth--;
  c->continue_depth--;
}

static void parse_while(parser* p, compiler* c) {
  size_t start_pos = op_pos(c->m);

  expect(p, tok_lparen);
  parse_expr(p, c);
  expect(p, tok_rparen);
  emit_truthy(c);

  size_t exit_jump = op_pos(c->m);
  op_emit2(c->m, op_ifeq, 0);

  push_loop(c, start_pos);
  parse_stmt(p, c);

  size_t back_jump = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);
  op_patch2(c->m, (uint16_t)(back_jump + 1), (uint16_t)(start_pos - back_jump));

  size_t end_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(exit_jump + 1), (uint16_t)(end_pos - exit_jump));
  pop_loop(c, end_pos);
}

static void parse_for(parser* p, compiler* c) {
  expect(p, tok_lparen);

  int saved_count = c->local_count;

  if (match(p, tok_kw_var) || match(p, tok_kw_let) || match(p, tok_kw_const)) {
    parse_var_decl(p, c, p->prev.kind);
  } else if (!check(p, tok_semi)) {
    parse_expr(p, c);
    op_emit(c->m, op_pop);
  }
  expect(p, tok_semi);

  size_t cond_pos = op_pos(c->m);
  int has_cond = !check(p, tok_semi);
  size_t exit_jump = 0;
  if (has_cond) {
    parse_expr(p, c);
    emit_truthy(c);
    exit_jump = op_pos(c->m);
    op_emit2(c->m, op_ifeq, 0);
  }
  expect(p, tok_semi);

  size_t body_jump = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);

  size_t inc_pos = op_pos(c->m);
  if (!check(p, tok_rparen)) {
    parse_expr(p, c);
    op_emit(c->m, op_pop);
  }
  size_t inc_to_cond = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);
  op_patch2(c->m, (uint16_t)(inc_to_cond + 1),
            (uint16_t)(cond_pos - inc_to_cond));

  expect(p, tok_rparen);

  size_t body_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(body_jump + 1), (uint16_t)(body_pos - body_jump));

  push_loop(c, inc_pos);
  parse_stmt(p, c);

  size_t body_to_inc = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);
  op_patch2(c->m, (uint16_t)(body_to_inc + 1),
            (uint16_t)(inc_pos - body_to_inc));

  size_t end_pos = op_pos(c->m);
  if (has_cond)
    op_patch2(c->m, (uint16_t)(exit_jump + 1), (uint16_t)(end_pos - exit_jump));
  pop_loop(c, end_pos);

  for (int i = saved_count; i < c->local_count; i++) {
    if (!c->locals[i].is_var)
      c->locals[i].dead = 1;
  }
}

static void parse_switch(parser* p, compiler* c) {
  expect(p, tok_lparen);
  parse_expr(p, c);
  expect(p, tok_rparen);
  expect(p, tok_lbrace);

  emit_astore(c->m, c->scratch_slot);

  c->breaks[c->break_depth].count = 0;
  c->break_depth++;

  size_t prev_case_jump = 0;
  int have_prev_case_jump = 0;
  int default_seen = 0;

  while (!check(p, tok_rbrace) && !check(p, tok_eof)) {
    if (match(p, tok_kw_case)) {
      emit_aload(c->m, c->scratch_slot);
      parse_expr(p, c);
      uint16_t idx = cf_methodref(c->cf, "V6Value", "strictEquals",
                                  "(LV6Value;LV6Value;)Z");
      op_emit2(c->m, op_invokestatic, idx);
      size_t skip_jump = op_pos(c->m);
      op_emit2(c->m, op_ifeq, 0);
      expect(p, tok_colon);

      if (have_prev_case_jump) {
        size_t here = op_pos(c->m);
        op_patch2(c->m, (uint16_t)(prev_case_jump + 1),
                  (uint16_t)(here - prev_case_jump));
        have_prev_case_jump = 0;
      }

      while (!check(p, tok_kw_case) && !check(p, tok_kw_default) &&
             !check(p, tok_rbrace) && !check(p, tok_eof))
        parse_stmt(p, c);

      prev_case_jump = op_pos(c->m);
      op_emit2(c->m, op_goto, 0);
      have_prev_case_jump = 1;

      size_t after = op_pos(c->m);
      op_patch2(c->m, (uint16_t)(skip_jump + 1), (uint16_t)(after - skip_jump));
    } else if (match(p, tok_kw_default)) {
      default_seen = 1;
      expect(p, tok_colon);

      if (have_prev_case_jump) {
        size_t here = op_pos(c->m);
        op_patch2(c->m, (uint16_t)(prev_case_jump + 1),
                  (uint16_t)(here - prev_case_jump));
        have_prev_case_jump = 0;
      }

      while (!check(p, tok_kw_case) && !check(p, tok_kw_default) &&
             !check(p, tok_rbrace) && !check(p, tok_eof))
        parse_stmt(p, c);
    } else {
      error_at(p, "expected 'case' or 'default'");
      break;
    }
  }
  (void)default_seen;

  if (have_prev_case_jump) {
    size_t here = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(prev_case_jump + 1),
              (uint16_t)(here - prev_case_jump));
  }

  expect(p, tok_rbrace);

  size_t end_pos = op_pos(c->m);
  patch_breaks(c, end_pos);
  c->break_depth--;
}

static void parse_function_decl(parser* p, compiler* c) {
  if (!expect(p, tok_ident))
    return;
  tok name = p->prev;

  if (!expect(p, tok_lparen))
    return;

  param params[v6_max_params];
  int param_count = 0;

  if (!check(p, tok_rparen)) {
    if (expect(p, tok_ident)) {
      params[param_count].name = p->prev.start;
      params[param_count].len = p->prev.len;
      param_count++;
    }
    while (match(p, tok_comma)) {
      if (expect(p, tok_ident)) {
        params[param_count].name = p->prev.start;
        params[param_count].len = p->prev.len;
        param_count++;
      }
    }
  }
  if (!expect(p, tok_rparen))
    return;

  fn_entry* fn = find_fn(c, name.start, name.len);
  method* m;

  if (fn) {
    char desc[16 + 9 * v6_max_params];
    build_descriptor(desc, param_count);
    m = cf_method(c->cf, acc_static, fn->name, desc);
  } else {
    char* fn_name = dup_tok(name);
    char desc[16 + 9 * v6_max_params];
    build_descriptor(desc, param_count);
    m = cf_method(c->cf, acc_static, fn_name, desc);
    c->fns[c->fn_count].name = fn_name;
    c->fns[c->fn_count].len = name.len;
    c->fns[c->fn_count].param_count = param_count;
    c->fn_count++;
  }

  m->max_stack = 64;

  compiler fc = *c;
  fc.m = m;
  fc.param_count = param_count;
  fc.local_count = 0;
  fc.scratch_slot = (uint16_t)param_count;
  fc.next_local_slot = (uint16_t)param_count + 2;
  fc.break_depth = 0;
  fc.continue_depth = 0;
  for (int i = 0; i < param_count; i++)
    fc.params[i] = params[i];

  if (!expect(p, tok_lbrace))
    return;
  prescan_decls(&fc, p->cur.start, 0);
  parse_block(p, &fc);

  emit_box_const(c->cf, m, op_iconst_3, op_dconst_0);
  op_emit(m, op_areturn);

  m->max_locals = fc.next_local_slot;
}

static void parse_stmt(parser* p, compiler* c) {
  if (match(p, tok_lbrace)) {
    parse_block(p, c);
    return;
  }

  if (match(p, tok_kw_if)) {
    parse_if(p, c);
    return;
  }

  if (match(p, tok_kw_while)) {
    parse_while(p, c);
    return;
  }

  if (match(p, tok_kw_for)) {
    parse_for(p, c);
    return;
  }

  if (match(p, tok_kw_switch)) {
    parse_switch(p, c);
    return;
  }

  if (match(p, tok_kw_break)) {
    if (c->break_depth == 0) {
      error_at(p, "'break' outside loop or switch");
    } else {
      break_ctx* bc = &c->breaks[c->break_depth - 1];
      bc->jumps[bc->count++] = op_pos(c->m);
      op_emit2(c->m, op_goto, 0);
    }
    expect(p, tok_semi);
    return;
  }

  if (match(p, tok_kw_continue)) {
    if (c->continue_depth == 0) {
      error_at(p, "'continue' outside loop");
    } else {
      size_t target = c->continues[c->continue_depth - 1];
      size_t here = op_pos(c->m);
      op_emit2(c->m, op_goto, 0);
      op_patch2(c->m, (uint16_t)(here + 1), (uint16_t)(target - here));
    }
    expect(p, tok_semi);
    return;
  }

  if (match(p, tok_kw_var) || match(p, tok_kw_let) || match(p, tok_kw_const)) {
    parse_var_decl(p, c, p->prev.kind);
    expect(p, tok_semi);
    return;
  }

  if (match(p, tok_kw_return)) {
    if (check(p, tok_semi))
      emit_box_const(c->cf, c->m, op_iconst_3, op_dconst_0);
    else
      parse_expr(p, c);
    expect(p, tok_semi);
    op_emit(c->m, op_areturn);
    return;
  }

  parse_expr(p, c);
  op_emit(c->m, op_pop);
  expect(p, tok_semi);
}

static void prescan_decls(compiler* c, const char* src, int hoist_functions) {
  lexer lx;
  lex_init(&lx, src);
  int depth = 0;
  tok t = lex_next(&lx);
  while (t.kind != tok_eof) {
    if (t.kind == tok_lbrace) {
      depth++;
    } else if (t.kind == tok_rbrace) {
      if (depth == 0)
        break;
      depth--;
    } else if (t.kind == tok_kw_var) {
      tok name = lex_next(&lx);
      if (name.kind == tok_ident) {
        uint16_t slot = c->next_local_slot++;
        emit_box_const(c->cf, c->m, op_iconst_3, op_dconst_0);
        emit_astore(c->m, slot);
        add_local(c, name, slot, 1, 0);
      }
      t = lex_next(&lx);
      continue;
    } else if (hoist_functions && depth == 0 && t.kind == tok_kw_function) {
      tok name = lex_next(&lx);
      if (name.kind == tok_ident) {
        lexer save = lx;
        tok lp = lex_next(&lx);
        int param_count = 0;
        if (lp.kind == tok_lparen) {
          tok pt = lex_next(&lx);
          if (pt.kind == tok_ident) {
            param_count++;
            pt = lex_next(&lx);
          }
          while (pt.kind == tok_comma) {
            pt = lex_next(&lx);
            if (pt.kind == tok_ident) {
              param_count++;
              pt = lex_next(&lx);
            }
          }
        }
        (void)save;
        char* fn_name = dup_tok(name);
        c->fns[c->fn_count].name = fn_name;
        c->fns[c->fn_count].len = name.len;
        c->fns[c->fn_count].param_count = param_count;
        c->fn_count++;
      }
    }
    t = lex_next(&lx);
  }
}

static void parse_program(parser* p, compiler* c) {
  while (!check(p, tok_eof)) {
    if (match(p, tok_kw_function))
      parse_function_decl(p, c);
    else
      parse_stmt(p, c);
  }
}

compile_result compile_program(const char* src, class_file* cf) {
  method* main_m =
      cf_method(cf, acc_public | acc_static, "main", "([Ljava/lang/String;)V");
  main_m->max_stack = 64;

  compiler c;
  c.cf = cf;
  c.m = main_m;
  c.param_count = 0;
  c.local_count = 0;
  c.scratch_slot = 1;
  c.next_local_slot = 3;
  c.fn_count = 0;
  c.break_depth = 0;
  c.continue_depth = 0;

  build_print_fn(&c);
  prescan_decls(&c, src, 1);

  parser p;
  parser_init(&p, src);
  parse_program(&p, &c);

  op_emit(main_m, op_return);
  main_m->max_locals = c.next_local_slot;

  compile_result r;
  r.ok = p.had_error ? 0 : 1;
  r.line = p.err_line;
  memcpy(r.message, p.err_msg, sizeof(r.message));
  return r;
}
