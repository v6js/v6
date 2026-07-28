#include "test.h"
#include "v6/buffer.h"

int test_buf(void) {
  int fails = 0;
  buf b;
  buf_init(&b);

  buf_u8(&b, 0xab);
  buf_u16(&b, 0x1234);
  buf_u32(&b, 0xdeadbeef);

  v6_check(&fails, b.len == 7);
  v6_check(&fails, b.data[0] == 0xab);
  v6_check(&fails, b.data[1] == 0x12);
  v6_check(&fails, b.data[2] == 0x34);
  v6_check(&fails, b.data[3] == 0xde);
  v6_check(&fails, b.data[4] == 0xad);
  v6_check(&fails, b.data[5] == 0xbe);
  v6_check(&fails, b.data[6] == 0xef);

  buf_free(&b);
  return fails;
}
