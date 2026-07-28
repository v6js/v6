#include "v6/parser.h"

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

static void parse_expr(parser* p, class_file* cf, method* m);

static void parse_primary(parser* p, class_file* cf, method* m) {
  if (match(p, tok_num)) {
    uint16_t idx = cf_double(cf, p->prev.num);
    op_emit2(m, op_ldc2_w, idx);
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
    op_emit(m, op_dneg);
    return;
  }
  parse_primary(p, cf, m);
}

static void parse_term(parser* p, class_file* cf, method* m) {
  parse_unary(p, cf, m);
  while (check(p, tok_star) || check(p, tok_slash)) {
    tok_kind k = p->cur.kind;
    advance(p);
    parse_unary(p, cf, m);
    op_emit(m, k == tok_star ? op_dmul : op_ddiv);
  }
}

static void parse_expr(parser* p, class_file* cf, method* m) {
  parse_term(p, cf, m);
  while (check(p, tok_plus) || check(p, tok_minus)) {
    tok_kind k = p->cur.kind;
    advance(p);
    parse_term(p, cf, m);
    op_emit(m, k == tok_plus ? op_dadd : op_dsub);
  }
}

int compile_expr(parser* p, class_file* cf, method* m) {
  parse_expr(p, cf, m);
  return p->had_error ? -1 : 0;
}

int compile_program(const char* src, class_file* cf) {
  method* m =
      cf_method(cf, acc_public | acc_static, "main", "([Ljava/lang/String;)V");
  m->max_stack = 8;
  m->max_locals = 1;

  uint16_t out_idx =
      cf_fieldref(cf, "java/lang/System", "out", "Ljava/io/PrintStream;");
  op_emit2(m, op_getstatic, out_idx);

  parser p2;
  parser_init(&p2, src);
  int rc = compile_expr(&p2, cf, m);
  if (rc != 0)
    return rc;

  uint16_t println_idx =
      cf_methodref(cf, "java/io/PrintStream", "println", "(D)V");
  op_emit2(m, op_invokevirtual, println_idx);
  op_emit(m, op_return);

  return 0;
}
