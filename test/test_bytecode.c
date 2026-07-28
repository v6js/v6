#include "test.h"
#include "v6/bytecode.h"

int test_bytecode(void) {
  int fails = 0;
  class_file cf;
  cf_init(&cf, "Main", "java/lang/Object");

  method *m = cf_method(&cf, acc_public | acc_static, "main", "()V");
  m->max_stack = 1;
  m->max_locals = 1;
  op_emit(m, op_return);

  buf out;
  buf_init(&out);
  cf_emit(&cf, &out);

  v6_check(&fails, out.len > 10);
  v6_check(&fails, out.data[0] == 0xca);
  v6_check(&fails, out.data[1] == 0xfe);
  v6_check(&fails, out.data[2] == 0xba);
  v6_check(&fails, out.data[3] == 0xbe);
  v6_check(&fails, out.data[6] == 0);
  v6_check(&fails, out.data[7] == 52);

  buf_free(&out);
  cf_free(&cf);
  return fails;
}
