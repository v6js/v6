#include "v6/parser.h"

#include <stdlib.h>
#include <string.h>

static void advance(parser* p) {
  p->prev = p->cur;
  p->cur = lex_next(&p->lex);
}

void parser_init(parser* p, const char* src) {
  lex_init(&p->lex, src);
  p->had_error = 0;
  advance(p);
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

static int expect(parser* p, tok_kind k) {
  if (match(p, k))
    return 1;
  p->had_error = 1;
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

static void emit_unbox_num(compiler* c) {
  uint16_t idx = cf_methodref(c->cf, "V6Value", "num", "()D");
  op_emit2(c->m, op_invokevirtual, idx);
}

static void emit_compare(compiler* c, uint8_t cmp_op, uint8_t if_op) {
  op_emit(c->m, cmp_op);
  size_t if_pos = op_pos(c->m);
  op_emit2(c->m, if_op, 0);
  emit_box_const(c->cf, c->m, op_iconst_1, op_dconst_0);
  size_t goto_pos = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);
  size_t true_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(if_pos + 1), (uint16_t)(true_pos - if_pos));
  emit_box_const(c->cf, c->m, op_iconst_1, op_dconst_1);
  size_t end_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(goto_pos + 1), (uint16_t)(end_pos - goto_pos));
}

static char* decode_string(tok t) {
  char* buf = malloc(t.len);
  size_t j = 0;
  for (size_t i = 1; i + 1 < t.len; i++) {
    char ch = t.start[i];
    if (ch == '\\' && i + 2 < t.len) {
      i++;
      char e = t.start[i];
      if (e == 'n')
        buf[j++] = '\n';
      else if (e == 't')
        buf[j++] = '\t';
      else
        buf[j++] = e;
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

static int find_param(compiler* c, const char* name, size_t len) {
  for (int i = 0; i < c->param_count; i++) {
    if (c->params[i].len == len && memcmp(c->params[i].name, name, len) == 0)
      return i;
  }
  return -1;
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

static void parse_call(parser* p, compiler* c, tok name) {
  fn_entry* fn = find_fn(c, name.start, name.len);
  if (!fn) {
    p->had_error = 1;
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
    p->had_error = 1;
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

  if (match(p, tok_ident)) {
    tok name = p->prev;
    if (match(p, tok_lparen)) {
      parse_call(p, c, name);
      return;
    }
    int slot = find_param(c, name.start, name.len);
    if (slot < 0) {
      p->had_error = 1;
      return;
    }
    emit_aload(c->m, (uint16_t)slot);
    return;
  }

  if (match(p, tok_lparen)) {
    parse_expr(p, c);
    if (!match(p, tok_rparen))
      p->had_error = 1;
    return;
  }

  p->had_error = 1;
  advance(p);
}

static void parse_unary(parser* p, compiler* c) {
  if (match(p, tok_minus)) {
    parse_unary(p, c);
    emit_unbox_num(c);
    op_emit(c->m, op_dneg);
    emit_box_tag(c, op_iconst_0);
    return;
  }
  parse_primary(p, c);
}

static void parse_mul(parser* p, compiler* c) {
  parse_unary(p, c);
  while (check(p, tok_star) || check(p, tok_slash) || check(p, tok_percent)) {
    tok_kind k = p->cur.kind;
    advance(p);
    emit_unbox_num(c);
    parse_unary(p, c);
    emit_unbox_num(c);
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
    emit_unbox_num(c);
    parse_mul(p, c);
    emit_unbox_num(c);
    op_emit(c->m, k == tok_plus ? op_dadd : op_dsub);
    emit_box_tag(c, op_iconst_0);
  }
}

static void parse_cmp(parser* p, compiler* c) {
  parse_add(p, c);
  while (check(p, tok_lt) || check(p, tok_gt) || check(p, tok_le) ||
         check(p, tok_ge)) {
    tok_kind k = p->cur.kind;
    advance(p);
    emit_unbox_num(c);
    parse_add(p, c);
    emit_unbox_num(c);
    if (k == tok_lt)
      emit_compare(c, op_dcmpg, op_iflt);
    else if (k == tok_le)
      emit_compare(c, op_dcmpg, op_ifle);
    else if (k == tok_gt)
      emit_compare(c, op_dcmpl, op_ifgt);
    else
      emit_compare(c, op_dcmpl, op_ifge);
  }
}

static void parse_eq(parser* p, compiler* c) {
  parse_cmp(p, c);
  while (check(p, tok_eq) || check(p, tok_neq)) {
    tok_kind k = p->cur.kind;
    advance(p);
    parse_cmp(p, c);
    uint16_t eq_idx =
        cf_methodref(c->cf, "V6Value", "equals", "(Ljava/lang/Object;)Z");
    op_emit2(c->m, op_invokevirtual, eq_idx);
    if (k == tok_neq) {
      op_emit(c->m, op_iconst_1);
      op_emit(c->m, op_ixor);
    }
    op_emit(c->m, op_i2d);
    emit_box_tag(c, op_iconst_1);
  }
}

static void parse_expr(parser* p, compiler* c) {
  parse_eq(p, c);
}

int compile_expr(parser* p, compiler* c) {
  parse_expr(p, c);
  return p->had_error ? -1 : 0;
}

static void parse_stmt(parser* p, compiler* c);

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

  char* fn_name = dup_tok(name);
  char desc[16 + 9 * v6_max_params];
  build_descriptor(desc, param_count);

  method* m = cf_method(c->cf, acc_static, fn_name, desc);
  m->max_stack = 64;
  m->max_locals = (uint16_t)param_count + 2;

  c->fns[c->fn_count].name = fn_name;
  c->fns[c->fn_count].len = name.len;
  c->fns[c->fn_count].param_count = param_count;
  c->fn_count++;

  compiler fc = *c;
  fc.m = m;
  fc.param_count = param_count;
  fc.scratch_slot = (uint16_t)param_count;
  for (int i = 0; i < param_count; i++)
    fc.params[i] = params[i];

  if (!expect(p, tok_lbrace))
    return;
  while (!check(p, tok_rbrace) && !check(p, tok_eof))
    parse_stmt(p, &fc);
  expect(p, tok_rbrace);

  emit_box_const(c->cf, m, op_iconst_3, op_dconst_0);
  op_emit(m, op_areturn);
}

static void parse_stmt(parser* p, compiler* c) {
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

static void parse_program(parser* p, compiler* c) {
  while (!check(p, tok_eof)) {
    if (match(p, tok_kw_function))
      parse_function_decl(p, c);
    else
      parse_stmt(p, c);
  }
}

int compile_program(const char* src, class_file* cf) {
  method* main_m =
      cf_method(cf, acc_public | acc_static, "main", "([Ljava/lang/String;)V");
  main_m->max_stack = 64;
  main_m->max_locals = 3;

  compiler c;
  c.cf = cf;
  c.m = main_m;
  c.param_count = 0;
  c.scratch_slot = 1;
  c.fn_count = 0;

  build_print_fn(&c);

  parser p;
  parser_init(&p, src);
  parse_program(&p, &c);

  op_emit(main_m, op_return);

  return p.had_error ? -1 : 0;
}
