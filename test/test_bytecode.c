#include "test.h"
#include "v6/bytecode.h"

#include <string.h>

static uint16_t rd16(const uint8_t* p) {
  return (uint16_t)((p[0] << 8) | p[1]);
}

static uint32_t rd32(const uint8_t* p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | p[3];
}

int test_bytecode(void) {
  int fails = 0;

  class_file cf;
  cf_init(&cf, "Main", "java/lang/Object");

  cf_field(&cf, acc_public, "count", "I");

  method* m = cf_method(&cf, acc_public | acc_static, "main", "()V");
  m->max_stack = 1;
  m->max_locals = 1;
  op_emit(m, op_return);

  buf out;
  buf_init(&out);
  cf_emit(&cf, &out);

  v6_check(&fails, rd32(out.data) == 0xcafebabeu);
  v6_check(&fails, rd16(out.data + 4) == 0);
  v6_check(&fails, rd16(out.data + 6) == 49);
  v6_check(&fails, rd16(out.data + 8) == (uint16_t)(cf.cp_count + 1));

  size_t off = 10;
  v6_check(&fails, off + cf.cp.len <= out.len);
  v6_check(&fails, memcmp(out.data + off, cf.cp.data, cf.cp.len) == 0);
  off += cf.cp.len;

  v6_check(&fails, rd16(out.data + off) == cf.access);
  v6_check(&fails, rd16(out.data + off + 2) == cf.this_idx);
  v6_check(&fails, rd16(out.data + off + 4) == cf.super_idx);
  v6_check(&fails, rd16(out.data + off + 6) == 0);
  v6_check(&fails, rd16(out.data + off + 8) == (uint16_t)cf.field_len);
  off += 10;

  for (size_t i = 0; i < cf.field_len; i++) {
    field* f = &cf.fields[i];
    v6_check(&fails, rd16(out.data + off) == f->access);
    v6_check(&fails, rd16(out.data + off + 2) == f->name_idx);
    v6_check(&fails, rd16(out.data + off + 4) == f->desc_idx);
    v6_check(&fails, rd16(out.data + off + 6) == 0);
    off += 8;
  }

  v6_check(&fails, rd16(out.data + off) == (uint16_t)cf.method_len);
  off += 2;

  v6_check(&fails, rd16(out.data + off) == m->access);
  v6_check(&fails, rd16(out.data + off + 2) == m->name_idx);
  v6_check(&fails, rd16(out.data + off + 4) == m->desc_idx);
  v6_check(&fails, rd16(out.data + off + 6) == 1);
  off += 8;

  v6_check(&fails, rd16(out.data + off) == cf.code_utf8);
  uint32_t attr_len = 2 + 2 + 4 + (uint32_t)m->code.len + 2 + 2;
  v6_check(&fails, rd32(out.data + off + 2) == attr_len);
  v6_check(&fails, rd16(out.data + off + 6) == m->max_stack);
  v6_check(&fails, rd16(out.data + off + 8) == m->max_locals);
  v6_check(&fails, rd32(out.data + off + 10) == (uint32_t)m->code.len);
  off += 14;

  v6_check(&fails, memcmp(out.data + off, m->code.data, m->code.len) == 0);
  off += m->code.len;

  v6_check(&fails, rd16(out.data + off) == 0);
  v6_check(&fails, rd16(out.data + off + 2) == 0);
  off += 4;

  v6_check(&fails, rd16(out.data + off) == 0);
  off += 2;

  v6_check(&fails, off == out.len);

  buf_free(&out);
  cf_free(&cf);
  return fails;
}
