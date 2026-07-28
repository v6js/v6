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

  return fails;
}
