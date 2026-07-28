#include "test.h"
#include "v6/parser.h"

static compiler make_compiler(class_file* cf, method* m) {
  compiler c;
  c.cf = cf;
  c.m = m;
  c.param_count = 0;
  c.local_count = 0;
  c.scratch_slot = 1;
  c.next_local_slot = 3;
  c.fn_count = 0;
  return c;
}

int test_parser(void) {
  int fails = 0;

  class_file cf;
  cf_init(&cf, "Main", "java/lang/Object");
  method* m = cf_method(&cf, acc_public | acc_static, "calc", "()D");
  compiler c = make_compiler(&cf, m);

  parser p;
  parser_init(&p, "1 + 2 * 3");
  int rc = compile_expr(&p, &c);
  v6_check(&fails, rc == 0);
  v6_check(&fails, m->code.len > 0);
  v6_check(&fails, m->code.data[0] == op_new);

  cf_free(&cf);

  class_file cf2;
  cf_init(&cf2, "Main", "java/lang/Object");
  method* m2 = cf_method(&cf2, acc_public | acc_static, "calc", "()D");
  compiler c2 = make_compiler(&cf2, m2);

  parser p2;
  parser_init(&p2, "-(1+2)*4");
  int rc2 = compile_expr(&p2, &c2);
  v6_check(&fails, rc2 == 0);

  cf_free(&cf2);

  class_file cf3;
  cf_init(&cf3, "Main", "java/lang/Object");
  method* m3 = cf_method(&cf3, acc_public | acc_static, "calc", "()D");
  compiler c3 = make_compiler(&cf3, m3);

  parser p3;
  parser_init(&p3, "(1 + 2");
  int rc3 = compile_expr(&p3, &c3);
  v6_check(&fails, rc3 != 0);

  cf_free(&cf3);

  class_file cf4;
  cf_init(&cf4, "Main", "java/lang/Object");
  method* m4 = cf_method(&cf4, acc_public | acc_static, "calc", "()D");
  compiler c4 = make_compiler(&cf4, m4);

  parser p4;
  parser_init(&p4, "true");
  int rc4 = compile_expr(&p4, &c4);
  v6_check(&fails, rc4 == 0);
  v6_check(&fails, m4->code.len > 0);
  v6_check(&fails, m4->code.data[0] == op_new);

  cf_free(&cf4);

  class_file cf5;
  cf_init(&cf5, "Main", "java/lang/Object");
  method* m5 = cf_method(&cf5, acc_public | acc_static, "calc", "()D");
  compiler c5 = make_compiler(&cf5, m5);

  parser p5;
  parser_init(&p5, "\"hi\"");
  int rc5 = compile_expr(&p5, &c5);
  v6_check(&fails, rc5 == 0);

  cf_free(&cf5);

  class_file cf6;
  cf_init(&cf6, "Main", "java/lang/Object");
  method* m6 = cf_method(&cf6, acc_public | acc_static, "calc", "()D");
  compiler c6 = make_compiler(&cf6, m6);

  parser p6;
  parser_init(&p6, "1 < 2");
  int rc6 = compile_expr(&p6, &c6);
  v6_check(&fails, rc6 == 0);

  cf_free(&cf6);

  class_file cf7;
  cf_init(&cf7, "Main", "java/lang/Object");
  method* m7 = cf_method(&cf7, acc_public | acc_static, "calc", "()D");
  compiler c7 = make_compiler(&cf7, m7);

  parser p7;
  parser_init(&p7, "1 == 1");
  int rc7 = compile_expr(&p7, &c7);
  v6_check(&fails, rc7 == 0);

  cf_free(&cf7);

  return fails;
}

int test_compile_program(void) {
  int fails = 0;

  class_file cf;
  cf_init(&cf, "Main", "java/lang/Object");
  int rc = compile_program("print(1 + 2 * 3);", &cf);
  v6_check(&fails, rc == 0);
  v6_check(&fails, cf.method_len == 2);
  cf_free(&cf);

  class_file cf2;
  cf_init(&cf2, "Main", "java/lang/Object");
  int rc2 = compile_program("(1 + 2", &cf2);
  v6_check(&fails, rc2 != 0);
  cf_free(&cf2);

  class_file cf3;
  cf_init(&cf3, "Main", "java/lang/Object");
  int rc3 = compile_program("print(true);", &cf3);
  v6_check(&fails, rc3 == 0);
  cf_free(&cf3);

  class_file cf4;
  cf_init(&cf4, "Main", "java/lang/Object");
  int rc4 = compile_program(
      "function add(a, b) { return a + b; } print(add(2, 3));", &cf4);
  v6_check(&fails, rc4 == 0);
  v6_check(&fails, cf4.method_len == 3);
  cf_free(&cf4);

  class_file cf5;
  cf_init(&cf5, "Main", "java/lang/Object");
  int rc5 = compile_program("print(nope(1));", &cf5);
  v6_check(&fails, rc5 != 0);
  cf_free(&cf5);

  return fails;
}
