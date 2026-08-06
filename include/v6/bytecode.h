#pragma once

#include "v6/buffer.h"

#include <stddef.h>
#include <stdint.h>

enum {
  V6_TAG_NUM = 0,
  V6_TAG_BOOL = 1,
  V6_TAG_NULL = 2,
  V6_TAG_UNDEF = 3,
  V6_TAG_OBJ = 4,
  V6_TAG_STR = 5,
  V6_TAG_FUNC = 6,
  V6_TAG_BIGINT = 7,
};

typedef enum {
  op_nop = 0x00,
  op_aconst_null = 0x01,
  op_iconst_m1 = 0x02,
  op_iconst_0 = 0x03,
  op_iconst_1 = 0x04,
  op_iconst_2 = 0x05,
  op_iconst_3 = 0x06,
  op_iconst_4 = 0x07,
  op_iconst_5 = 0x08,
  op_dconst_0 = 0x0e,
  op_dconst_1 = 0x0f,
  op_bipush = 0x10,
  op_sipush = 0x11,
  op_ldc = 0x12,
  op_ldc_w = 0x13,
  op_ldc2_w = 0x14,
  op_aaload = 0x32,
  op_aastore = 0x53,
  op_anewarray = 0xbd,
  op_arraylength = 0xbe,
  op_iload = 0x15,
  op_dload = 0x18,
  op_aload = 0x19,
  op_iload_0 = 0x1a,
  op_iload_1 = 0x1b,
  op_iload_2 = 0x1c,
  op_iload_3 = 0x1d,
  op_dload_0 = 0x26,
  op_dload_1 = 0x27,
  op_dload_2 = 0x28,
  op_dload_3 = 0x29,
  op_aload_0 = 0x2a,
  op_aload_1 = 0x2b,
  op_aload_2 = 0x2c,
  op_aload_3 = 0x2d,
  op_istore = 0x36,
  op_dstore = 0x39,
  op_astore = 0x3a,
  op_istore_0 = 0x3b,
  op_istore_1 = 0x3c,
  op_istore_2 = 0x3d,
  op_istore_3 = 0x3e,
  op_dstore_0 = 0x47,
  op_dstore_1 = 0x48,
  op_dstore_2 = 0x49,
  op_dstore_3 = 0x4a,
  op_astore_0 = 0x4b,
  op_astore_1 = 0x4c,
  op_astore_2 = 0x4d,
  op_astore_3 = 0x4e,
  op_dup = 0x59,
  op_dup_x1 = 0x5a,
  op_dup2 = 0x5c,
  op_pop = 0x57,
  op_swap = 0x5f,
  op_iadd = 0x60,
  op_dadd = 0x63,
  op_isub = 0x64,
  op_dsub = 0x67,
  op_imul = 0x68,
  op_dmul = 0x6b,
  op_idiv = 0x6c,
  op_ddiv = 0x6f,
  op_drem = 0x73,
  op_dneg = 0x77,
  op_i2d = 0x87,
  op_d2i = 0x8e,
  op_ifeq = 0x99,
  op_ifne = 0x9a,
  op_iflt = 0x9b,
  op_ifge = 0x9c,
  op_ifgt = 0x9d,
  op_ifle = 0x9e,
  op_if_icmpeq = 0x9f,
  op_if_icmpne = 0xa0,
  op_dcmpl = 0x97,
  op_dcmpg = 0x98,
  op_goto = 0xa7,
  op_ifnull = 0xc6,
  op_ifnonnull = 0xc7,
  op_ireturn = 0xac,
  op_dreturn = 0xaf,
  op_areturn = 0xb0,
  op_return = 0xb1,
  op_getstatic = 0xb2,
  op_putstatic = 0xb3,
  op_getfield = 0xb4,
  op_putfield = 0xb5,
  op_invokevirtual = 0xb6,
  op_invokespecial = 0xb7,
  op_invokestatic = 0xb8,
  op_invokeinterface = 0xb9,
  op_new = 0xbb,
  op_checkcast = 0xc0,
  op_instanceof = 0xc1,
  op_athrow = 0xbf,
  op_ixor = 0x82,
  op_dup_x2 = 0x5b,
  op_wide = 0xc4,
  op_ineg = 0x74,
  op_ishl = 0x78,
  op_ishr = 0x7a,
  op_iushr = 0x7c,
  op_iand = 0x7e,
  op_ior = 0x80,
} opcode;

enum {
  acc_public = 0x0001,
  acc_static = 0x0008,
  acc_super = 0x0020,
};

typedef struct exc_entry {
  uint16_t start_pc;
  uint16_t end_pc;
  uint16_t handler_pc;
  uint16_t catch_type;
} exc_entry;

typedef struct method {
  uint16_t access;
  uint16_t name_idx;
  uint16_t desc_idx;
  uint16_t max_stack;
  uint16_t max_locals;
  buf code;
  exc_entry* exceptions;
  size_t exception_len;
  size_t exception_cap;
} method;

typedef struct field {
  uint16_t access;
  uint16_t name_idx;
  uint16_t desc_idx;
} field;

typedef struct cp_utf8_entry {
  char* s;
  uint16_t idx;
} cp_utf8_entry;

typedef struct cp_class_entry {
  char* name;
  uint16_t idx;
} cp_class_entry;

typedef struct cp_nt_entry {
  char* name;
  char* desc;
  uint16_t idx;
} cp_nt_entry;

typedef struct cp_ref_entry {
  char* cls;
  char* name;
  char* desc;
  uint16_t idx;
} cp_ref_entry;

typedef struct cp_str_entry {
  char* s;
  uint16_t idx;
} cp_str_entry;

typedef struct cp_int_entry {
  int32_t v;
  uint16_t idx;
} cp_int_entry;

typedef struct cp_dbl_entry {
  double v;
  uint16_t idx;
} cp_dbl_entry;

typedef struct class_file {
  buf cp;
  uint16_t cp_count;
  uint16_t access;
  uint16_t this_idx;
  uint16_t super_idx;
  uint16_t code_utf8;
  method** methods;
  size_t method_len;
  size_t method_cap;
  field* fields;
  size_t field_len;
  size_t field_cap;
  cp_utf8_entry* utf8_cache;
  size_t utf8_cache_len, utf8_cache_cap;
  cp_class_entry* class_cache;
  size_t class_cache_len, class_cache_cap;
  cp_nt_entry* nt_cache;
  size_t nt_cache_len, nt_cache_cap;
  cp_ref_entry* methodref_cache;
  size_t methodref_cache_len, methodref_cache_cap;
  cp_ref_entry* fieldref_cache;
  size_t fieldref_cache_len, fieldref_cache_cap;
  cp_str_entry* str_cache;
  size_t str_cache_len, str_cache_cap;
  cp_int_entry* int_cache;
  size_t int_cache_len, int_cache_cap;
  cp_dbl_entry* dbl_cache;
  size_t dbl_cache_len, dbl_cache_cap;
} class_file;

void cf_init(class_file* cf, const char* this_name, const char* super_name);
void cf_free(class_file* cf);

uint16_t cf_utf8(class_file* cf, const char* s);
uint16_t cf_class(class_file* cf, const char* name);
uint16_t cf_name_type(class_file* cf, const char* name, const char* desc);
uint16_t cf_methodref(class_file* cf, const char* cls, const char* name,
                      const char* desc);
uint16_t cf_fieldref(class_file* cf, const char* cls, const char* name,
                     const char* desc);
uint16_t cf_string(class_file* cf, const char* s);
uint16_t cf_integer(class_file* cf, int32_t v);
uint16_t cf_double(class_file* cf, double v);

method* cf_method(class_file* cf, uint16_t access, const char* name,
                  const char* desc);
void cf_field(class_file* cf, uint16_t access, const char* name,
              const char* desc);

void op_emit(method* m, uint8_t code);
void op_emit1(method* m, uint8_t code, uint8_t a);
void op_emit2(method* m, uint8_t code, uint16_t a);
size_t op_pos(method* m);
void op_patch2(method* m, size_t at, uint16_t v);
void method_add_exception(method* m, uint16_t start_pc, uint16_t end_pc,
                          uint16_t handler_pc, uint16_t catch_type);

void cf_emit(class_file* cf, buf* out);

uint16_t value_ctor(class_file* cf);
uint16_t value_num_method(class_file* cf);
void emit_box_const(class_file* cf, method* m, uint8_t tag_op, uint8_t num_op);
void emit_const_singleton(class_file* cf, method* m, const char* field);
void emit_undef(class_file* cf, method* m);
uint16_t object_class(class_file* cf);
