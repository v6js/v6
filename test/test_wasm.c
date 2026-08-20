#include "test.h"
#include "v6/wasm.h"

#include <string.h>

static const uint8_t add_module[] = {
    0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00, 0x01, 0x07, 0x01,
    0x60, 0x02, 0x7F, 0x7F, 0x01, 0x7F, 0x03, 0x02, 0x01, 0x00, 0x07,
    0x07, 0x01, 0x03, 0x61, 0x64, 0x64, 0x00, 0x00, 0x0A, 0x09, 0x01,
    0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6A, 0x0B,
};

int test_wasm(void) {
  int fails = 0;

  wasm_module m;
  int rc = wasm_parse_module(add_module, sizeof(add_module), &m);
  v6_check(&fails, rc == 0);
  v6_check(&fails, m.ok);

  v6_check(&fails, m.type_count == 1);
  v6_check(&fails, m.types[0].param_count == 2);
  v6_check(&fails, m.types[0].params[0] == wasm_type_i32);
  v6_check(&fails, m.types[0].params[1] == wasm_type_i32);
  v6_check(&fails, m.types[0].result_count == 1);
  v6_check(&fails, m.types[0].results[0] == wasm_type_i32);

  v6_check(&fails, m.func_count == 1);
  v6_check(&fails, m.func_type_indices[0] == 0);

  v6_check(&fails, m.export_count == 1);
  v6_check(&fails, strcmp(m.exports[0].name, "add") == 0);
  v6_check(&fails, m.exports[0].kind == wasm_import_func);
  v6_check(&fails, m.exports[0].index == 0);

  v6_check(&fails, m.code_count == 1);
  v6_check(&fails, m.codes[0].local_count == 0);
  v6_check(&fails, m.codes[0].len == 6);

  v6_check(&fails, wasm_func_is_import(&m, 0) == 0);
  const wasm_functype* ft = wasm_func_type(&m, 0);
  v6_check(&fails, ft != NULL);
  v6_check(&fails, ft->param_count == 2);

  wasm_module_free(&m);

  wasm_module bad;
  uint8_t garbage[4] = {1, 2, 3, 4};
  int rc2 = wasm_parse_module(garbage, sizeof(garbage), &bad);
  v6_check(&fails, rc2 != 0);
  v6_check(&fails, !bad.ok);

  return fails;
}
