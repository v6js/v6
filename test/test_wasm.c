#include "test.h"
#include "v6/jvm.h"
#include "v6/wasm.h"

#include <string.h>

static const uint8_t add_module[] = {
    0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00, 0x01, 0x07, 0x01,
    0x60, 0x02, 0x7F, 0x7F, 0x01, 0x7F, 0x03, 0x02, 0x01, 0x00, 0x07,
    0x07, 0x01, 0x03, 0x61, 0x64, 0x64, 0x00, 0x00, 0x0A, 0x09, 0x01,
    0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6A, 0x0B,
};

static const uint8_t global_module[] = {
    0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00, 0x01, 0x05, 0x01, 0x60,
    0x00, 0x01, 0x7F, 0x03, 0x02, 0x01, 0x00, 0x06, 0x06, 0x01, 0x7F, 0x00,
    0x41, 0x2A, 0x0B, 0x07, 0x08, 0x01, 0x04, 0x67, 0x65, 0x74, 0x47, 0x00,
    0x00, 0x0A, 0x06, 0x01, 0x04, 0x00, 0x23, 0x00, 0x0B,
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

int test_wasm_codegen(void) {
  int fails = 0;

  wasm_module m;
  int rc = wasm_parse_module(add_module, sizeof(add_module), &m);
  v6_check(&fails, rc == 0);
  if (rc != 0)
    return fails;

  class_file cf;
  cf_init(&cf, "WasmAddTest", "java/lang/Object");
  compile_result cr = wasm_compile_module(&m, &cf, "WasmAddTest");
  v6_check(&fails, cr.ok);

  v6_check(&fails, cf.method_len == 1);
  if (cf.method_len == 1) {
    method* wf = cf.methods[0];
    v6_check(&fails, wf->access == (acc_public | acc_static));
    v6_check(&fails, wf->max_locals >= 2);
    v6_check(&fails, wf->code.len > 0);
  }

  buf out;
  buf_init(&out);
  cf_emit(&cf, &out);
  v6_check(&fails, out.len > 10);
  v6_check(&fails, out.data[0] == 0xCA && out.data[1] == 0xFE &&
                       out.data[2] == 0xBA && out.data[3] == 0xBE);
  v6_check(&fails, out.data[6] == 0 && out.data[7] == 49);

  if (v6_jvm_available()) {
    v6_jvm* jvm = v6_jvm_create(NULL, 0);
    if (jvm) {
      int def_rc = v6_jvm_define_extra(jvm, "WasmAddTest", out.data, out.len);
      v6_check(&fails, def_rc == 0);
      if (def_rc == 0) {
        int result = 0;
        int call_rc = v6_jvm_call_static_i_ii(jvm, "WasmAddTest", "wasmFunc0",
                                              2, 3, &result);
        v6_check(&fails, call_rc == 0);
        v6_check(&fails, result == 5);

        int result2 = 0;
        int call_rc2 = v6_jvm_call_static_i_ii(jvm, "WasmAddTest", "wasmFunc0",
                                               -7, 10, &result2);
        v6_check(&fails, call_rc2 == 0);
        v6_check(&fails, result2 == 3);
      }
      v6_jvm_destroy(jvm);
    }
  }

  buf_free(&out);
  cf_free(&cf);
  wasm_module_free(&m);

  return fails;
}

int test_wasm_globals(void) {
  int fails = 0;

  wasm_module m;
  int rc = wasm_parse_module(global_module, sizeof(global_module), &m);
  v6_check(&fails, rc == 0);
  if (rc != 0)
    return fails;

  v6_check(&fails, m.global_count == 1);
  v6_check(&fails, m.globals[0].val_type == wasm_type_i32);
  v6_check(&fails, m.globals[0].is_mutable == 0);

  class_file cf;
  cf_init(&cf, "WasmGlobalTest", "java/lang/Object");
  compile_result cr = wasm_compile_module(&m, &cf, "WasmGlobalTest");
  v6_check(&fails, cr.ok);

  buf out;
  buf_init(&out);
  cf_emit(&cf, &out);

  if (v6_jvm_available()) {
    v6_jvm* jvm = v6_jvm_create(NULL, 0);
    if (jvm) {
      int def_rc =
          v6_jvm_define_extra(jvm, "WasmGlobalTest", out.data, out.len);
      v6_check(&fails, def_rc == 0);
      if (def_rc == 0) {
        int result = 0;
        int call_rc =
            v6_jvm_call_static_i(jvm, "WasmGlobalTest", "wasmFunc0", &result);
        v6_check(&fails, call_rc == 0);
        v6_check(&fails, result == 42);
      }
      v6_jvm_destroy(jvm);
    }
  }

  buf_free(&out);
  cf_free(&cf);
  wasm_module_free(&m);

  return fails;
}
