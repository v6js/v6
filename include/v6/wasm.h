#pragma once

#include "v6/bytecode.h"
#include "v6/parser.h"

#include <stddef.h>
#include <stdint.h>

enum {
  wasm_type_i32 = 0x7F,
  wasm_type_i64 = 0x7E,
  wasm_type_f32 = 0x7D,
  wasm_type_f64 = 0x7C,
  wasm_type_v128 = 0x7B,
  wasm_type_funcref = 0x70,
  wasm_type_externref = 0x6F,
};

enum {
  wasm_sec_custom = 0,
  wasm_sec_type = 1,
  wasm_sec_import = 2,
  wasm_sec_function = 3,
  wasm_sec_table = 4,
  wasm_sec_memory = 5,
  wasm_sec_global = 6,
  wasm_sec_export = 7,
  wasm_sec_start = 8,
  wasm_sec_element = 9,
  wasm_sec_code = 10,
  wasm_sec_data = 11,
  wasm_sec_datacount = 12,
};

enum {
  wasm_import_func = 0,
  wasm_import_table = 1,
  wasm_import_mem = 2,
  wasm_import_global = 3,
};

typedef struct wasm_functype {
  uint8_t* params;
  uint32_t param_count;
  uint8_t* results;
  uint32_t result_count;
} wasm_functype;

typedef struct wasm_limits {
  uint32_t min;
  uint32_t max;
  int has_max;
} wasm_limits;

typedef struct wasm_global_decl {
  uint8_t val_type;
  int is_mutable;
  const uint8_t* init_start;
  size_t init_len;
  int is_import;
  uint32_t import_index;
} wasm_global_decl;

typedef struct wasm_import {
  char* module_name;
  char* field_name;
  int kind;
  uint32_t type_index;
  wasm_limits table_limits;
  uint8_t table_reftype;
  wasm_limits mem_limits;
  uint8_t global_type;
  int global_mutable;
} wasm_import;

typedef struct wasm_export {
  char* name;
  int kind;
  uint32_t index;
} wasm_export;

typedef struct wasm_code_body {
  const uint8_t* start;
  size_t len;
  uint8_t* local_types;
  uint32_t local_count;
} wasm_code_body;

typedef struct wasm_element_seg {
  uint32_t table_index;
  const uint8_t* offset_start;
  size_t offset_len;
  uint32_t* func_indices;
  uint32_t func_count;
} wasm_element_seg;

typedef struct wasm_data_seg {
  uint32_t mem_index;
  const uint8_t* offset_start;
  size_t offset_len;
  const uint8_t* data;
  uint32_t data_len;
} wasm_data_seg;

typedef struct wasm_module {
  wasm_functype* types;
  uint32_t type_count;

  wasm_import* imports;
  uint32_t import_count;
  uint32_t imported_func_count;
  uint32_t imported_table_count;
  uint32_t imported_mem_count;
  uint32_t imported_global_count;

  uint32_t* func_type_indices;
  uint32_t func_count;

  wasm_limits* tables;
  uint8_t* table_reftypes;
  uint32_t table_count;

  wasm_limits* memories;
  uint32_t memory_count;

  wasm_global_decl* globals;
  uint32_t global_count;

  wasm_export* exports;
  uint32_t export_count;

  int has_start;
  uint32_t start_func_index;

  wasm_element_seg* elements;
  uint32_t element_count;

  wasm_code_body* codes;
  uint32_t code_count;

  wasm_data_seg* datas;
  uint32_t data_count;

  const uint8_t* buf;
  size_t buf_len;
  int ok;
  char err_msg[256];
} wasm_module;

int wasm_parse_module(const uint8_t* buf, size_t len, wasm_module* out);
void wasm_module_free(wasm_module* m);

uint32_t wasm_func_type_index(const wasm_module* m, uint32_t func_index);
const wasm_functype* wasm_func_type(const wasm_module* m, uint32_t func_index);
int wasm_func_is_import(const wasm_module* m, uint32_t func_index);

compile_result wasm_compile_module(wasm_module* m, class_file* cf,
                                   const char* class_name);

int wasm_find_entry(const wasm_module* m, uint32_t* out_func_idx);

void wasm_build_func_desc(const wasm_functype* ft, char* out, size_t cap);
