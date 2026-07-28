#include "v6/bytecode.h"

#include <stdlib.h>
#include <string.h>

enum {
  cp_utf8 = 1,
  cp_integer = 3,
  cp_double = 6,
  cp_class = 7,
  cp_string = 8,
  cp_fieldref = 9,
  cp_methodref = 10,
  cp_name_type = 12,
};

uint16_t cf_utf8(class_file* cf, const char* s) {
  size_t n = strlen(s);
  cf->cp_count++;
  uint16_t idx = cf->cp_count;
  buf_u8(&cf->cp, cp_utf8);
  buf_u16(&cf->cp, (uint16_t)n);
  buf_bytes(&cf->cp, (const uint8_t*)s, n);
  return idx;
}

uint16_t cf_class(class_file* cf, const char* name) {
  uint16_t name_idx = cf_utf8(cf, name);
  cf->cp_count++;
  uint16_t idx = cf->cp_count;
  buf_u8(&cf->cp, cp_class);
  buf_u16(&cf->cp, name_idx);
  return idx;
}

uint16_t cf_name_type(class_file* cf, const char* name, const char* desc) {
  uint16_t name_idx = cf_utf8(cf, name);
  uint16_t desc_idx = cf_utf8(cf, desc);
  cf->cp_count++;
  uint16_t idx = cf->cp_count;
  buf_u8(&cf->cp, cp_name_type);
  buf_u16(&cf->cp, name_idx);
  buf_u16(&cf->cp, desc_idx);
  return idx;
}

uint16_t cf_methodref(class_file* cf, const char* cls, const char* name,
                      const char* desc) {
  uint16_t cls_idx = cf_class(cf, cls);
  uint16_t nt_idx = cf_name_type(cf, name, desc);
  cf->cp_count++;
  uint16_t idx = cf->cp_count;
  buf_u8(&cf->cp, cp_methodref);
  buf_u16(&cf->cp, cls_idx);
  buf_u16(&cf->cp, nt_idx);
  return idx;
}

uint16_t cf_fieldref(class_file* cf, const char* cls, const char* name,
                     const char* desc) {
  uint16_t cls_idx = cf_class(cf, cls);
  uint16_t nt_idx = cf_name_type(cf, name, desc);
  cf->cp_count++;
  uint16_t idx = cf->cp_count;
  buf_u8(&cf->cp, cp_fieldref);
  buf_u16(&cf->cp, cls_idx);
  buf_u16(&cf->cp, nt_idx);
  return idx;
}

uint16_t cf_string(class_file* cf, const char* s) {
  uint16_t str_idx = cf_utf8(cf, s);
  cf->cp_count++;
  uint16_t idx = cf->cp_count;
  buf_u8(&cf->cp, cp_string);
  buf_u16(&cf->cp, str_idx);
  return idx;
}

uint16_t cf_integer(class_file* cf, int32_t v) {
  cf->cp_count++;
  uint16_t idx = cf->cp_count;
  buf_u8(&cf->cp, cp_integer);
  buf_u32(&cf->cp, (uint32_t)v);
  return idx;
}

uint16_t cf_double(class_file* cf, double v) {
  uint64_t bits;
  memcpy(&bits, &v, sizeof(bits));
  cf->cp_count++;
  uint16_t idx = cf->cp_count;
  buf_u8(&cf->cp, cp_double);
  buf_u32(&cf->cp, (uint32_t)(bits >> 32));
  buf_u32(&cf->cp, (uint32_t)bits);
  cf->cp_count++;
  return idx;
}

void cf_init(class_file* cf, const char* this_name, const char* super_name) {
  buf_init(&cf->cp);
  cf->cp_count = 0;
  cf->access = acc_public | acc_super;
  cf->this_idx = cf_class(cf, this_name);
  cf->super_idx = cf_class(cf, super_name);
  cf->code_utf8 = cf_utf8(cf, "Code");
  cf->methods = NULL;
  cf->method_len = 0;
  cf->method_cap = 0;
  cf->fields = NULL;
  cf->field_len = 0;
  cf->field_cap = 0;
}

void cf_free(class_file* cf) {
  for (size_t i = 0; i < cf->method_len; i++) {
    buf_free(&cf->methods[i]->code);
    free(cf->methods[i]);
  }
  free(cf->methods);
  free(cf->fields);
  buf_free(&cf->cp);
}

method* cf_method(class_file* cf, uint16_t access, const char* name,
                  const char* desc) {
  if (cf->method_len == cf->method_cap) {
    cf->method_cap = cf->method_cap ? cf->method_cap * 2 : 4;
    cf->methods = realloc(cf->methods, cf->method_cap * sizeof(method*));
  }
  method* m = malloc(sizeof(method));
  m->access = access;
  m->name_idx = cf_utf8(cf, name);
  m->desc_idx = cf_utf8(cf, desc);
  m->max_stack = 0;
  m->max_locals = 0;
  buf_init(&m->code);
  cf->methods[cf->method_len++] = m;
  return m;
}

void cf_field(class_file* cf, uint16_t access, const char* name,
              const char* desc) {
  if (cf->field_len == cf->field_cap) {
    cf->field_cap = cf->field_cap ? cf->field_cap * 2 : 4;
    cf->fields = realloc(cf->fields, cf->field_cap * sizeof(field));
  }
  field* f = &cf->fields[cf->field_len++];
  f->access = access;
  f->name_idx = cf_utf8(cf, name);
  f->desc_idx = cf_utf8(cf, desc);
}

void op_emit(method* m, uint8_t code) {
  buf_u8(&m->code, code);
}

void op_emit1(method* m, uint8_t code, uint8_t a) {
  buf_u8(&m->code, code);
  buf_u8(&m->code, a);
}

void op_emit2(method* m, uint8_t code, uint16_t a) {
  buf_u8(&m->code, code);
  buf_u16(&m->code, a);
}

size_t op_pos(method* m) {
  return m->code.len;
}

void op_patch2(method* m, size_t at, uint16_t v) {
  m->code.data[at] = (uint8_t)(v >> 8);
  m->code.data[at + 1] = (uint8_t)v;
}

void cf_emit(class_file* cf, buf* out) {
  buf_u32(out, 0xcafebabeu);
  buf_u16(out, 0);
  buf_u16(out, 52);
  buf_u16(out, (uint16_t)(cf->cp_count + 1));
  buf_bytes(out, cf->cp.data, cf->cp.len);
  buf_u16(out, cf->access);
  buf_u16(out, cf->this_idx);
  buf_u16(out, cf->super_idx);
  buf_u16(out, 0);
  buf_u16(out, (uint16_t)cf->field_len);
  for (size_t i = 0; i < cf->field_len; i++) {
    field* f = &cf->fields[i];
    buf_u16(out, f->access);
    buf_u16(out, f->name_idx);
    buf_u16(out, f->desc_idx);
    buf_u16(out, 0);
  }
  buf_u16(out, (uint16_t)cf->method_len);
  for (size_t i = 0; i < cf->method_len; i++) {
    method* m = cf->methods[i];
    uint32_t code_attr_len = 2 + 2 + 4 + (uint32_t)m->code.len + 2 + 2;
    buf_u16(out, m->access);
    buf_u16(out, m->name_idx);
    buf_u16(out, m->desc_idx);
    buf_u16(out, 1);
    buf_u16(out, cf->code_utf8);
    buf_u32(out, code_attr_len);
    buf_u16(out, m->max_stack);
    buf_u16(out, m->max_locals);
    buf_u32(out, (uint32_t)m->code.len);
    buf_bytes(out, m->code.data, m->code.len);
    buf_u16(out, 0);
    buf_u16(out, 0);
  }
  buf_u16(out, 0);
}
