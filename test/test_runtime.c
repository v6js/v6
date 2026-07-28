#include "test.h"
#include "v6/runtime.h"

int test_runtime(void) {
  int fails = 0;

  v6_check(&fails, v6_runtime_class_len > 10);
  v6_check(&fails, v6_runtime_class[0] == 0xca);
  v6_check(&fails, v6_runtime_class[1] == 0xfe);
  v6_check(&fails, v6_runtime_class[2] == 0xba);
  v6_check(&fails, v6_runtime_class[3] == 0xbe);

  return fails;
}
