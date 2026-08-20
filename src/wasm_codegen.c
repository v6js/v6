#include "v6/wasm.h"

#include "v6/internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define wasm_max_ctrl_depth 1024
#define wasm_max_end_jumps 256
#define wasm_max_type_stack 4096

typedef struct wasm_ctrl_frame {
  int kind;
  int has_result;
  uint8_t result_type;
  size_t label_pos;
  size_t end_jumps[wasm_max_end_jumps];
  int end_jumps_count;
  size_t else_jump_pos;
  int has_else;
  int type_stack_base;
} wasm_ctrl_frame;

enum {
  wasm_ctrl_block = 0,
  wasm_ctrl_loop = 1,
  wasm_ctrl_if = 2,
};

typedef struct wasm_func_ctx {
  class_file* cf;
  method* m;
  wasm_module* mod;
  const char* class_name;
  uint8_t* local_types;
  uint16_t* local_slots;
  uint32_t local_count;
  uint32_t param_count;
  uint16_t next_slot;
  uint8_t result_type;
  int has_result;
  wasm_ctrl_frame frames[wasm_max_ctrl_depth];
  int frame_depth;
  uint8_t type_stack[wasm_max_type_stack];
  int type_stack_len;
  int had_error;
  char err_msg[256];
} wasm_func_ctx;

static void wf_error(wasm_func_ctx* fc, const char* msg) {
  if (fc->had_error)
    return;
  fc->had_error = 1;
  snprintf(fc->err_msg, sizeof(fc->err_msg), "%s", msg);
}

static void ts_push(wasm_func_ctx* fc, uint8_t vt) {
  if (fc->type_stack_len < wasm_max_type_stack)
    fc->type_stack[fc->type_stack_len++] = vt;
}

static uint8_t ts_pop(wasm_func_ctx* fc) {
  if (fc->type_stack_len <= 0)
    return wasm_type_i32;
  return fc->type_stack[--fc->type_stack_len];
}

static uint8_t ts_peek(wasm_func_ctx* fc) {
  if (fc->type_stack_len <= 0)
    return wasm_type_i32;
  return fc->type_stack[fc->type_stack_len - 1];
}

static int slot_width(uint8_t vt) {
  return (vt == wasm_type_i64 || vt == wasm_type_f64) ? 2 : 1;
}

static const char* jvm_desc_for(uint8_t vt) {
  switch (vt) {
  case wasm_type_i32:
    return "I";
  case wasm_type_i64:
    return "J";
  case wasm_type_f32:
    return "F";
  case wasm_type_f64:
    return "D";
  default:
    return "I";
  }
}

static uint8_t wasm_global_type(wasm_module* mod, uint32_t idx) {
  if (idx < mod->imported_global_count) {
    uint32_t seen = 0;
    for (uint32_t i = 0; i < mod->import_count; i++) {
      if (mod->imports[i].kind != wasm_import_global)
        continue;
      if (seen == idx)
        return mod->imports[i].global_type;
      seen++;
    }
    return wasm_type_i32;
  }
  return mod->globals[idx - mod->imported_global_count].val_type;
}

static uint16_t wasm_global_field(wasm_func_ctx* fc, uint32_t idx) {
  char name[32];
  snprintf(name, sizeof(name), "wasmGlobal%u", idx);
  uint8_t vt = wasm_global_type(fc->mod, idx);
  return cf_fieldref(fc->cf, fc->class_name, name, jvm_desc_for(vt));
}

static uint16_t wasm_memory_field(wasm_func_ctx* fc) {
  return cf_fieldref(fc->cf, fc->class_name, "wasmMemory0", "LV6WasmMemory;");
}

static uint16_t wasm_table_field(wasm_func_ctx* fc) {
  return cf_fieldref(fc->cf, fc->class_name, "wasmTable0", "LV6WasmTable;");
}

static void build_func_desc(const wasm_functype* ft, char* out, size_t cap) {
  size_t p = 0;
  out[p++] = '(';
  for (uint32_t i = 0; i < ft->param_count && p < cap - 3; i++) {
    const char* d = jvm_desc_for(ft->params[i]);
    out[p++] = d[0];
  }
  out[p++] = ')';
  if (ft->result_count == 0) {
    out[p++] = 'V';
  } else {
    const char* d = jvm_desc_for(ft->results[0]);
    out[p++] = d[0];
  }
  out[p] = '\0';
}

static void build_indirect_desc(const wasm_functype* ft, char* out,
                                size_t cap) {
  size_t p = 0;
  out[p++] = '(';
  for (uint32_t i = 0; i < ft->param_count && p < cap - 4; i++) {
    const char* d = jvm_desc_for(ft->params[i]);
    out[p++] = d[0];
  }
  out[p++] = 'I';
  out[p++] = ')';
  if (ft->result_count == 0) {
    out[p++] = 'V';
  } else {
    const char* d = jvm_desc_for(ft->results[0]);
    out[p++] = d[0];
  }
  out[p] = '\0';
}

static void emit_load_local(wasm_func_ctx* fc, uint32_t idx) {
  uint8_t vt = fc->local_types[idx];
  uint16_t slot = fc->local_slots[idx];
  switch (vt) {
  case wasm_type_i32:
    emit_iload_slot(fc->m, slot);
    break;
  case wasm_type_i64:
    emit_lload(fc->m, slot);
    break;
  case wasm_type_f32:
    emit_fload(fc->m, slot);
    break;
  case wasm_type_f64:
    emit_dload(fc->m, slot);
    break;
  }
  ts_push(fc, vt);
}

static void emit_store_local(wasm_func_ctx* fc, uint32_t idx) {
  uint8_t vt = fc->local_types[idx];
  uint16_t slot = fc->local_slots[idx];
  ts_pop(fc);
  switch (vt) {
  case wasm_type_i32:
    emit_istore(fc->m, slot);
    break;
  case wasm_type_i64:
    emit_lstore(fc->m, slot);
    break;
  case wasm_type_f32:
    emit_fstore(fc->m, slot);
    break;
  case wasm_type_f64:
    emit_dstore(fc->m, slot);
    break;
  }
}

static void push_frame(wasm_func_ctx* fc, int kind, int has_result,
                       uint8_t result_type, size_t label_pos) {
  if (fc->frame_depth >= wasm_max_ctrl_depth) {
    wf_error(fc, "control stack overflow");
    return;
  }
  wasm_ctrl_frame* f = &fc->frames[fc->frame_depth++];
  f->kind = kind;
  f->has_result = has_result;
  f->result_type = result_type;
  f->label_pos = label_pos;
  f->end_jumps_count = 0;
  f->has_else = 0;
  f->type_stack_base = fc->type_stack_len;
}

static void add_end_jump(wasm_func_ctx* fc, wasm_ctrl_frame* f, size_t pos) {
  if (f->end_jumps_count >= wasm_max_end_jumps) {
    wf_error(fc, "too many branch targets in one block");
    return;
  }
  f->end_jumps[f->end_jumps_count++] = pos;
}

static void patch_end_jumps(wasm_func_ctx* fc, wasm_ctrl_frame* f) {
  size_t here = op_pos(fc->m);
  for (int i = 0; i < f->end_jumps_count; i++)
    op_patch2(fc->m, (uint16_t)(f->end_jumps[i] + 1),
              (uint16_t)(here - f->end_jumps[i]));
}

static wasm_ctrl_frame* frame_at_depth(wasm_func_ctx* fc, uint32_t depth) {
  int idx = fc->frame_depth - 1 - (int)depth;
  if (idx < 0)
    return NULL;
  return &fc->frames[idx];
}

static void emit_branch_to(wasm_func_ctx* fc, uint32_t depth) {
  wasm_ctrl_frame* f = frame_at_depth(fc, depth);
  if (!f) {
    wf_error(fc, "invalid branch depth");
    return;
  }
  size_t here = op_pos(fc->m);
  op_emit2(fc->m, op_goto, 0);
  if (f->kind == wasm_ctrl_loop)
    op_patch2(fc->m, (uint16_t)(here + 1), (uint16_t)(f->label_pos - here));
  else
    add_end_jump(fc, f, here);
}

typedef struct wasm_reader2 {
  const uint8_t* buf;
  size_t len;
  size_t pos;
  int failed;
} wasm_reader2;

static uint8_t w2_u8(wasm_reader2* r) {
  if (r->failed || r->pos >= r->len) {
    r->failed = 1;
    return 0;
  }
  return r->buf[r->pos++];
}

static uint32_t w2_u32(wasm_reader2* r) {
  uint32_t result = 0;
  int shift = 0;
  for (;;) {
    if (r->failed)
      return 0;
    uint8_t byte = w2_u8(r);
    result |= (uint32_t)(byte & 0x7F) << shift;
    if ((byte & 0x80) == 0)
      break;
    shift += 7;
    if (shift >= 35) {
      r->failed = 1;
      return 0;
    }
  }
  return result;
}

static int64_t w2_i64(wasm_reader2* r) {
  int64_t result = 0;
  int shift = 0;
  uint8_t byte;
  for (;;) {
    if (r->failed)
      return 0;
    byte = w2_u8(r);
    result |= (int64_t)(byte & 0x7F) << shift;
    shift += 7;
    if ((byte & 0x80) == 0)
      break;
    if (shift >= 70) {
      r->failed = 1;
      return 0;
    }
  }
  if (shift < 64 && (byte & 0x40))
    result |= -((int64_t)1 << shift);
  return result;
}

static int32_t w2_i32(wasm_reader2* r) {
  return (int32_t)w2_i64(r);
}

static float w2_f32(wasm_reader2* r) {
  if (r->failed || r->pos + 4 > r->len) {
    r->failed = 1;
    return 0;
  }
  uint32_t bits = (uint32_t)r->buf[r->pos] |
                  ((uint32_t)r->buf[r->pos + 1] << 8) |
                  ((uint32_t)r->buf[r->pos + 2] << 16) |
                  ((uint32_t)r->buf[r->pos + 3] << 24);
  r->pos += 4;
  float f;
  memcpy(&f, &bits, 4);
  return f;
}

static double w2_f64(wasm_reader2* r) {
  if (r->failed || r->pos + 8 > r->len) {
    r->failed = 1;
    return 0;
  }
  uint64_t bits = 0;
  for (int i = 0; i < 8; i++)
    bits |= (uint64_t)r->buf[r->pos + i] << (8 * i);
  r->pos += 8;
  double d;
  memcpy(&d, &bits, 8);
  return d;
}

static uint16_t sm(wasm_func_ctx* fc, const char* name, const char* desc) {
  return cf_methodref(fc->cf, "V6Wasm", name, desc);
}

static void emit_mem_load(wasm_func_ctx* fc, uint32_t offset,
                          const char* method_name, const char* desc,
                          uint8_t result_type) {
  method* m = fc->m;
  uint16_t addr_scratch = fc->next_slot;
  ts_pop(fc);
  emit_istore(m, addr_scratch);
  op_emit2(m, op_getstatic, wasm_memory_field(fc));
  emit_iload_slot(m, addr_scratch);
  emit_iconst(m, (int)offset);
  op_emit(m, op_iadd);
  op_emit2(m, op_invokevirtual,
           cf_methodref(fc->cf, "V6WasmMemory", method_name, desc));
  ts_push(fc, result_type);
}

static void emit_mem_store(wasm_func_ctx* fc, uint32_t offset,
                           const char* method_name, const char* desc,
                           uint8_t value_type) {
  method* m = fc->m;
  uint16_t addr_scratch = fc->next_slot;
  uint16_t value_scratch = fc->next_slot + 1;
  ts_pop(fc);
  switch (value_type) {
  case wasm_type_i32:
    emit_istore(m, value_scratch);
    break;
  case wasm_type_i64:
    emit_lstore(m, value_scratch);
    break;
  case wasm_type_f32:
    emit_fstore(m, value_scratch);
    break;
  case wasm_type_f64:
    emit_dstore(m, value_scratch);
    break;
  }
  ts_pop(fc);
  emit_istore(m, addr_scratch);
  op_emit2(m, op_getstatic, wasm_memory_field(fc));
  emit_iload_slot(m, addr_scratch);
  emit_iconst(m, (int)offset);
  op_emit(m, op_iadd);
  switch (value_type) {
  case wasm_type_i32:
    emit_iload_slot(m, value_scratch);
    break;
  case wasm_type_i64:
    emit_lload(m, value_scratch);
    break;
  case wasm_type_f32:
    emit_fload(m, value_scratch);
    break;
  case wasm_type_f64:
    emit_dload(m, value_scratch);
    break;
  }
  op_emit2(m, op_invokevirtual,
           cf_methodref(fc->cf, "V6WasmMemory", method_name, desc));
}

static void codegen_instr(wasm_func_ctx* fc, wasm_reader2* r);
static void codegen_numeric(wasm_func_ctx* fc, uint8_t op);

static void codegen_body(wasm_func_ctx* fc, const uint8_t* start, size_t len) {
  wasm_reader2 r;
  r.buf = start;
  r.len = len;
  r.pos = 0;
  while (r.pos < r.len && !r.failed && !fc->had_error)
    codegen_instr(fc, &r);
}

static void binop(wasm_func_ctx* fc, uint8_t jvm_op, uint8_t vt) {
  ts_pop(fc);
  ts_pop(fc);
  op_emit(fc->m, jvm_op);
  ts_push(fc, vt);
}

static void unop_same(wasm_func_ctx* fc, uint8_t jvm_op) {
  uint8_t vt = ts_pop(fc);
  op_emit(fc->m, jvm_op);
  ts_push(fc, vt);
}

static void static_binop(wasm_func_ctx* fc, const char* cls, const char* name,
                         const char* desc, uint8_t vt) {
  ts_pop(fc);
  ts_pop(fc);
  op_emit2(fc->m, op_invokestatic, cf_methodref(fc->cf, cls, name, desc));
  ts_push(fc, vt);
}

static void bool_from_cond_jump(wasm_func_ctx* fc, uint8_t cond_op) {
  method* m = fc->m;
  size_t j1 = op_pos(m);
  op_emit2(m, cond_op, 0);
  emit_iconst(m, 0);
  size_t j2 = op_pos(m);
  op_emit2(m, op_goto, 0);
  size_t true_pos = op_pos(m);
  op_patch2(m, (uint16_t)(j1 + 1), (uint16_t)(true_pos - j1));
  emit_iconst(m, 1);
  size_t end_pos = op_pos(m);
  op_patch2(m, (uint16_t)(j2 + 1), (uint16_t)(end_pos - j2));
}

static void i32_cmp(wasm_func_ctx* fc, uint8_t if_icmp_op) {
  ts_pop(fc);
  ts_pop(fc);
  bool_from_cond_jump(fc, if_icmp_op);
  ts_push(fc, wasm_type_i32);
}

static void i32_cmp_unsigned(wasm_func_ctx* fc, uint8_t if_vs_zero_op) {
  ts_pop(fc);
  ts_pop(fc);
  op_emit2(
      fc->m, op_invokestatic,
      cf_methodref(fc->cf, "java/lang/Integer", "compareUnsigned", "(II)I"));
  bool_from_cond_jump(fc, if_vs_zero_op);
  ts_push(fc, wasm_type_i32);
}

static void i64_cmp(wasm_func_ctx* fc, uint8_t if_vs_zero_op) {
  ts_pop(fc);
  ts_pop(fc);
  op_emit(fc->m, op_lcmp);
  bool_from_cond_jump(fc, if_vs_zero_op);
  ts_push(fc, wasm_type_i32);
}

static void i64_cmp_unsigned(wasm_func_ctx* fc, uint8_t if_vs_zero_op) {
  ts_pop(fc);
  ts_pop(fc);
  op_emit2(fc->m, op_invokestatic,
           cf_methodref(fc->cf, "java/lang/Long", "compareUnsigned", "(JJ)I"));
  bool_from_cond_jump(fc, if_vs_zero_op);
  ts_push(fc, wasm_type_i32);
}

static void f32_cmp(wasm_func_ctx* fc, uint8_t cmp_op, uint8_t if_vs_zero_op) {
  ts_pop(fc);
  ts_pop(fc);
  op_emit(fc->m, cmp_op);
  bool_from_cond_jump(fc, if_vs_zero_op);
  ts_push(fc, wasm_type_i32);
}

static void f64_cmp(wasm_func_ctx* fc, uint8_t cmp_op, uint8_t if_vs_zero_op) {
  ts_pop(fc);
  ts_pop(fc);
  op_emit(fc->m, cmp_op);
  bool_from_cond_jump(fc, if_vs_zero_op);
  ts_push(fc, wasm_type_i32);
}

static void codegen_numeric(wasm_func_ctx* fc, uint8_t op) {
  method* m = fc->m;
  class_file* cf = fc->cf;

  switch (op) {
  case 0x45:
    ts_pop(fc);
    bool_from_cond_jump(fc, op_ifeq);
    ts_push(fc, wasm_type_i32);
    break;
  case 0x46:
    i32_cmp(fc, op_if_icmpeq);
    break;
  case 0x47:
    i32_cmp(fc, op_if_icmpne);
    break;
  case 0x48:
    i32_cmp(fc, op_if_icmplt);
    break;
  case 0x49:
    i32_cmp_unsigned(fc, op_iflt);
    break;
  case 0x4A:
    i32_cmp(fc, op_if_icmpgt);
    break;
  case 0x4B:
    i32_cmp_unsigned(fc, op_ifgt);
    break;
  case 0x4C:
    i32_cmp(fc, op_if_icmple);
    break;
  case 0x4D:
    i32_cmp_unsigned(fc, op_ifle);
    break;
  case 0x4E:
    i32_cmp(fc, op_if_icmpge);
    break;
  case 0x4F:
    i32_cmp_unsigned(fc, op_ifge);
    break;

  case 0x50:
    ts_pop(fc);
    op_emit2(m, op_ldc2_w, cf_long(cf, 0));
    op_emit(m, op_lcmp);
    bool_from_cond_jump(fc, op_ifeq);
    ts_push(fc, wasm_type_i32);
    break;
  case 0x51:
    i64_cmp(fc, op_ifeq);
    break;
  case 0x52:
    i64_cmp(fc, op_ifne);
    break;
  case 0x53:
    i64_cmp(fc, op_iflt);
    break;
  case 0x54:
    i64_cmp_unsigned(fc, op_iflt);
    break;
  case 0x55:
    i64_cmp(fc, op_ifgt);
    break;
  case 0x56:
    i64_cmp_unsigned(fc, op_ifgt);
    break;
  case 0x57:
    i64_cmp(fc, op_ifle);
    break;
  case 0x58:
    i64_cmp_unsigned(fc, op_ifle);
    break;
  case 0x59:
    i64_cmp(fc, op_ifge);
    break;
  case 0x5A:
    i64_cmp_unsigned(fc, op_ifge);
    break;

  case 0x5B:
    f32_cmp(fc, op_fcmpl, op_ifeq);
    break;
  case 0x5C:
    f32_cmp(fc, op_fcmpl, op_ifne);
    break;
  case 0x5D:
    f32_cmp(fc, op_fcmpg, op_iflt);
    break;
  case 0x5E:
    f32_cmp(fc, op_fcmpl, op_ifgt);
    break;
  case 0x5F:
    f32_cmp(fc, op_fcmpg, op_ifle);
    break;
  case 0x60:
    f32_cmp(fc, op_fcmpl, op_ifge);
    break;

  case 0x61:
    f64_cmp(fc, op_dcmpl, op_ifeq);
    break;
  case 0x62:
    f64_cmp(fc, op_dcmpl, op_ifne);
    break;
  case 0x63:
    f64_cmp(fc, op_dcmpg, op_iflt);
    break;
  case 0x64:
    f64_cmp(fc, op_dcmpl, op_ifgt);
    break;
  case 0x65:
    f64_cmp(fc, op_dcmpg, op_ifle);
    break;
  case 0x66:
    f64_cmp(fc, op_dcmpl, op_ifge);
    break;

  case 0x67:
    op_emit2(
        m, op_invokestatic,
        cf_methodref(cf, "java/lang/Integer", "numberOfLeadingZeros", "(I)I"));
    break;
  case 0x68:
    op_emit2(
        m, op_invokestatic,
        cf_methodref(cf, "java/lang/Integer", "numberOfTrailingZeros", "(I)I"));
    break;
  case 0x69:
    op_emit2(m, op_invokestatic,
             cf_methodref(cf, "java/lang/Integer", "bitCount", "(I)I"));
    break;
  case 0x6A:
    binop(fc, op_iadd, wasm_type_i32);
    break;
  case 0x6B:
    binop(fc, op_isub, wasm_type_i32);
    break;
  case 0x6C:
    binop(fc, op_imul, wasm_type_i32);
    break;
  case 0x6D:
    binop(fc, op_idiv, wasm_type_i32);
    break;
  case 0x6E:
    static_binop(fc, "java/lang/Integer", "divideUnsigned", "(II)I",
                 wasm_type_i32);
    break;
  case 0x6F:
    binop(fc, op_irem, wasm_type_i32);
    break;
  case 0x70:
    static_binop(fc, "java/lang/Integer", "remainderUnsigned", "(II)I",
                 wasm_type_i32);
    break;
  case 0x71:
    binop(fc, op_iand, wasm_type_i32);
    break;
  case 0x72:
    binop(fc, op_ior, wasm_type_i32);
    break;
  case 0x73:
    binop(fc, op_ixor, wasm_type_i32);
    break;
  case 0x74:
    binop(fc, op_ishl, wasm_type_i32);
    break;
  case 0x75:
    binop(fc, op_ishr, wasm_type_i32);
    break;
  case 0x76:
    binop(fc, op_iushr, wasm_type_i32);
    break;
  case 0x77:
    static_binop(fc, "java/lang/Integer", "rotateLeft", "(II)I", wasm_type_i32);
    break;
  case 0x78:
    static_binop(fc, "java/lang/Integer", "rotateRight", "(II)I",
                 wasm_type_i32);
    break;

  case 0x79:
    ts_pop(fc);
    op_emit2(
        m, op_invokestatic,
        cf_methodref(cf, "java/lang/Long", "numberOfLeadingZeros", "(J)I"));
    op_emit(m, op_i2l);
    ts_push(fc, wasm_type_i64);
    break;
  case 0x7A:
    ts_pop(fc);
    op_emit2(
        m, op_invokestatic,
        cf_methodref(cf, "java/lang/Long", "numberOfTrailingZeros", "(J)I"));
    op_emit(m, op_i2l);
    ts_push(fc, wasm_type_i64);
    break;
  case 0x7B:
    ts_pop(fc);
    op_emit2(m, op_invokestatic,
             cf_methodref(cf, "java/lang/Long", "bitCount", "(J)I"));
    op_emit(m, op_i2l);
    ts_push(fc, wasm_type_i64);
    break;
  case 0x7C:
    binop(fc, op_ladd, wasm_type_i64);
    break;
  case 0x7D:
    binop(fc, op_lsub, wasm_type_i64);
    break;
  case 0x7E:
    binop(fc, op_lmul, wasm_type_i64);
    break;
  case 0x7F:
    binop(fc, op_ldiv, wasm_type_i64);
    break;
  case 0x80:
    static_binop(fc, "java/lang/Long", "divideUnsigned", "(JJ)J",
                 wasm_type_i64);
    break;
  case 0x81:
    binop(fc, op_lrem, wasm_type_i64);
    break;
  case 0x82:
    static_binop(fc, "java/lang/Long", "remainderUnsigned", "(JJ)J",
                 wasm_type_i64);
    break;
  case 0x83:
    binop(fc, op_land, wasm_type_i64);
    break;
  case 0x84:
    binop(fc, op_lor, wasm_type_i64);
    break;
  case 0x85:
    binop(fc, op_lxor, wasm_type_i64);
    break;
  case 0x86:
    ts_pop(fc);
    op_emit(m, op_l2i);
    ts_pop(fc);
    op_emit(m, op_lshl);
    ts_push(fc, wasm_type_i64);
    break;
  case 0x87:
    ts_pop(fc);
    op_emit(m, op_l2i);
    ts_pop(fc);
    op_emit(m, op_lshr);
    ts_push(fc, wasm_type_i64);
    break;
  case 0x88:
    ts_pop(fc);
    op_emit(m, op_l2i);
    ts_pop(fc);
    op_emit(m, op_lushr);
    ts_push(fc, wasm_type_i64);
    break;
  case 0x89:
    static_binop(fc, "java/lang/Long", "rotateLeft", "(JI)J", wasm_type_i64);
    break;
  case 0x8A:
    static_binop(fc, "java/lang/Long", "rotateRight", "(JI)J", wasm_type_i64);
    break;

  case 0x8B:
    op_emit2(m, op_invokestatic,
             cf_methodref(cf, "java/lang/Math", "abs", "(F)F"));
    break;
  case 0x8C:
    unop_same(fc, op_fneg);
    break;
  case 0x8D:
    op_emit2(m, op_invokestatic, cf_methodref(cf, "V6Wasm", "f32ceil", "(F)F"));
    break;
  case 0x8E:
    op_emit2(m, op_invokestatic,
             cf_methodref(cf, "V6Wasm", "f32floor", "(F)F"));
    break;
  case 0x8F:
    op_emit2(m, op_invokestatic,
             cf_methodref(cf, "V6Wasm", "f32trunc", "(F)F"));
    break;
  case 0x90:
    op_emit2(m, op_invokestatic,
             cf_methodref(cf, "V6Wasm", "f32nearest", "(F)F"));
    break;
  case 0x91:
    op_emit2(m, op_invokestatic, cf_methodref(cf, "V6Wasm", "f32sqrt", "(F)F"));
    break;
  case 0x92:
    binop(fc, op_fadd, wasm_type_f32);
    break;
  case 0x93:
    binop(fc, op_fsub, wasm_type_f32);
    break;
  case 0x94:
    binop(fc, op_fmul, wasm_type_f32);
    break;
  case 0x95:
    binop(fc, op_fdiv, wasm_type_f32);
    break;
  case 0x96:
    static_binop(fc, "java/lang/Math", "min", "(FF)F", wasm_type_f32);
    break;
  case 0x97:
    static_binop(fc, "java/lang/Math", "max", "(FF)F", wasm_type_f32);
    break;
  case 0x98:
    static_binop(fc, "java/lang/Math", "copySign", "(FF)F", wasm_type_f32);
    break;

  case 0x99:
    op_emit2(m, op_invokestatic,
             cf_methodref(cf, "java/lang/Math", "abs", "(D)D"));
    break;
  case 0x9A:
    unop_same(fc, op_dneg);
    break;
  case 0x9B:
    op_emit2(m, op_invokestatic,
             cf_methodref(cf, "java/lang/Math", "ceil", "(D)D"));
    break;
  case 0x9C:
    op_emit2(m, op_invokestatic,
             cf_methodref(cf, "java/lang/Math", "floor", "(D)D"));
    break;
  case 0x9D:
    op_emit2(m, op_invokestatic,
             cf_methodref(cf, "V6Wasm", "f64trunc", "(D)D"));
    break;
  case 0x9E:
    op_emit2(m, op_invokestatic,
             cf_methodref(cf, "java/lang/Math", "rint", "(D)D"));
    break;
  case 0x9F:
    op_emit2(m, op_invokestatic,
             cf_methodref(cf, "java/lang/Math", "sqrt", "(D)D"));
    break;
  case 0xA0:
    binop(fc, op_dadd, wasm_type_f64);
    break;
  case 0xA1:
    binop(fc, op_dsub, wasm_type_f64);
    break;
  case 0xA2:
    binop(fc, op_dmul, wasm_type_f64);
    break;
  case 0xA3:
    binop(fc, op_ddiv, wasm_type_f64);
    break;
  case 0xA4:
    static_binop(fc, "java/lang/Math", "min", "(DD)D", wasm_type_f64);
    break;
  case 0xA5:
    static_binop(fc, "java/lang/Math", "max", "(DD)D", wasm_type_f64);
    break;
  case 0xA6:
    static_binop(fc, "java/lang/Math", "copySign", "(DD)D", wasm_type_f64);
    break;

  case 0xA7:
    ts_pop(fc);
    op_emit(m, op_l2i);
    ts_push(fc, wasm_type_i32);
    break;
  case 0xA8:
    ts_pop(fc);
    op_emit(m, op_f2i);
    ts_push(fc, wasm_type_i32);
    break;
  case 0xAA:
    ts_pop(fc);
    op_emit(m, op_d2i);
    ts_push(fc, wasm_type_i32);
    break;
  case 0xAC:
    ts_pop(fc);
    op_emit(m, op_i2l);
    ts_push(fc, wasm_type_i64);
    break;
  case 0xAD:
    ts_pop(fc);
    op_emit2(m, op_ldc2_w, cf_long(cf, 0xFFFFFFFFL));
    op_emit(m, op_i2l);
    op_emit(m, op_land);
    ts_push(fc, wasm_type_i64);
    break;
  case 0xAE:
    ts_pop(fc);
    op_emit(m, op_f2l);
    ts_push(fc, wasm_type_i64);
    break;
  case 0xB0:
    ts_pop(fc);
    op_emit(m, op_d2l);
    ts_push(fc, wasm_type_i64);
    break;
  case 0xB2:
    ts_pop(fc);
    op_emit(m, op_i2f);
    ts_push(fc, wasm_type_f32);
    break;
  case 0xB4:
    ts_pop(fc);
    op_emit(m, op_l2f);
    ts_push(fc, wasm_type_f32);
    break;
  case 0xB6:
    ts_pop(fc);
    op_emit(m, op_d2f);
    ts_push(fc, wasm_type_f32);
    break;
  case 0xB7:
    ts_pop(fc);
    op_emit(m, op_i2d);
    ts_push(fc, wasm_type_f64);
    break;
  case 0xB9:
    ts_pop(fc);
    op_emit(m, op_l2d);
    ts_push(fc, wasm_type_f64);
    break;
  case 0xBB:
    ts_pop(fc);
    op_emit(m, op_f2d);
    ts_push(fc, wasm_type_f64);
    break;
  case 0xBC:
    ts_pop(fc);
    op_emit2(m, op_invokestatic,
             cf_methodref(cf, "java/lang/Float", "floatToRawIntBits", "(F)I"));
    ts_push(fc, wasm_type_i32);
    break;
  case 0xBD:
    ts_pop(fc);
    op_emit2(
        m, op_invokestatic,
        cf_methodref(cf, "java/lang/Double", "doubleToRawLongBits", "(D)J"));
    ts_push(fc, wasm_type_i64);
    break;
  case 0xBE:
    ts_pop(fc);
    op_emit2(m, op_invokestatic,
             cf_methodref(cf, "java/lang/Float", "intBitsToFloat", "(I)F"));
    ts_push(fc, wasm_type_f32);
    break;
  case 0xBF:
    ts_pop(fc);
    op_emit2(m, op_invokestatic,
             cf_methodref(cf, "java/lang/Double", "longBitsToDouble", "(J)D"));
    ts_push(fc, wasm_type_f64);
    break;
  case 0xC0:
    ts_pop(fc);
    op_emit(m, op_i2b);
    ts_push(fc, wasm_type_i32);
    break;
  case 0xC1:
    ts_pop(fc);
    op_emit(m, op_i2s);
    ts_push(fc, wasm_type_i32);
    break;
  case 0xC2:
    ts_pop(fc);
    op_emit(m, op_l2i);
    op_emit(m, op_i2b);
    op_emit(m, op_i2l);
    ts_push(fc, wasm_type_i64);
    break;
  case 0xC3:
    ts_pop(fc);
    op_emit(m, op_l2i);
    op_emit(m, op_i2s);
    op_emit(m, op_i2l);
    ts_push(fc, wasm_type_i64);
    break;
  case 0xC4:
    ts_pop(fc);
    op_emit(m, op_l2i);
    op_emit(m, op_i2l);
    ts_push(fc, wasm_type_i64);
    break;

  default:
    wf_error(fc, "unsupported wasm numeric opcode in this build");
    break;
  }
}

static void codegen_instr(wasm_func_ctx* fc, wasm_reader2* r) {
  uint8_t op = w2_u8(r);
  method* m = fc->m;
  class_file* cf = fc->cf;

  switch (op) {
  case 0x00:
    op_emit2(m, op_invokestatic, sm(fc, "trapUnreachable", "()V"));
    break;
  case 0x01:
    op_emit(m, op_nop);
    break;
  case 0x02: {
    uint8_t bt = w2_u8(r);
    push_frame(fc, wasm_ctrl_block, bt != 0x40, bt, op_pos(m));
    break;
  }
  case 0x03: {
    uint8_t bt = w2_u8(r);
    push_frame(fc, wasm_ctrl_loop, bt != 0x40, bt, op_pos(m));
    break;
  }
  case 0x04: {
    uint8_t bt = w2_u8(r);
    ts_pop(fc);
    size_t else_jump = op_pos(m);
    op_emit2(m, op_ifeq, 0);
    push_frame(fc, wasm_ctrl_if, bt != 0x40, bt, op_pos(m));
    fc->frames[fc->frame_depth - 1].else_jump_pos = else_jump;
    break;
  }
  case 0x05: {
    wasm_ctrl_frame* f = &fc->frames[fc->frame_depth - 1];
    size_t end_jump = op_pos(m);
    op_emit2(m, op_goto, 0);
    add_end_jump(fc, f, end_jump);
    size_t here = op_pos(m);
    op_patch2(m, (uint16_t)(f->else_jump_pos + 1),
              (uint16_t)(here - f->else_jump_pos));
    f->has_else = 1;
    fc->type_stack_len = f->type_stack_base;
    break;
  }
  case 0x0B: {
    if (fc->frame_depth == 0)
      break;
    wasm_ctrl_frame* f = &fc->frames[fc->frame_depth - 1];
    if (f->kind == wasm_ctrl_if && !f->has_else) {
      size_t here = op_pos(m);
      op_patch2(m, (uint16_t)(f->else_jump_pos + 1),
                (uint16_t)(here - f->else_jump_pos));
    }
    patch_end_jumps(fc, f);
    fc->type_stack_len = f->type_stack_base;
    if (f->has_result)
      ts_push(fc, f->result_type);
    fc->frame_depth--;
    break;
  }
  case 0x0C: {
    uint32_t depth = w2_u32(r);
    emit_branch_to(fc, depth);
    break;
  }
  case 0x0D: {
    uint32_t depth = w2_u32(r);
    ts_pop(fc);
    size_t skip = op_pos(m);
    op_emit2(m, op_ifeq, 0);
    emit_branch_to(fc, depth);
    size_t here = op_pos(m);
    op_patch2(m, (uint16_t)(skip + 1), (uint16_t)(here - skip));
    break;
  }
  case 0x0E: {
    uint32_t n = w2_u32(r);
    uint32_t* targets = malloc(sizeof(uint32_t) * (n + 1));
    for (uint32_t i = 0; i < n; i++)
      targets[i] = w2_u32(r);
    uint32_t def = w2_u32(r);
    ts_pop(fc);
    uint16_t sel_slot = fc->next_slot;
    emit_istore(m, sel_slot);
    for (uint32_t i = 0; i < n; i++) {
      emit_iload_slot(m, sel_slot);
      emit_iconst(m, (int)i);
      size_t skip = op_pos(m);
      op_emit2(m, op_if_icmpne, 0);
      emit_branch_to(fc, targets[i]);
      size_t here = op_pos(m);
      op_patch2(m, (uint16_t)(skip + 1), (uint16_t)(here - skip));
    }
    emit_branch_to(fc, def);
    free(targets);
    break;
  }
  case 0x0F: {
    if (!fc->has_result) {
      op_emit(m, op_return);
    } else {
      switch (fc->result_type) {
      case wasm_type_i32:
        op_emit(m, op_ireturn);
        break;
      case wasm_type_i64:
        op_emit(m, op_lreturn);
        break;
      case wasm_type_f32:
        op_emit(m, op_freturn);
        break;
      case wasm_type_f64:
        op_emit(m, op_dreturn);
        break;
      }
    }
    break;
  }
  case 0x10: {
    uint32_t fidx = w2_u32(r);
    const wasm_functype* ft = wasm_func_type(fc->mod, fidx);
    char desc[512];
    build_func_desc(ft, desc, sizeof(desc));
    char fname[32];
    snprintf(fname, sizeof(fname), "wasmFunc%u", fidx);
    for (uint32_t i = 0; i < ft->param_count; i++)
      ts_pop(fc);
    uint16_t idx = cf_methodref(cf, fc->class_name, fname, desc);
    op_emit2(m, op_invokestatic, idx);
    if (ft->result_count > 0)
      ts_push(fc, ft->results[0]);
    break;
  }
  case 0x11: {
    uint32_t tidx = w2_u32(r);
    w2_u32(r);
    const wasm_functype* ft = &fc->mod->types[tidx];
    ts_pop(fc);
    for (uint32_t i = 0; i < ft->param_count; i++)
      ts_pop(fc);

    uint16_t idx_scratch = fc->next_slot;
    emit_istore(m, idx_scratch);
    op_emit2(m, op_getstatic, wasm_table_field(fc));
    emit_iload_slot(m, idx_scratch);
    op_emit2(m, op_invokevirtual,
             cf_methodref(cf, "V6WasmTable", "get", "(I)I"));

    char dispatch_desc[512];
    build_indirect_desc(ft, dispatch_desc, sizeof(dispatch_desc));
    char dispatch_name[32];
    snprintf(dispatch_name, sizeof(dispatch_name), "wasmIndirect%u", tidx);
    op_emit2(m, op_invokestatic,
             cf_methodref(cf, fc->class_name, dispatch_name, dispatch_desc));
    if (ft->result_count > 0)
      ts_push(fc, ft->results[0]);
    break;
  }
  case 0x1A:
    if (slot_width(ts_pop(fc)) == 2)
      op_emit(m, op_pop2);
    else
      op_emit(m, op_pop);
    break;
  case 0x1B: {
    ts_pop(fc);
    uint8_t vt = ts_peek(fc);
    ts_pop(fc);
    ts_pop(fc);
    const char* suffix = vt == wasm_type_i32   ? "I32"
                         : vt == wasm_type_i64 ? "I64"
                         : vt == wasm_type_f32 ? "F32"
                                               : "F64";
    char name[32];
    snprintf(name, sizeof(name), "select%s", suffix);
    char desc[32];
    snprintf(desc, sizeof(desc), "(%s%sI)%s", jvm_desc_for(vt),
             jvm_desc_for(vt), jvm_desc_for(vt));
    op_emit2(m, op_invokestatic, sm(fc, name, desc));
    ts_push(fc, vt);
    break;
  }
  case 0x20: {
    uint32_t idx = w2_u32(r);
    emit_load_local(fc, idx);
    break;
  }
  case 0x21: {
    uint32_t idx = w2_u32(r);
    emit_store_local(fc, idx);
    break;
  }
  case 0x22: {
    uint32_t idx = w2_u32(r);
    uint8_t vt = fc->local_types[idx];
    op_emit(m, slot_width(vt) == 2 ? op_dup2 : op_dup);
    ts_push(fc, vt);
    emit_store_local(fc, idx);
    break;
  }
  case 0x23: {
    uint32_t idx = w2_u32(r);
    uint16_t fref = wasm_global_field(fc, idx);
    op_emit2(m, op_getstatic, fref);
    ts_push(fc, wasm_global_type(fc->mod, idx));
    break;
  }
  case 0x24: {
    uint32_t idx = w2_u32(r);
    ts_pop(fc);
    uint16_t fref = wasm_global_field(fc, idx);
    op_emit2(m, op_putstatic, fref);
    break;
  }
  case 0x28: {
    w2_u32(r);
    uint32_t off = w2_u32(r);
    emit_mem_load(fc, off, "loadI32", "(I)I", wasm_type_i32);
    break;
  }
  case 0x29: {
    w2_u32(r);
    uint32_t off = w2_u32(r);
    emit_mem_load(fc, off, "loadI64", "(I)J", wasm_type_i64);
    break;
  }
  case 0x2A: {
    w2_u32(r);
    uint32_t off = w2_u32(r);
    emit_mem_load(fc, off, "loadF32", "(I)F", wasm_type_f32);
    break;
  }
  case 0x2B: {
    w2_u32(r);
    uint32_t off = w2_u32(r);
    emit_mem_load(fc, off, "loadF64", "(I)D", wasm_type_f64);
    break;
  }
  case 0x2C: {
    w2_u32(r);
    uint32_t off = w2_u32(r);
    emit_mem_load(fc, off, "loadI32_8s", "(I)I", wasm_type_i32);
    break;
  }
  case 0x2D: {
    w2_u32(r);
    uint32_t off = w2_u32(r);
    emit_mem_load(fc, off, "loadI32_8u", "(I)I", wasm_type_i32);
    break;
  }
  case 0x2E: {
    w2_u32(r);
    uint32_t off = w2_u32(r);
    emit_mem_load(fc, off, "loadI32_16s", "(I)I", wasm_type_i32);
    break;
  }
  case 0x2F: {
    w2_u32(r);
    uint32_t off = w2_u32(r);
    emit_mem_load(fc, off, "loadI32_16u", "(I)I", wasm_type_i32);
    break;
  }
  case 0x30: {
    w2_u32(r);
    uint32_t off = w2_u32(r);
    emit_mem_load(fc, off, "loadI64_8s", "(I)J", wasm_type_i64);
    break;
  }
  case 0x31: {
    w2_u32(r);
    uint32_t off = w2_u32(r);
    emit_mem_load(fc, off, "loadI64_8u", "(I)J", wasm_type_i64);
    break;
  }
  case 0x32: {
    w2_u32(r);
    uint32_t off = w2_u32(r);
    emit_mem_load(fc, off, "loadI64_16s", "(I)J", wasm_type_i64);
    break;
  }
  case 0x33: {
    w2_u32(r);
    uint32_t off = w2_u32(r);
    emit_mem_load(fc, off, "loadI64_16u", "(I)J", wasm_type_i64);
    break;
  }
  case 0x34: {
    w2_u32(r);
    uint32_t off = w2_u32(r);
    emit_mem_load(fc, off, "loadI64_32s", "(I)J", wasm_type_i64);
    break;
  }
  case 0x35: {
    w2_u32(r);
    uint32_t off = w2_u32(r);
    emit_mem_load(fc, off, "loadI64_32u", "(I)J", wasm_type_i64);
    break;
  }
  case 0x36: {
    w2_u32(r);
    uint32_t off = w2_u32(r);
    emit_mem_store(fc, off, "storeI32", "(II)V", wasm_type_i32);
    break;
  }
  case 0x37: {
    w2_u32(r);
    uint32_t off = w2_u32(r);
    emit_mem_store(fc, off, "storeI64", "(IJ)V", wasm_type_i64);
    break;
  }
  case 0x38: {
    w2_u32(r);
    uint32_t off = w2_u32(r);
    emit_mem_store(fc, off, "storeF32", "(IF)V", wasm_type_f32);
    break;
  }
  case 0x39: {
    w2_u32(r);
    uint32_t off = w2_u32(r);
    emit_mem_store(fc, off, "storeF64", "(ID)V", wasm_type_f64);
    break;
  }
  case 0x3A: {
    w2_u32(r);
    uint32_t off = w2_u32(r);
    emit_mem_store(fc, off, "storeI32_8", "(II)V", wasm_type_i32);
    break;
  }
  case 0x3B: {
    w2_u32(r);
    uint32_t off = w2_u32(r);
    emit_mem_store(fc, off, "storeI32_16", "(II)V", wasm_type_i32);
    break;
  }
  case 0x3C: {
    w2_u32(r);
    uint32_t off = w2_u32(r);
    emit_mem_store(fc, off, "storeI64_8", "(IJ)V", wasm_type_i64);
    break;
  }
  case 0x3D: {
    w2_u32(r);
    uint32_t off = w2_u32(r);
    emit_mem_store(fc, off, "storeI64_16", "(IJ)V", wasm_type_i64);
    break;
  }
  case 0x3E: {
    w2_u32(r);
    uint32_t off = w2_u32(r);
    emit_mem_store(fc, off, "storeI64_32", "(IJ)V", wasm_type_i64);
    break;
  }
  case 0x3F:
    w2_u8(r);
    op_emit2(m, op_getstatic, wasm_memory_field(fc));
    op_emit2(m, op_invokevirtual,
             cf_methodref(cf, "V6WasmMemory", "size", "()I"));
    ts_push(fc, wasm_type_i32);
    break;
  case 0x40: {
    w2_u8(r);
    ts_pop(fc);
    op_emit2(m, op_getstatic, wasm_memory_field(fc));
    op_emit(m, op_swap);
    op_emit2(m, op_invokevirtual,
             cf_methodref(cf, "V6WasmMemory", "grow", "(I)I"));
    ts_push(fc, wasm_type_i32);
    break;
  }
  case 0x41:
    emit_iconst(m, w2_i32(r));
    ts_push(fc, wasm_type_i32);
    break;
  case 0x42: {
    int64_t v = w2_i64(r);
    op_emit2(m, op_ldc2_w, cf_long(cf, v));
    ts_push(fc, wasm_type_i64);
    break;
  }
  case 0x43: {
    float v = w2_f32(r);
    op_emit2(m, op_ldc_w, cf_float(cf, v));
    ts_push(fc, wasm_type_f32);
    break;
  }
  case 0x44: {
    double v = w2_f64(r);
    op_emit2(m, op_ldc2_w, cf_double(cf, v));
    ts_push(fc, wasm_type_f64);
    break;
  }

  default:
    codegen_numeric(fc, op);
    break;
  }
}

static compile_result wasm_err(const char* msg) {
  compile_result r;
  r.ok = 0;
  r.line = 0;
  snprintf(r.message, sizeof(r.message), "%s", msg);
  return r;
}

static void compile_one_function(wasm_module* mod, class_file* cf,
                                 const char* class_name, uint32_t combined_idx,
                                 method* m, compile_result* out_err) {
  const wasm_functype* ft = wasm_func_type(mod, combined_idx);
  uint32_t local_idx = combined_idx - mod->imported_func_count;
  wasm_code_body* body = &mod->codes[local_idx];

  wasm_func_ctx fc;
  memset(&fc, 0, sizeof(fc));
  fc.cf = cf;
  fc.m = m;
  fc.mod = mod;
  fc.class_name = class_name;
  fc.param_count = ft->param_count;
  fc.local_count = ft->param_count + body->local_count;
  fc.local_types = malloc(fc.local_count > 0 ? fc.local_count : 1);
  fc.local_slots =
      malloc(sizeof(uint16_t) * (fc.local_count > 0 ? fc.local_count : 1));

  uint16_t slot = 0;
  for (uint32_t i = 0; i < ft->param_count; i++) {
    fc.local_types[i] = ft->params[i];
    fc.local_slots[i] = slot;
    slot += (uint16_t)slot_width(ft->params[i]);
  }
  for (uint32_t i = 0; i < body->local_count; i++) {
    fc.local_types[ft->param_count + i] = body->local_types[i];
    fc.local_slots[ft->param_count + i] = slot;
    slot += (uint16_t)slot_width(body->local_types[i]);
  }
  fc.next_slot = slot;
  fc.has_result = ft->result_count > 0;
  fc.result_type = fc.has_result ? ft->results[0] : 0;

  codegen_body(&fc, body->start, body->len);

  if (!fc.had_error) {
    if (!fc.has_result) {
      op_emit(m, op_return);
    } else {
      switch (fc.result_type) {
      case wasm_type_i32:
        op_emit(m, op_ireturn);
        break;
      case wasm_type_i64:
        op_emit(m, op_lreturn);
        break;
      case wasm_type_f32:
        op_emit(m, op_freturn);
        break;
      case wasm_type_f64:
        op_emit(m, op_dreturn);
        break;
      }
    }
  }

  m->max_stack = 64;
  m->max_locals = fc.next_slot + 3;

  free(fc.local_types);
  free(fc.local_slots);

  if (fc.had_error && out_err->ok) {
    out_err->ok = 0;
    snprintf(out_err->message, sizeof(out_err->message), "%s", fc.err_msg);
  }
}

static void compile_indirect_dispatch(wasm_module* mod, class_file* cf,
                                      const char* class_name,
                                      uint32_t type_index, method* m) {
  const wasm_functype* ft = &mod->types[type_index];

  uint16_t* param_slots = malloc(sizeof(uint16_t) * (ft->param_count + 1));
  uint16_t slot = 0;
  for (uint32_t i = 0; i < ft->param_count; i++) {
    param_slots[i] = slot;
    slot += (uint16_t)slot_width(ft->params[i]);
  }
  uint16_t func_idx_slot = slot;
  slot += 1;

  for (uint32_t fi = 0; fi < mod->func_count; fi++) {
    if (mod->func_type_indices[fi] != type_index)
      continue;
    uint32_t combined_idx = mod->imported_func_count + fi;

    emit_iload_slot(m, func_idx_slot);
    emit_iconst(m, (int)combined_idx);
    size_t skip = op_pos(m);
    op_emit2(m, op_if_icmpne, 0);

    for (uint32_t i = 0; i < ft->param_count; i++) {
      switch (ft->params[i]) {
      case wasm_type_i32:
        emit_iload_slot(m, param_slots[i]);
        break;
      case wasm_type_i64:
        emit_lload(m, param_slots[i]);
        break;
      case wasm_type_f32:
        emit_fload(m, param_slots[i]);
        break;
      case wasm_type_f64:
        emit_dload(m, param_slots[i]);
        break;
      }
    }
    char fname[32];
    snprintf(fname, sizeof(fname), "wasmFunc%u", combined_idx);
    char desc[512];
    build_func_desc(ft, desc, sizeof(desc));
    op_emit2(m, op_invokestatic, cf_methodref(cf, class_name, fname, desc));
    if (ft->result_count == 0) {
      op_emit(m, op_return);
    } else {
      switch (ft->results[0]) {
      case wasm_type_i32:
        op_emit(m, op_ireturn);
        break;
      case wasm_type_i64:
        op_emit(m, op_lreturn);
        break;
      case wasm_type_f32:
        op_emit(m, op_freturn);
        break;
      case wasm_type_f64:
        op_emit(m, op_dreturn);
        break;
      }
    }

    size_t here = op_pos(m);
    op_patch2(m, (uint16_t)(skip + 1), (uint16_t)(here - skip));
  }

  op_emit2(m, op_invokestatic,
           cf_methodref(cf, "V6Wasm", "trapUnreachable", "()V"));
  if (ft->result_count == 0) {
    op_emit(m, op_return);
  } else {
    switch (ft->results[0]) {
    case wasm_type_i32:
      emit_iconst(m, 0);
      op_emit(m, op_ireturn);
      break;
    case wasm_type_i64:
      op_emit2(m, op_ldc2_w, cf_long(cf, 0));
      op_emit(m, op_lreturn);
      break;
    case wasm_type_f32:
      op_emit2(m, op_ldc_w, cf_float(cf, 0));
      op_emit(m, op_freturn);
      break;
    case wasm_type_f64:
      op_emit2(m, op_ldc2_w, cf_double(cf, 0));
      op_emit(m, op_dreturn);
      break;
    }
  }

  m->max_stack = 64;
  m->max_locals = func_idx_slot + 1;
  free(param_slots);
}

static void compile_clinit(wasm_module* mod, class_file* cf,
                           const char* class_name, method* clinit,
                           compile_result* out_err) {
  if (mod->memory_count > 0) {
    wasm_limits* lim = &mod->memories[0];
    uint16_t mem_cls = cf_class(cf, "V6WasmMemory");
    op_emit2(clinit, op_new, mem_cls);
    op_emit(clinit, op_dup);
    emit_iconst(clinit, (int)lim->min);
    emit_iconst(clinit, lim->has_max ? (int)lim->max : -1);
    op_emit2(clinit, op_invokespecial,
             cf_methodref(cf, "V6WasmMemory", "<init>", "(II)V"));
    op_emit2(clinit, op_putstatic,
             cf_fieldref(cf, class_name, "wasmMemory0", "LV6WasmMemory;"));
  }

  if (mod->table_count > 0) {
    wasm_limits* lim = &mod->tables[0];
    uint16_t tbl_cls = cf_class(cf, "V6WasmTable");
    op_emit2(clinit, op_new, tbl_cls);
    op_emit(clinit, op_dup);
    emit_iconst(clinit, (int)lim->min);
    emit_iconst(clinit, lim->has_max ? (int)lim->max : -1);
    op_emit2(clinit, op_invokespecial,
             cf_methodref(cf, "V6WasmTable", "<init>", "(II)V"));
    op_emit2(clinit, op_putstatic,
             cf_fieldref(cf, class_name, "wasmTable0", "LV6WasmTable;"));
  }

  for (uint32_t i = 0; i < mod->element_count; i++) {
    wasm_element_seg* e = &mod->elements[i];
    if (!e->offset_start)
      continue;

    wasm_func_ctx fc;
    memset(&fc, 0, sizeof(fc));
    fc.cf = cf;
    fc.m = clinit;
    fc.mod = mod;
    fc.class_name = class_name;

    codegen_body(&fc, e->offset_start, e->offset_len);

    if (fc.had_error) {
      if (out_err->ok) {
        out_err->ok = 0;
        snprintf(out_err->message, sizeof(out_err->message), "%s", fc.err_msg);
      }
      return;
    }

    uint16_t off_scratch = 0;
    emit_istore(clinit, off_scratch);
    for (uint32_t j = 0; j < e->func_count; j++) {
      op_emit2(clinit, op_getstatic,
               cf_fieldref(cf, class_name, "wasmTable0", "LV6WasmTable;"));
      emit_iload_slot(clinit, off_scratch);
      emit_iconst(clinit, (int)j);
      op_emit(clinit, op_iadd);
      emit_iconst(clinit, (int)e->func_indices[j]);
      op_emit2(clinit, op_invokevirtual,
               cf_methodref(cf, "V6WasmTable", "set", "(II)V"));
    }
  }

  for (uint32_t i = 0; i < mod->global_count; i++) {
    wasm_global_decl* g = &mod->globals[i];

    wasm_func_ctx fc;
    memset(&fc, 0, sizeof(fc));
    fc.cf = cf;
    fc.m = clinit;
    fc.mod = mod;
    fc.class_name = class_name;

    codegen_body(&fc, g->init_start, g->init_len);

    if (fc.had_error) {
      if (out_err->ok) {
        out_err->ok = 0;
        snprintf(out_err->message, sizeof(out_err->message), "%s", fc.err_msg);
      }
      return;
    }

    uint32_t combined_idx = mod->imported_global_count + i;
    char name[32];
    snprintf(name, sizeof(name), "wasmGlobal%u", combined_idx);
    op_emit2(clinit, op_putstatic,
             cf_fieldref(cf, class_name, name, jvm_desc_for(g->val_type)));
  }
  op_emit(clinit, op_return);
  clinit->max_stack = 8;
  clinit->max_locals = 1;
}

compile_result wasm_compile_module(wasm_module* mod, class_file* cf,
                                   const char* class_name) {
  if (mod->import_count > 0)
    return wasm_err("wasm imports not yet supported in this build");
  if (mod->memory_count > 1)
    return wasm_err("wasm multi-memory not supported (max one memory)");
  if (mod->table_count > 1)
    return wasm_err("wasm multi-table not supported (max one table)");
  if (mod->data_count > 0)
    return wasm_err("wasm data segments not yet supported in this build");
  if (mod->func_count != mod->code_count)
    return wasm_err("wasm function/code section count mismatch");

  compile_result result;
  result.ok = 1;
  result.line = 0;
  result.message[0] = '\0';

  if (mod->memory_count > 0)
    cf_field(cf, acc_static, "wasmMemory0", "LV6WasmMemory;");

  if (mod->table_count > 0)
    cf_field(cf, acc_static, "wasmTable0", "LV6WasmTable;");

  for (uint32_t i = 0; i < mod->global_count; i++) {
    uint32_t combined_idx = mod->imported_global_count + i;
    char name[32];
    snprintf(name, sizeof(name), "wasmGlobal%u", combined_idx);
    cf_field(cf, acc_static, name, jvm_desc_for(mod->globals[i].val_type));
  }

  for (uint32_t i = 0; i < mod->func_count; i++) {
    uint32_t combined_idx = mod->imported_func_count + i;
    const wasm_functype* ft = wasm_func_type(mod, combined_idx);
    char desc[512];
    build_func_desc(ft, desc, sizeof(desc));
    char fname[32];
    snprintf(fname, sizeof(fname), "wasmFunc%u", combined_idx);
    method* m = cf_method(cf, acc_public | acc_static, fname, desc);
    compile_one_function(mod, cf, class_name, combined_idx, m, &result);
  }

  if (mod->table_count > 0) {
    for (uint32_t t = 0; t < mod->type_count; t++) {
      const wasm_functype* ft = &mod->types[t];
      char desc[512];
      build_indirect_desc(ft, desc, sizeof(desc));
      char dispatch_name[32];
      snprintf(dispatch_name, sizeof(dispatch_name), "wasmIndirect%u", t);
      method* m = cf_method(cf, acc_static, dispatch_name, desc);
      compile_indirect_dispatch(mod, cf, class_name, t, m);
    }
  }

  if (mod->global_count > 0 || mod->memory_count > 0 || mod->table_count > 0) {
    method* clinit = cf_method(cf, acc_static, "<clinit>", "()V");
    compile_clinit(mod, cf, class_name, clinit, &result);
  }

  return result;
}
