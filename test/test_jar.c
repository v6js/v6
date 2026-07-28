#include "test.h"
#include "v6/jar.h"

int test_jar(void) {
  int fails = 0;

  const uint8_t data_a[] = {1, 2, 3, 4};
  const uint8_t data_b[] = {5, 6, 7};

  jar_entry entries[2];
  entries[0].name = "A.class";
  entries[0].data = data_a;
  entries[0].len = sizeof(data_a);
  entries[1].name = "B.class";
  entries[1].data = data_b;
  entries[1].len = sizeof(data_b);

  buf out;
  buf_init(&out);
  jar_write(&out, entries, 2, "A");

  v6_check(&fails, out.len > 22);
  v6_check(&fails, out.data[0] == 0x50);
  v6_check(&fails, out.data[1] == 0x4b);
  v6_check(&fails, out.data[2] == 0x03);
  v6_check(&fails, out.data[3] == 0x04);

  v6_check(&fails, out.data[out.len - 22] == 0x50);
  v6_check(&fails, out.data[out.len - 21] == 0x4b);
  v6_check(&fails, out.data[out.len - 20] == 0x05);
  v6_check(&fails, out.data[out.len - 19] == 0x06);

  buf_free(&out);
  return fails;
}
