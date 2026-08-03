#include "test.h"
#include "v6/jvm.h"

int test_jvm(void) {
  int fails = 0;

  if (!v6_jvm_available())
    return fails;

  v6_jvm* jvm = v6_jvm_create(NULL);
  v6_check(&fails, jvm != NULL);
  if (!jvm)
    return fails;

  v6_check(&fails, v6_jvm_load_runtime(jvm) == 0);

  v6_jvm_destroy(jvm);
  return fails;
}
