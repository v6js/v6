#include "test.h"
#include "v6/parser.h"

int test_parser(void) {
  int fails = 0;

  class_file cf;
  cf_init(&cf, "Main", "java/lang/Object");
  method* m = cf_method(&cf, acc_public | acc_static, "calc", "()D");

  parser p;
  parser_init(&p, "1 + 2 * 3");
  int rc = compile_expr(&p, &cf, m);
  v6_check(&fails, rc == 0);
  v6_check(&fails, m->code.len > 0);
  v6_check(&fails, m->code.data[0] == op_new);

  cf_free(&cf);

  class_file cf2;
  cf_init(&cf2, "Main", "java/lang/Object");
  method* m2 = cf_method(&cf2, acc_public | acc_static, "calc", "()D");

  parser p2;
  parser_init(&p2, "-(1+2)*4");
  int rc2 = compile_expr(&p2, &cf2, m2);
  v6_check(&fails, rc2 == 0);

  cf_free(&cf2);

  class_file cf3;
  cf_init(&cf3, "Main", "java/lang/Object");
  method* m3 = cf_method(&cf3, acc_public | acc_static, "calc", "()D");

  parser p3;
  parser_init(&p3, "(1 + 2");
  int rc3 = compile_expr(&p3, &cf3, m3);
  v6_check(&fails, rc3 != 0);

  cf_free(&cf3);

  class_file cf4;
  cf_init(&cf4, "Main", "java/lang/Object");
  method* m4 = cf_method(&cf4, acc_public | acc_static, "calc", "()D");

  parser p4;
  parser_init(&p4, "true");
  int rc4 = compile_expr(&p4, &cf4, m4);
  v6_check(&fails, rc4 == 0);
  v6_check(&fails, m4->code.len > 0);
  v6_check(&fails, m4->code.data[0] == op_new);

  cf_free(&cf4);

  class_file cf5;
  cf_init(&cf5, "Main", "java/lang/Object");
  method* m5 = cf_method(&cf5, acc_public | acc_static, "calc", "()D");

  parser p5;
  parser_init(&p5, "\"hi\"");
  int rc5 = compile_expr(&p5, &cf5, m5);
  v6_check(&fails, rc5 == 0);

  cf_free(&cf5);

  return fails;
}

int test_compile_program(void) {
  int fails = 0;

  class_file cf;
  cf_init(&cf, "Main", "java/lang/Object");
  int rc = compile_program("1 + 2 * 3", &cf);
  v6_check(&fails, rc == 0);
  v6_check(&fails, cf.method_len == 1);
  cf_free(&cf);

  class_file cf2;
  cf_init(&cf2, "Main", "java/lang/Object");
  int rc2 = compile_program("(1 + 2", &cf2);
  v6_check(&fails, rc2 != 0);
  cf_free(&cf2);

  class_file cf3;
  cf_init(&cf3, "Main", "java/lang/Object");
  int rc3 = compile_program("true", &cf3);
  v6_check(&fails, rc3 == 0);
  cf_free(&cf3);

  class_file cf4;
  cf_init(&cf4, "Main", "java/lang/Object");
  int rc4 = compile_program("\"hi\"", &cf4);
  v6_check(&fails, rc4 == 0);
  cf_free(&cf4);

  return fails;
}
