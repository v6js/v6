#include <stdio.h>

int test_value(void);
int test_buf(void);
int test_lexer(void);
int test_bytecode(void);
int test_parser(void);
int test_runtime(void);

int main(void) {
  int fails = 0;

  fails += test_value();
  fails += test_buf();
  fails += test_lexer();
  fails += test_bytecode();
  fails += test_parser();
  fails += test_runtime();

  if (fails) {
    fprintf(stderr, "%d failure(s)\n", fails);
    return 1;
  }

  printf("ok\n");
  return 0;
}
