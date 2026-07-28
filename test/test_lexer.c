#include "test.h"
#include "v6/lexer.h"

#include <string.h>

int test_lexer(void) {
  int fails = 0;
  lexer lx;
  lex_init(&lx, "var x = 1.5 + foo(\"hi\");\n// comment\ntrue");

  tok t = lex_next(&lx);
  v6_check(&fails, t.kind == tok_kw_var);

  t = lex_next(&lx);
  v6_check(&fails, t.kind == tok_ident);
  v6_check(&fails, t.len == 1 && t.start[0] == 'x');

  t = lex_next(&lx);
  v6_check(&fails, t.kind == tok_assign);

  t = lex_next(&lx);
  v6_check(&fails, t.kind == tok_num);
  v6_check(&fails, t.num == 1.5);

  t = lex_next(&lx);
  v6_check(&fails, t.kind == tok_plus);

  t = lex_next(&lx);
  v6_check(&fails, t.kind == tok_ident);
  v6_check(&fails, t.len == 3 && memcmp(t.start, "foo", 3) == 0);

  t = lex_next(&lx);
  v6_check(&fails, t.kind == tok_lparen);

  t = lex_next(&lx);
  v6_check(&fails, t.kind == tok_str);

  t = lex_next(&lx);
  v6_check(&fails, t.kind == tok_rparen);

  t = lex_next(&lx);
  v6_check(&fails, t.kind == tok_semi);

  t = lex_next(&lx);
  v6_check(&fails, t.kind == tok_kw_true);

  t = lex_next(&lx);
  v6_check(&fails, t.kind == tok_eof);

  lexer lx2;
  lex_init(&lx2, "0x1F 0o17 0b101 1e3 1.5e-2 .5");

  t = lex_next(&lx2);
  v6_check(&fails, t.kind == tok_num && t.num == 31);
  t = lex_next(&lx2);
  v6_check(&fails, t.kind == tok_num && t.num == 15);
  t = lex_next(&lx2);
  v6_check(&fails, t.kind == tok_num && t.num == 5);
  t = lex_next(&lx2);
  v6_check(&fails, t.kind == tok_num && t.num == 1000);
  t = lex_next(&lx2);
  v6_check(&fails, t.kind == tok_num && t.num == 0.015);
  t = lex_next(&lx2);
  v6_check(&fails, t.kind == tok_num && t.num == 0.5);

  lexer lx3;
  lex_init(&lx3, "+= -= *= /= %= ++ -- ? : === !== break continue switch case "
                 "default");

  static const tok_kind expected[] = {
      tok_plus_eq,     tok_minus_eq,  tok_star_eq,     tok_slash_eq,
      tok_percent_eq,  tok_plus_plus, tok_minus_minus, tok_question,
      tok_colon,       tok_eq_strict, tok_neq_strict,  tok_kw_break,
      tok_kw_continue, tok_kw_switch, tok_kw_case,     tok_kw_default,
  };
  for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
    t = lex_next(&lx3);
    v6_check(&fails, t.kind == expected[i]);
  }

  return fails;
}
