#include <stdio.h>

int test_value(void);
int test_buf(void);
int test_lexer(void);
int test_bytecode(void);
int test_parser(void);
int test_compile_program(void);
int test_runtime(void);
int test_jvm(void);
int test_jar(void);
int test_wasm(void);
int test_wasm_codegen(void);
int test_wasm_memory(void);
int test_wasm_data(void);
int test_wasm_select(void);
int test_wasm_jni_bridge(void);
int test_wasm_import(void);
int test_wasm_table(void);
int test_wasm_globals(void);

int main(void) {
  int fails = 0;

  fails += test_value();
  fails += test_buf();
  fails += test_lexer();
  fails += test_bytecode();
  fails += test_parser();
  fails += test_compile_program();
  fails += test_runtime();
  fails += test_jvm();
  fails += test_jar();
  fails += test_wasm();
  fails += test_wasm_codegen();
  fails += test_wasm_memory();
  fails += test_wasm_data();
  fails += test_wasm_select();
  fails += test_wasm_jni_bridge();
  fails += test_wasm_import();
  fails += test_wasm_table();
  fails += test_wasm_globals();

  if (fails) {
    fprintf(stderr, "%d failure(s)\n", fails);
    return 1;
  }

  printf("ok\n");
  return 0;
}
