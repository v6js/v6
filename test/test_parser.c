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
  c.break_depth = 0;
  c.continue_depth = 0;
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
  compile_result rc = compile_program("print(1 + 2 * 3);", &cf);
  v6_check(&fails, rc.ok);
  v6_check(&fails, cf.method_len == 2);
  cf_free(&cf);

  class_file cf2;
  cf_init(&cf2, "Main", "java/lang/Object");
  compile_result rc2 = compile_program("(1 + 2", &cf2);
  v6_check(&fails, !rc2.ok);
  cf_free(&cf2);

  class_file cf3;
  cf_init(&cf3, "Main", "java/lang/Object");
  compile_result rc3 = compile_program("print(true);", &cf3);
  v6_check(&fails, rc3.ok);
  cf_free(&cf3);

  class_file cf4;
  cf_init(&cf4, "Main", "java/lang/Object");
  compile_result rc4 = compile_program(
      "function add(a, b) { return a + b; } print(add(2, 3));", &cf4);
  v6_check(&fails, rc4.ok);
  v6_check(&fails, cf4.method_len == 3);
  cf_free(&cf4);

  class_file cf5;
  cf_init(&cf5, "Main", "java/lang/Object");
  compile_result rc5 = compile_program("print(nope(1));", &cf5);
  v6_check(&fails, !rc5.ok);
  cf_free(&cf5);

  class_file cf6;
  cf_init(&cf6, "Main", "java/lang/Object");
  compile_result rc6 = compile_program(
      "print(add(2, 3)); function add(a, b) { return a + b; }", &cf6);
  v6_check(&fails, rc6.ok);
  cf_free(&cf6);

  class_file cf7;
  cf_init(&cf7, "Main", "java/lang/Object");
  compile_result rc7 = compile_program("print(1 < 2 ? \"a\" : \"b\");", &cf7);
  v6_check(&fails, rc7.ok);
  cf_free(&cf7);

  class_file cf8;
  cf_init(&cf8, "Main", "java/lang/Object");
  compile_result rc8 =
      compile_program("var x = 1; x += 2; x++; --x; print(x);", &cf8);
  v6_check(&fails, rc8.ok);
  cf_free(&cf8);

  class_file cf9;
  cf_init(&cf9, "Main", "java/lang/Object");
  compile_result rc9 = compile_program(
      "var i = 0; while (i < 3) { if (i == 1) { i++; continue; } print(i); "
      "i++; }",
      &cf9);
  v6_check(&fails, rc9.ok);
  cf_free(&cf9);

  class_file cf10;
  cf_init(&cf10, "Main", "java/lang/Object");
  compile_result rc10 = compile_program(
      "switch (1) { case 1: print(\"a\"); break; default: print(\"b\"); }",
      &cf10);
  v6_check(&fails, rc10.ok);
  cf_free(&cf10);

  class_file cf11;
  cf_init(&cf11, "Main", "java/lang/Object");
  compile_result rc11 = compile_program(
      "var o = { a: 1 }; o.a = 2; print(o.a); print(o[\"a\"]);", &cf11);
  v6_check(&fails, rc11.ok);
  cf_free(&cf11);

  class_file cf12;
  cf_init(&cf12, "Main", "java/lang/Object");
  compile_result rc12 = compile_program(
      "var arr = [1, 2, 3]; print(arr[1]); print(arr.length);", &cf12);
  v6_check(&fails, rc12.ok);
  cf_free(&cf12);

  class_file cf13;
  cf_init(&cf13, "Main", "java/lang/Object");
  compile_result rc13 = compile_program("const x = 1; x = 2;", &cf13);
  v6_check(&fails, !rc13.ok);
  cf_free(&cf13);

  class_file cf14;
  cf_init(&cf14, "Main", "java/lang/Object");
  compile_result rc14 = compile_program("break;", &cf14);
  v6_check(&fails, !rc14.ok);
  cf_free(&cf14);

  return fails;
}
