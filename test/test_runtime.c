#include "test.h"
#include "v6/runtime.h"

#include <string.h>

int test_runtime(void) {
  int fails = 0;

  v6_check(&fails, v6_runtime_class_count >= 6);

  int found_value = 0;
  for (size_t i = 0; i < v6_runtime_class_count; i++) {
    v6_check(&fails, v6_runtime_classes[i].len > 10);
    v6_check(&fails, v6_runtime_classes[i].data[0] == 0xca);
    v6_check(&fails, v6_runtime_classes[i].data[1] == 0xfe);
    v6_check(&fails, v6_runtime_classes[i].data[2] == 0xba);
    v6_check(&fails, v6_runtime_classes[i].data[3] == 0xbe);
    if (strcmp(v6_runtime_classes[i].name, "V6Value") == 0)
      found_value = 1;
  }
  v6_check(&fails, found_value);

  return fails;
}
