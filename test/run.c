#include <stdio.h>

int test_value(void);
int test_buf(void);

int main(void) {
  int fails = 0;

  fails += test_value();
  fails += test_buf();

  if (fails) {
    fprintf(stderr, "%d failure(s)\n", fails);
    return 1;
  }

  printf("ok\n");
  return 0;
}
