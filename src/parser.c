#include "v6/parser.h"

#include <stdlib.h>

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

static uint16_t value_class(class_file* cf) {
  return cf_class(cf, "V6Value");
}

static uint16_t value_ctor(class_file* cf) {
  return cf_methodref(cf, "V6Value", "<init>", "(IDLjava/lang/Object;)V");
}

static void emit_unbox_num(class_file* cf, method* m) {
  uint16_t num_idx = cf_methodref(cf, "V6Value", "num", "()D");
  op_emit2(m, op_invokevirtual, num_idx);
}

static void emit_box_computed(class_file* cf, method* m) {
  op_emit(m, op_dstore_1);
  op_emit2(m, op_new, value_class(cf));
  op_emit(m, op_dup);
  op_emit(m, op_iconst_0);
  op_emit(m, op_dload_1);
  op_emit(m, op_aconst_null);
  op_emit2(m, op_invokespecial, value_ctor(cf));
}

static char* decode_string(tok t) {
  char* buf = malloc(t.len);
  size_t j = 0;
  for (size_t i = 1; i + 1 < t.len; i++) {
    char c = t.start[i];
    if (c == '\\' && i + 2 < t.len) {
      i++;
      char e = t.start[i];
      if (e == 'n')
        buf[j++] = '\n';
      else if (e == 't')
        buf[j++] = '\t';
      else
        buf[j++] = e;
    } else {
      buf[j++] = c;
    }
  }
  buf[j] = '\0';
  return buf;
}

static void parse_expr(parser* p, class_file* cf, method* m);

static void parse_primary(parser* p, class_file* cf, method* m) {
  uint16_t cls = value_class(cf);
  uint16_t ctor = value_ctor(cf);

  if (match(p, tok_num)) {
    uint16_t idx = cf_double(cf, p->prev.num);
    op_emit2(m, op_new, cls);
    op_emit(m, op_dup);
    op_emit(m, op_iconst_0);
    op_emit2(m, op_ldc2_w, idx);
    op_emit(m, op_aconst_null);
    op_emit2(m, op_invokespecial, ctor);
    return;
  }

  if (match(p, tok_kw_true)) {
    op_emit2(m, op_new, cls);
    op_emit(m, op_dup);
    op_emit(m, op_iconst_1);
    op_emit(m, op_dconst_1);
    op_emit(m, op_aconst_null);
    op_emit2(m, op_invokespecial, ctor);
    return;
  }

  if (match(p, tok_kw_false)) {
    op_emit2(m, op_new, cls);
    op_emit(m, op_dup);
    op_emit(m, op_iconst_1);
    op_emit(m, op_dconst_0);
    op_emit(m, op_aconst_null);
    op_emit2(m, op_invokespecial, ctor);
    return;
  }

  if (match(p, tok_kw_null)) {
    op_emit2(m, op_new, cls);
    op_emit(m, op_dup);
    op_emit(m, op_iconst_2);
    op_emit(m, op_dconst_0);
    op_emit(m, op_aconst_null);
    op_emit2(m, op_invokespecial, ctor);
    return;
  }

  if (match(p, tok_kw_undefined)) {
    op_emit2(m, op_new, cls);
    op_emit(m, op_dup);
    op_emit(m, op_iconst_3);
    op_emit(m, op_dconst_0);
    op_emit(m, op_aconst_null);
    op_emit2(m, op_invokespecial, ctor);
    return;
  }

  if (check(p, tok_str)) {
    tok t = p->cur;
    advance(p);
    char* s = decode_string(t);
    uint16_t str_idx = cf_string(cf, s);
    free(s);

    op_emit2(m, op_new, cls);
    op_emit(m, op_dup);
    op_emit(m, op_iconst_5);
    op_emit(m, op_dconst_0);
    op_emit2(m, op_ldc_w, str_idx);
    op_emit2(m, op_invokespecial, ctor);
    return;
  }

  if (match(p, tok_lparen)) {
    parse_expr(p, cf, m);
    if (!match(p, tok_rparen))
      p->had_error = 1;
    return;
  }

  p->had_error = 1;
  advance(p);
}

static void parse_unary(parser* p, class_file* cf, method* m) {
  if (match(p, tok_minus)) {
    parse_unary(p, cf, m);
    emit_unbox_num(cf, m);
    op_emit(m, op_dneg);
    emit_box_computed(cf, m);
    return;
  }
  parse_primary(p, cf, m);
}

static void parse_term(parser* p, class_file* cf, method* m) {
  parse_unary(p, cf, m);
  while (check(p, tok_star) || check(p, tok_slash)) {
    tok_kind k = p->cur.kind;
    advance(p);
    emit_unbox_num(cf, m);
    parse_unary(p, cf, m);
    emit_unbox_num(cf, m);
    op_emit(m, k == tok_star ? op_dmul : op_ddiv);
    emit_box_computed(cf, m);
  }
}

static void parse_expr(parser* p, class_file* cf, method* m) {
  parse_term(p, cf, m);
  while (check(p, tok_plus) || check(p, tok_minus)) {
    tok_kind k = p->cur.kind;
    advance(p);
    emit_unbox_num(cf, m);
    parse_term(p, cf, m);
    emit_unbox_num(cf, m);
    op_emit(m, k == tok_plus ? op_dadd : op_dsub);
    emit_box_computed(cf, m);
  }
}

int compile_expr(parser* p, class_file* cf, method* m) {
  parse_expr(p, cf, m);
  return p->had_error ? -1 : 0;
}

int compile_program(const char* src, class_file* cf) {
  method* m =
      cf_method(cf, acc_public | acc_static, "main", "([Ljava/lang/String;)V");
  m->max_stack = 64;
  m->max_locals = 3;

  uint16_t out_idx =
      cf_fieldref(cf, "java/lang/System", "out", "Ljava/io/PrintStream;");
  op_emit2(m, op_getstatic, out_idx);

  parser p2;
  parser_init(&p2, src);
  int rc = compile_expr(&p2, cf, m);
  if (rc != 0)
    return rc;

  uint16_t tostring_idx =
      cf_methodref(cf, "V6Value", "toString", "()Ljava/lang/String;");
  uint16_t println_idx = cf_methodref(cf, "java/io/PrintStream", "println",
                                      "(Ljava/lang/String;)V");
  op_emit2(m, op_invokevirtual, tostring_idx);
  op_emit2(m, op_invokevirtual, println_idx);
  op_emit(m, op_return);

  return 0;
}
