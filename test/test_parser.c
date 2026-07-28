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
  v6_check(&fails, m->code.len == 11);
  if (m->code.len == 11) {
    v6_check(&fails, m->code.data[0] == op_ldc2_w);
    v6_check(&fails, m->code.data[3] == op_ldc2_w);
    v6_check(&fails, m->code.data[6] == op_ldc2_w);
    v6_check(&fails, m->code.data[9] == op_dmul);
    v6_check(&fails, m->code.data[10] == op_dadd);
  }

  cf_free(&cf);

  class_file cf2;
  cf_init(&cf2, "Main", "java/lang/Object");
  method* m2 = cf_method(&cf2, acc_public | acc_static, "calc", "()D");

  parser p2;
  parser_init(&p2, "-(1+2)*4");
  int rc2 = compile_expr(&p2, &cf2, m2);

  v6_check(&fails, rc2 == 0);
  v6_check(&fails, m2->code.len == 12);
  if (m2->code.len == 12) {
    v6_check(&fails, m2->code.data[0] == op_ldc2_w);
    v6_check(&fails, m2->code.data[3] == op_ldc2_w);
    v6_check(&fails, m2->code.data[6] == op_dadd);
    v6_check(&fails, m2->code.data[7] == op_dneg);
    v6_check(&fails, m2->code.data[8] == op_ldc2_w);
    v6_check(&fails, m2->code.data[11] == op_dmul);
  }

  cf_free(&cf2);

  class_file cf3;
  cf_init(&cf3, "Main", "java/lang/Object");
  method* m3 = cf_method(&cf3, acc_public | acc_static, "calc", "()D");

  parser p3;
  parser_init(&p3, "(1 + 2");
  int rc3 = compile_expr(&p3, &cf3, m3);
  v6_check(&fails, rc3 != 0);

  cf_free(&cf3);

  return fails;
}

int test_compile_program(void) {
  int fails = 0;

  class_file cf;
  cf_init(&cf, "Main", "java/lang/Object");

  int rc = compile_program("1 + 2 * 3", &cf);
  v6_check(&fails, rc == 0);
  v6_check(&fails, cf.method_len == 1);

  if (rc == 0 && cf.method_len == 1) {
    method* m = cf.methods[0];
    v6_check(&fails, m->code.len == 32);
    if (m->code.len == 32) {
      v6_check(&fails, m->code.data[0] == op_getstatic);
      v6_check(&fails, m->code.data[3] == op_ldc2_w);
      v6_check(&fails, m->code.data[14] == op_dstore_1);
      v6_check(&fails, m->code.data[15] == op_new);
      v6_check(&fails, m->code.data[18] == op_dup);
      v6_check(&fails, m->code.data[19] == op_iconst_0);
      v6_check(&fails, m->code.data[20] == op_dload_1);
      v6_check(&fails, m->code.data[21] == op_aconst_null);
      v6_check(&fails, m->code.data[22] == op_invokespecial);
      v6_check(&fails, m->code.data[25] == op_invokevirtual);
      v6_check(&fails, m->code.data[28] == op_invokevirtual);
      v6_check(&fails, m->code.data[31] == op_return);
    }
  }

  cf_free(&cf);

  class_file cf2;
  cf_init(&cf2, "Main", "java/lang/Object");
  int rc2 = compile_program("(1 + 2", &cf2);
  v6_check(&fails, rc2 != 0);
  cf_free(&cf2);

  return fails;
}
