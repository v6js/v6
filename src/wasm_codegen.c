#include "v6/wasm.h"

#include <stdio.h>
#include <string.h>

compile_result wasm_compile_module(wasm_module* m, class_file* cf,
                                   const char* class_name) {
  (void)m;
  (void)cf;
  (void)class_name;
  compile_result r;
  r.ok = 0;
  r.line = 0;
  snprintf(r.message, sizeof(r.message), "wasm codegen not yet implemented");
  return r;
}
