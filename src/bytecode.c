#include "v6/bytecode.h"

void cf_init(class_file *cf, const char *this_name, const char *super_name) {
  (void)this_name;
  (void)super_name;
  buf_init(&cf->cp);
  cf->cp_count = 0;
  cf->access = 0;
  cf->this_idx = 0;
  cf->super_idx = 0;
  cf->code_utf8 = 0;
  cf->methods = NULL;
  cf->method_len = 0;
  cf->method_cap = 0;
}

void cf_free(class_file *cf) {
  buf_free(&cf->cp);
}

uint16_t cf_utf8(class_file *cf, const char *s) {
  (void)cf;
  (void)s;
  return 0;
}

uint16_t cf_class(class_file *cf, const char *name) {
  (void)cf;
  (void)name;
  return 0;
}

uint16_t cf_name_type(class_file *cf, const char *name, const char *desc) {
  (void)cf;
  (void)name;
  (void)desc;
  return 0;
}

uint16_t cf_methodref(class_file *cf, const char *cls, const char *name,
                       const char *desc) {
  (void)cf;
  (void)cls;
  (void)name;
  (void)desc;
  return 0;
}

uint16_t cf_fieldref(class_file *cf, const char *cls, const char *name,
                      const char *desc) {
  (void)cf;
  (void)cls;
  (void)name;
  (void)desc;
  return 0;
}

uint16_t cf_string(class_file *cf, const char *s) {
  (void)cf;
  (void)s;
  return 0;
}

uint16_t cf_integer(class_file *cf, int32_t v) {
  (void)cf;
  (void)v;
  return 0;
}

uint16_t cf_double(class_file *cf, double v) {
  (void)cf;
  (void)v;
  return 0;
}

method *cf_method(class_file *cf, uint16_t access, const char *name,
                   const char *desc) {
  (void)cf;
  (void)access;
  (void)name;
  (void)desc;
  return NULL;
}

void op_emit(method *m, uint8_t code) {
  (void)m;
  (void)code;
}

void op_emit1(method *m, uint8_t code, uint8_t a) {
  (void)m;
  (void)code;
  (void)a;
}

void op_emit2(method *m, uint8_t code, uint16_t a) {
  (void)m;
  (void)code;
  (void)a;
}

size_t op_pos(method *m) {
  (void)m;
  return 0;
}

void op_patch2(method *m, size_t at, uint16_t v) {
  (void)m;
  (void)at;
  (void)v;
}

void cf_emit(class_file *cf, buf *out) {
  (void)cf;
  (void)out;
}
