#include "v6/parser.h"

#include "v6/module.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void advance(parser* p) {
  p->prev = p->cur;
  p->cur = lex_next(&p->lex);
}

void parser_init(parser* p, const char* src) {
  lex_init(&p->lex, src);
  p->had_error = 0;
  p->err_msg[0] = '\0';
  p->err_line = 0;
  advance(p);
}

static void error_at(parser* p, const char* msg) {
  if (p->had_error)
    return;
  p->had_error = 1;
  p->err_line = p->cur.line;
  size_t n = strlen(msg);
  if (n >= sizeof(p->err_msg))
    n = sizeof(p->err_msg) - 1;
  memcpy(p->err_msg, msg, n);
  p->err_msg[n] = '\0';
}

static int check(parser* p, tok_kind k) {
  return p->cur.kind == k;
}

static int match(parser* p, tok_kind k) {
  if (!check(p, k))
    return 0;
  advance(p);
  return 1;
}

static const char* tok_name(tok_kind k) {
  switch (k) {
  case tok_semi:
    return ";";
  case tok_colon:
    return ":";
  case tok_lparen:
    return "(";
  case tok_rparen:
    return ")";
  case tok_lbrace:
    return "{";
  case tok_rbrace:
    return "}";
  case tok_lbracket:
    return "[";
  case tok_rbracket:
    return "]";
  case tok_ident:
    return "identifier";
  default:
    return "token";
  }
}

static int expect(parser* p, tok_kind k) {
  if (match(p, k))
    return 1;
  char msg[64];
  snprintf(msg, sizeof(msg), "expected '%s'", tok_name(k));
  error_at(p, msg);
  return 0;
}

static int is_keyword_kind(tok_kind k) {
  switch (k) {
  case tok_kw_var:
  case tok_kw_let:
  case tok_kw_const:
  case tok_kw_function:
  case tok_kw_return:
  case tok_kw_if:
  case tok_kw_else:
  case tok_kw_while:
  case tok_kw_for:
  case tok_kw_true:
  case tok_kw_false:
  case tok_kw_null:
  case tok_kw_undefined:
  case tok_kw_break:
  case tok_kw_continue:
  case tok_kw_switch:
  case tok_kw_case:
  case tok_kw_default:
  case tok_kw_typeof:
  case tok_kw_in:
  case tok_kw_of:
  case tok_kw_this:
  case tok_kw_new:
  case tok_kw_class:
  case tok_kw_extends:
  case tok_kw_super:
  case tok_kw_try:
  case tok_kw_catch:
  case tok_kw_finally:
  case tok_kw_throw:
  case tok_kw_static:
  case tok_kw_instanceof:
  case tok_kw_get:
  case tok_kw_set:
  case tok_kw_async:
  case tok_kw_await:
  case tok_kw_yield:
  case tok_kw_import:
  case tok_kw_export:
  case tok_kw_from:
  case tok_kw_as:
    return 1;
  default:
    return 0;
  }
}

static int match_property_name(parser* p) {
  if (check(p, tok_ident) || is_keyword_kind(p->cur.kind)) {
    advance(p);
    return 1;
  }
  return 0;
}

static uint16_t value_class(class_file* cf) {
  return cf_class(cf, "V6Value");
}

static uint16_t value_ctor(class_file* cf) {
  return cf_methodref(cf, "V6Value", "<init>", "(IDLjava/lang/Object;)V");
}

static void emit_dstore(method* m, uint16_t slot) {
  switch (slot) {
  case 0:
    op_emit(m, op_dstore_0);
    return;
  case 1:
    op_emit(m, op_dstore_1);
    return;
  case 2:
    op_emit(m, op_dstore_2);
    return;
  case 3:
    op_emit(m, op_dstore_3);
    return;
  default:
    op_emit1(m, op_dstore, (uint8_t)slot);
    return;
  }
}

static void emit_dload(method* m, uint16_t slot) {
  switch (slot) {
  case 0:
    op_emit(m, op_dload_0);
    return;
  case 1:
    op_emit(m, op_dload_1);
    return;
  case 2:
    op_emit(m, op_dload_2);
    return;
  case 3:
    op_emit(m, op_dload_3);
    return;
  default:
    op_emit1(m, op_dload, (uint8_t)slot);
    return;
  }
}

static void emit_aload(method* m, uint16_t slot) {
  switch (slot) {
  case 0:
    op_emit(m, op_aload_0);
    return;
  case 1:
    op_emit(m, op_aload_1);
    return;
  case 2:
    op_emit(m, op_aload_2);
    return;
  case 3:
    op_emit(m, op_aload_3);
    return;
  default:
    op_emit1(m, op_aload, (uint8_t)slot);
    return;
  }
}

static void emit_astore(method* m, uint16_t slot) {
  switch (slot) {
  case 0:
    op_emit(m, op_astore_0);
    return;
  case 1:
    op_emit(m, op_astore_1);
    return;
  case 2:
    op_emit(m, op_astore_2);
    return;
  case 3:
    op_emit(m, op_astore_3);
    return;
  default:
    op_emit1(m, op_astore, (uint8_t)slot);
    return;
  }
}

static void emit_box_const(class_file* cf, method* m, uint8_t tag_op,
                           uint8_t num_op) {
  op_emit2(m, op_new, value_class(cf));
  op_emit(m, op_dup);
  op_emit(m, tag_op);
  op_emit(m, num_op);
  op_emit(m, op_aconst_null);
  op_emit2(m, op_invokespecial, value_ctor(cf));
}

static void emit_const_singleton(class_file* cf, method* m, const char* field) {
  uint16_t idx = cf_fieldref(cf, "V6Value", field, "LV6Value;");
  op_emit2(m, op_getstatic, idx);
}

static void emit_undef(class_file* cf, method* m) {
  emit_const_singleton(cf, m, "UNDEF");
}

static void emit_box_tag_m(class_file* cf, method* m, uint16_t scratch_slot,
                           uint8_t tag_op) {
  emit_dstore(m, scratch_slot);
  op_emit2(m, op_new, value_class(cf));
  op_emit(m, op_dup);
  op_emit(m, tag_op);
  emit_dload(m, scratch_slot);
  op_emit(m, op_aconst_null);
  op_emit2(m, op_invokespecial, value_ctor(cf));
}

static void emit_box_tag(compiler* c, uint8_t tag_op) {
  emit_box_tag_m(c->cf, c->m, c->scratch_slot, tag_op);
}

static void emit_box_bool(compiler* c) {
  emit_dstore(c->m, c->scratch_slot);
  emit_dload(c->m, c->scratch_slot);
  op_emit(c->m, op_dconst_0);
  op_emit(c->m, op_dcmpg);
  size_t false_jump = op_pos(c->m);
  op_emit2(c->m, op_ifeq, 0);
  emit_const_singleton(c->cf, c->m, "TRUE");
  size_t end_jump = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);
  size_t false_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(false_jump + 1),
            (uint16_t)(false_pos - false_jump));
  emit_const_singleton(c->cf, c->m, "FALSE");
  size_t end_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(end_jump + 1), (uint16_t)(end_pos - end_jump));
}

static void emit_to_number(compiler* c) {
  uint16_t idx = cf_methodref(c->cf, "V6Value", "toNumber", "()D");
  op_emit2(c->m, op_invokevirtual, idx);
}

static void emit_truthy(compiler* c) {
  uint16_t idx = cf_methodref(c->cf, "V6Value", "truthy", "()Z");
  op_emit2(c->m, op_invokevirtual, idx);
}

static void emit_iconst(method* m, int n) {
  if (n >= -1 && n <= 5) {
    op_emit(m, (uint8_t)(op_iconst_0 + n));
    return;
  }
  if (n >= -128 && n <= 127) {
    op_emit1(m, op_bipush, (uint8_t)n);
    return;
  }
  op_emit2(m, op_sipush, (uint16_t)n);
}

static uint16_t ref_class(class_file* cf) {
  return cf_class(cf, "V6Ref");
}

static uint16_t ref_ctor(class_file* cf) {
  return cf_methodref(cf, "V6Ref", "<init>", "(LV6Value;)V");
}

static uint16_t ref_field(class_file* cf) {
  return cf_fieldref(cf, "V6Ref", "value", "LV6Value;");
}

static void emit_ref_push(compiler* c, int is_upvalue, uint16_t index) {
  if (!is_upvalue) {
    emit_aload(c->m, index);
    return;
  }
  emit_aload(c->m, 0);
  emit_iconst(c->m, (int)index);
  op_emit(c->m, op_aaload);
}

static void emit_var_read(compiler* c, int is_upvalue, uint16_t index) {
  emit_ref_push(c, is_upvalue, index);
  op_emit2(c->m, op_getfield, ref_field(c->cf));
}

static void emit_var_write(compiler* c, int is_upvalue, uint16_t index) {
  emit_ref_push(c, is_upvalue, index);
  op_emit(c->m, op_swap);
  op_emit(c->m, op_dup_x1);
  op_emit2(c->m, op_putfield, ref_field(c->cf));
}

static void emit_var_declare(compiler* c, uint16_t slot) {
  if (!c->box_locals) {
    emit_astore(c->m, slot);
    return;
  }
  op_emit2(c->m, op_new, ref_class(c->cf));
  op_emit(c->m, op_dup_x1);
  op_emit(c->m, op_swap);
  op_emit2(c->m, op_invokespecial, ref_ctor(c->cf));
  emit_astore(c->m, slot);
}

static int hex_val(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

static size_t encode_utf8(uint32_t cp, char* out) {
  if (cp == 0) {
    out[0] = (char)0xc0;
    out[1] = (char)0x80;
    return 2;
  }
  if (cp <= 0x7f) {
    out[0] = (char)cp;
    return 1;
  }
  if (cp <= 0x7ff) {
    out[0] = (char)(0xc0 | (cp >> 6));
    out[1] = (char)(0x80 | (cp & 0x3f));
    return 2;
  }
  out[0] = (char)(0xe0 | (cp >> 12));
  out[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
  out[2] = (char)(0x80 | (cp & 0x3f));
  return 3;
}

static char* decode_string(tok t) {
  char* buf = malloc(t.len > 0 ? t.len : 1);
  size_t j = 0;
  for (size_t i = 1; i + 1 < t.len; i++) {
    char ch = t.start[i];
    if (ch == '\\' && i + 2 < t.len) {
      i++;
      char e = t.start[i];
      if (e == 'n') {
        buf[j++] = '\n';
      } else if (e == 't') {
        buf[j++] = '\t';
      } else if (e == 'r') {
        buf[j++] = '\r';
      } else if (e == '0') {
        j += encode_utf8(0, buf + j);
      } else if (e == 'u' && i + 4 < t.len) {
        int h0 = hex_val(t.start[i + 1]);
        int h1 = hex_val(t.start[i + 2]);
        int h2 = hex_val(t.start[i + 3]);
        int h3 = hex_val(t.start[i + 4]);
        if (h0 >= 0 && h1 >= 0 && h2 >= 0 && h3 >= 0) {
          uint32_t cp = (uint32_t)((h0 << 12) | (h1 << 8) | (h2 << 4) | h3);
          j += encode_utf8(cp, buf + j);
          i += 4;
        } else {
          buf[j++] = e;
        }
      } else if (e == 'x' && i + 2 < t.len) {
        int h0 = hex_val(t.start[i + 1]);
        int h1 = hex_val(t.start[i + 2]);
        if (h0 >= 0 && h1 >= 0) {
          uint32_t cp = (uint32_t)((h0 << 4) | h1);
          j += encode_utf8(cp, buf + j);
          i += 2;
        } else {
          buf[j++] = e;
        }
      } else {
        buf[j++] = e;
      }
    } else {
      buf[j++] = ch;
    }
  }
  buf[j] = '\0';
  return buf;
}

static char* decode_raw_chunk(const char* start, size_t len) {
  char* buf = malloc(len + 1);
  size_t j = 0;
  for (size_t i = 0; i < len; i++) {
    char ch = start[i];
    if (ch == '\\' && i + 1 < len) {
      i++;
      char e = start[i];
      if (e == 'n') {
        buf[j++] = '\n';
      } else if (e == 't') {
        buf[j++] = '\t';
      } else if (e == 'r') {
        buf[j++] = '\r';
      } else if (e == '`') {
        buf[j++] = '`';
      } else if (e == '$') {
        buf[j++] = '$';
      } else if (e == 'u' && i + 4 < len) {
        int h0 = hex_val(start[i + 1]);
        int h1 = hex_val(start[i + 2]);
        int h2 = hex_val(start[i + 3]);
        int h3 = hex_val(start[i + 4]);
        if (h0 >= 0 && h1 >= 0 && h2 >= 0 && h3 >= 0) {
          uint32_t cp = (uint32_t)((h0 << 12) | (h1 << 8) | (h2 << 4) | h3);
          j += encode_utf8(cp, buf + j);
          i += 4;
        } else {
          buf[j++] = e;
        }
      } else {
        buf[j++] = e;
      }
    } else {
      buf[j++] = ch;
    }
  }
  buf[j] = '\0';
  return buf;
}

static char* raw_chunk_copy(const char* start, size_t len) {
  char* buf = malloc(len + 1);
  memcpy(buf, start, len);
  buf[len] = '\0';
  return buf;
}

static char* dup_tok(tok t) {
  char* s = malloc(t.len + 1);
  memcpy(s, t.start, t.len);
  s[t.len] = '\0';
  return s;
}

static char* format_num_key(double n) {
  char buf[64];
  if (n == (double)(long long)n) {
    snprintf(buf, sizeof(buf), "%lld", (long long)n);
  } else {
    snprintf(buf, sizeof(buf), "%g", n);
  }
  char* s = malloc(strlen(buf) + 1);
  strcpy(s, buf);
  return s;
}

static local* find_local_entry(compiler* c, const char* name, size_t len) {
  for (int i = c->local_count - 1; i >= 0; i--) {
    if (c->locals[i].dead)
      continue;
    if (c->locals[i].len == len && memcmp(c->locals[i].name, name, len) == 0)
      return &c->locals[i];
  }
  return NULL;
}

static const char* find_direct_fn(compiler* c, const char* name, size_t len) {
  while (c) {
    for (int i = 0; i < c->param_count; i++) {
      if (c->params[i].len == len && memcmp(c->params[i].name, name, len) == 0)
        return NULL;
    }
    local* le = find_local_entry(c, name, len);
    if (le)
      return le->direct_fn ? le->fn_method_name : NULL;
    c = c->parent;
  }
  return NULL;
}

static int name_reassigned_in_scope(const char* src, const char* name,
                                    size_t name_len) {
  lexer lx;
  lex_init(&lx, src);
  int depth = 0;
  tok t = lex_next(&lx);
  while (t.kind != tok_eof) {
    if (t.kind == tok_lbrace) {
      depth++;
    } else if (t.kind == tok_rbrace) {
      if (depth == 0)
        break;
      depth--;
    } else if (t.kind == tok_ident && t.len == name_len &&
               memcmp(t.start, name, name_len) == 0) {
      tok next = lex_next(&lx);
      if (next.kind == tok_assign || next.kind == tok_plus_eq ||
          next.kind == tok_minus_eq || next.kind == tok_star_eq ||
          next.kind == tok_slash_eq || next.kind == tok_percent_eq ||
          next.kind == tok_amp_eq || next.kind == tok_pipe_eq ||
          next.kind == tok_caret_eq || next.kind == tok_shl_eq ||
          next.kind == tok_shr_eq || next.kind == tok_ushr_eq ||
          next.kind == tok_plus_plus || next.kind == tok_minus_minus) {
        return 1;
      }
      t = next;
      continue;
    } else if (t.kind == tok_plus_plus || t.kind == tok_minus_minus) {
      tok next = lex_next(&lx);
      if (next.kind == tok_ident && next.len == name_len &&
          memcmp(next.start, name, name_len) == 0)
        return 1;
      t = next;
      continue;
    }
    t = lex_next(&lx);
  }
  return 0;
}

static int find_slot(compiler* c, const char* name, size_t len, uint16_t* out) {
  for (int i = 0; i < c->param_count; i++) {
    if (c->params[i].len == len && memcmp(c->params[i].name, name, len) == 0) {
      *out = c->params[i].slot;
      return 1;
    }
  }
  local* le = find_local_entry(c, name, len);
  if (le) {
    *out = le->slot;
    return 1;
  }
  return 0;
}

static void add_local(compiler* c, tok name, uint16_t slot, int is_var,
                      int is_const) {
  c->locals[c->local_count].name = name.start;
  c->locals[c->local_count].len = name.len;
  c->locals[c->local_count].slot = slot;
  c->locals[c->local_count].is_var = is_var;
  c->locals[c->local_count].is_const = is_const;
  c->locals[c->local_count].dead = 0;
  c->locals[c->local_count].direct_fn = 0;
  c->locals[c->local_count].fn_method_name = NULL;
  c->local_count++;
}

typedef enum { var_local, var_upvalue, var_not_found } var_kind;

typedef struct {
  var_kind kind;
  uint16_t index;
} var_ref;

static var_ref resolve_var(compiler* c, const char* name, size_t len) {
  uint16_t slot;
  if (find_slot(c, name, len, &slot)) {
    var_ref vr;
    vr.kind = var_local;
    vr.index = slot;
    return vr;
  }
  if (!c->parent) {
    var_ref vr;
    vr.kind = var_not_found;
    vr.index = 0;
    return vr;
  }
  for (int i = 0; i < c->upvalue_count; i++) {
    if (c->upvalues[i].len == len &&
        memcmp(c->upvalues[i].name, name, len) == 0) {
      var_ref vr;
      vr.kind = var_upvalue;
      vr.index = (uint16_t)i;
      return vr;
    }
  }
  var_ref pref = resolve_var(c->parent, name, len);
  if (pref.kind == var_not_found)
    return pref;
  upvalue* uv = &c->upvalues[c->upvalue_count];
  uv->name = name;
  uv->len = len;
  uv->from_parent_local = pref.kind == var_local;
  uv->parent_index = pref.index;
  var_ref vr;
  vr.kind = var_upvalue;
  vr.index = (uint16_t)c->upvalue_count;
  c->upvalue_count++;
  return vr;
}

static void emit_var_read_ref(compiler* c, var_ref vr) {
  if (!c->box_locals && vr.kind == var_local) {
    emit_aload(c->m, vr.index);
    return;
  }
  emit_var_read(c, vr.kind == var_upvalue, vr.index);
}

static void emit_var_write_ref(compiler* c, var_ref vr) {
  if (!c->box_locals && vr.kind == var_local) {
    op_emit(c->m, op_dup);
    emit_astore(c->m, vr.index);
    return;
  }
  emit_var_write(c, vr.kind == var_upvalue, vr.index);
}

static void parse_expr(parser* p, compiler* c);
static void parse_unary(parser* p, compiler* c);
static void parse_object_literal(parser* p, compiler* c);
static void parse_array_literal(parser* p, compiler* c);
static void parse_primary(parser* p, compiler* c);
static void compile_closure_value(parser* p, compiler* c, int is_arrow,
                                  int parens_params, char* out_lambda_name);
static void skip_balanced(parser* p, tok_kind open, tok_kind close);
static compile_result
compile_module_impl(class_file* cf, const char* this_class_name,
                    const char* user_src, const char* module_dir,
                    module_ctx* modctx, int is_entry, int is_cjs);
static void emit_require_expr(parser* p, compiler* c);
static void parse_object_pattern(parser* p, compiler* c, tok_kind kind,
                                 uint16_t src_slot);
static void emit_tagged_template_call(parser* p, compiler* c);

static int is_logical_assign_op(tok_kind k) {
  return k == tok_amp_amp_eq || k == tok_pipe_pipe_eq ||
         k == tok_question_question_eq;
}

static void emit_logical_assign_ident(parser* p, compiler* c, var_ref vr,
                                      tok_kind op) {
  emit_var_read_ref(c, vr);
  op_emit(c->m, op_dup);
  if (op == tok_question_question_eq) {
    uint16_t idx = cf_methodref(c->cf, "V6Value", "isNullish", "()Z");
    op_emit2(c->m, op_invokevirtual, idx);
  } else {
    emit_truthy(c);
  }
  size_t else_jump = op_pos(c->m);
  uint8_t jump_op = (op == tok_pipe_pipe_eq) ? op_ifne : op_ifeq;
  op_emit2(c->m, jump_op, 0);
  op_emit(c->m, op_pop);
  parse_expr(p, c);
  emit_var_write_ref(c, vr);
  size_t end_jump = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);
  size_t else_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(else_jump + 1), (uint16_t)(else_pos - else_jump));
  size_t end_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(end_jump + 1), (uint16_t)(end_pos - end_jump));
}

static void emit_logical_assign_member(parser* p, compiler* c, tok_kind op) {
  op_emit(c->m, op_dup2);
  uint16_t getprop_idx = cf_methodref(c->cf, "V6Value", "getProp",
                                      "(Ljava/lang/String;)LV6Value;");
  op_emit2(c->m, op_invokevirtual, getprop_idx);
  op_emit(c->m, op_dup);
  if (op == tok_question_question_eq) {
    uint16_t idx = cf_methodref(c->cf, "V6Value", "isNullish", "()Z");
    op_emit2(c->m, op_invokevirtual, idx);
  } else {
    emit_truthy(c);
  }
  size_t else_jump = op_pos(c->m);
  uint8_t jump_op = (op == tok_pipe_pipe_eq) ? op_ifne : op_ifeq;
  op_emit2(c->m, jump_op, 0);
  op_emit(c->m, op_pop);
  parse_expr(p, c);
  op_emit(c->m, op_dup_x2);
  uint16_t setprop_idx = cf_methodref(c->cf, "V6Value", "setProp",
                                      "(Ljava/lang/String;LV6Value;)V");
  op_emit2(c->m, op_invokevirtual, setprop_idx);
  size_t end_jump = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);
  size_t else_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(else_jump + 1), (uint16_t)(else_pos - else_jump));
  op_emit(c->m, op_dup_x2);
  op_emit(c->m, op_pop);
  op_emit(c->m, op_pop);
  op_emit(c->m, op_pop);
  size_t end_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(end_jump + 1), (uint16_t)(end_pos - end_jump));
}

static int peek_is_arrow(parser* p) {
  lexer save_lex = p->lex;
  tok after = lex_next(&p->lex);
  int is_arrow = after.kind == tok_arrow;
  p->lex = save_lex;
  return is_arrow;
}

static int peek_arrow_after_parens(parser* p) {
  lexer save_lex = p->lex;
  tok save_cur = p->cur;
  tok save_prev = p->prev;

  int depth = 0;
  tok t = p->cur;
  for (;;) {
    if (t.kind == tok_lparen) {
      depth++;
    } else if (t.kind == tok_rparen) {
      depth--;
      if (depth == 0) {
        t = lex_next(&p->lex);
        break;
      }
    } else if (t.kind == tok_eof) {
      break;
    }
    t = lex_next(&p->lex);
  }
  int is_arrow = t.kind == tok_arrow;

  p->lex = save_lex;
  p->cur = save_cur;
  p->prev = save_prev;
  return is_arrow;
}

static uint16_t object_class(class_file* cf) {
  return cf_class(cf, "V6Object");
}

static void emit_box_ref_computed(compiler* c, int tag_val) {
  emit_astore(c->m, c->scratch_slot);
  op_emit2(c->m, op_new, value_class(c->cf));
  op_emit(c->m, op_dup);
  emit_iconst(c->m, tag_val);
  op_emit(c->m, op_dconst_0);
  emit_aload(c->m, c->scratch_slot);
  op_emit2(c->m, op_invokespecial, value_ctor(c->cf));
}

static void emit_box_object_ref(compiler* c) {
  emit_box_ref_computed(c, V6_TAG_OBJ);
}

static void emit_wrap_generator(compiler* c) {
  uint16_t ascall_idx =
      cf_methodref(c->cf, "V6Value", "asCallable", "()LV6Callable;");
  op_emit2(c->m, op_invokevirtual, ascall_idx);
  uint16_t tmp_slot = c->next_local_slot++;
  emit_astore(c->m, tmp_slot);
  uint16_t genfn_cls = cf_class(c->cf, "V6GeneratorFunction");
  uint16_t genfn_ctor =
      cf_methodref(c->cf, "V6GeneratorFunction", "<init>", "(LV6Callable;)V");
  op_emit2(c->m, op_new, genfn_cls);
  op_emit(c->m, op_dup);
  emit_aload(c->m, tmp_slot);
  op_emit2(c->m, op_invokespecial, genfn_ctor);
  emit_box_ref_computed(c, V6_TAG_FUNC);
}

static void emit_wrap_async(compiler* c) {
  uint16_t ascall_idx =
      cf_methodref(c->cf, "V6Value", "asCallable", "()LV6Callable;");
  op_emit2(c->m, op_invokevirtual, ascall_idx);
  uint16_t tmp_slot = c->next_local_slot++;
  emit_astore(c->m, tmp_slot);
  uint16_t asyncfn_cls = cf_class(c->cf, "V6AsyncFunction");
  uint16_t asyncfn_ctor =
      cf_methodref(c->cf, "V6AsyncFunction", "<init>", "(LV6Callable;)V");
  op_emit2(c->m, op_new, asyncfn_cls);
  op_emit(c->m, op_dup);
  emit_aload(c->m, tmp_slot);
  op_emit2(c->m, op_invokespecial, asyncfn_ctor);
  emit_box_ref_computed(c, V6_TAG_FUNC);
}

static void emit_wrap_async_generator(compiler* c) {
  uint16_t ascall_idx =
      cf_methodref(c->cf, "V6Value", "asCallable", "()LV6Callable;");
  op_emit2(c->m, op_invokevirtual, ascall_idx);
  uint16_t tmp_slot = c->next_local_slot++;
  emit_astore(c->m, tmp_slot);
  uint16_t asyncgenfn_cls = cf_class(c->cf, "V6AsyncGeneratorFunction");
  uint16_t asyncgenfn_ctor = cf_methodref(c->cf, "V6AsyncGeneratorFunction",
                                          "<init>", "(LV6Callable;)V");
  op_emit2(c->m, op_new, asyncgenfn_cls);
  op_emit(c->m, op_dup);
  emit_aload(c->m, tmp_slot);
  op_emit2(c->m, op_invokespecial, asyncgenfn_ctor);
  emit_box_ref_computed(c, V6_TAG_FUNC);
}

static void parse_object_entry(parser* p, compiler* c) {
  if (match(p, tok_ellipsis)) {
    op_emit(c->m, op_dup);
    parse_expr(p, c);
    uint16_t spread_idx =
        cf_methodref(c->cf, "V6Object", "spreadFrom", "(LV6Value;)V");
    op_emit2(c->m, op_invokevirtual, spread_idx);
    return;
  }

  int is_async = 0;
  if (check(p, tok_kw_async)) {
    lexer save_lex = p->lex;
    tok save_cur = p->cur;
    tok save_prev = p->prev;
    advance(p);
    if (check(p, tok_colon) || check(p, tok_lparen) || check(p, tok_comma) ||
        check(p, tok_rbrace)) {
      p->lex = save_lex;
      p->cur = save_cur;
      p->prev = save_prev;
    } else {
      is_async = 1;
    }
  }

  int is_gen = match(p, tok_star);

  int is_getter = 0, is_setter = 0;
  if (!is_gen && !is_async && (check(p, tok_kw_get) || check(p, tok_kw_set))) {
    tok_kind modifier = p->cur.kind;
    lexer save_lex = p->lex;
    tok save_cur = p->cur;
    tok save_prev = p->prev;
    advance(p);
    if (check(p, tok_colon) || check(p, tok_lparen) || check(p, tok_comma) ||
        check(p, tok_rbrace)) {
      p->lex = save_lex;
      p->cur = save_cur;
      p->prev = save_prev;
    } else {
      is_getter = modifier == tok_kw_get;
      is_setter = modifier == tok_kw_set;
    }
  }

  int computed = 0;
  int shorthand_ok = 0;
  tok ident_tok;
  char* key = NULL;
  if (match(p, tok_lbracket)) {
    computed = 1;
  } else if (check(p, tok_str)) {
    tok t = p->cur;
    advance(p);
    key = decode_string(t);
  } else if (check(p, tok_num)) {
    tok t = p->cur;
    advance(p);
    key = format_num_key(t.num);
  } else if (check(p, tok_ident)) {
    ident_tok = p->cur;
    advance(p);
    key = dup_tok(ident_tok);
    shorthand_ok = 1;
  } else if (match_property_name(p)) {
    key = dup_tok(p->prev);
  } else {
    return;
  }

  uint16_t key_idx = 0;
  uint16_t computed_key_slot = 0;
  if (computed) {
    parse_expr(p, c);
    uint16_t tostring_idx =
        cf_methodref(c->cf, "V6Value", "toString", "()Ljava/lang/String;");
    op_emit2(c->m, op_invokevirtual, tostring_idx);
    if (!expect(p, tok_rbracket))
      return;
    computed_key_slot = c->next_local_slot++;
    emit_astore(c->m, computed_key_slot);
  } else {
    key_idx = cf_string(c->cf, key);
    free(key);
  }

  if (is_getter || is_setter) {
    if (!check(p, tok_lparen)) {
      error_at(p, "expected '('");
      return;
    }
    op_emit(c->m, op_dup);
    if (computed)
      emit_aload(c->m, computed_key_slot);
    else
      op_emit2(c->m, op_ldc_w, key_idx);
    compile_closure_value(p, c, 0, 1, NULL);
    uint16_t ascall_idx =
        cf_methodref(c->cf, "V6Value", "asCallable", "()LV6Callable;");
    op_emit2(c->m, op_invokevirtual, ascall_idx);
    uint16_t def_idx = cf_methodref(c->cf, "V6Object",
                                    is_getter ? "defineGetter" : "defineSetter",
                                    "(Ljava/lang/String;LV6Callable;)V");
    op_emit2(c->m, op_invokevirtual, def_idx);
    return;
  }

  op_emit(c->m, op_dup);
  if (computed)
    emit_aload(c->m, computed_key_slot);
  else
    op_emit2(c->m, op_ldc_w, key_idx);

  if (check(p, tok_lparen)) {
    compile_closure_value(p, c, 0, 1, NULL);
    if (is_gen && is_async) {
      error_at(p, "async generator methods are not supported");
      return;
    } else if (is_gen) {
      emit_wrap_generator(c);
    } else if (is_async) {
      emit_wrap_async(c);
    }
  } else if (match(p, tok_colon)) {
    parse_expr(p, c);
  } else if (shorthand_ok && !computed) {
    var_ref vr = resolve_var(c, ident_tok.start, ident_tok.len);
    if (vr.kind == var_not_found) {
      error_at(p, "undeclared variable");
      return;
    }
    emit_var_read_ref(c, vr);
  } else {
    error_at(p, "expected ':'");
    return;
  }
  uint16_t set_idx =
      cf_methodref(c->cf, "V6Object", "set", "(Ljava/lang/String;LV6Value;)V");
  op_emit2(c->m, op_invokevirtual, set_idx);
}

static void parse_object_literal(parser* p, compiler* c) {
  uint16_t ctor_idx = cf_methodref(c->cf, "V6Object", "<init>", "()V");
  op_emit2(c->m, op_new, object_class(c->cf));
  op_emit(c->m, op_dup);
  op_emit2(c->m, op_invokespecial, ctor_idx);

  if (!check(p, tok_rbrace)) {
    parse_object_entry(p, c);
    while (match(p, tok_comma)) {
      if (check(p, tok_rbrace))
        break;
      parse_object_entry(p, c);
    }
  }
  expect(p, tok_rbrace);

  emit_box_object_ref(c);
}

static void parse_array_literal(parser* p, compiler* c) {
  uint16_t array_cls = cf_class(c->cf, "V6Array");
  uint16_t ctor_idx = cf_methodref(c->cf, "V6Array", "<init>", "()V");
  op_emit2(c->m, op_new, array_cls);
  op_emit(c->m, op_dup);
  op_emit2(c->m, op_invokespecial, ctor_idx);

  if (!check(p, tok_rbracket)) {
    uint16_t push_idx = cf_methodref(c->cf, "V6Object", "push", "(LV6Value;)V");
    uint16_t pushall_idx =
        cf_methodref(c->cf, "V6Object", "pushAll", "(LV6Value;)V");
    for (;;) {
      if (check(p, tok_rbracket))
        break;
      op_emit(c->m, op_dup);
      if (match(p, tok_ellipsis)) {
        parse_expr(p, c);
        op_emit2(c->m, op_invokevirtual, pushall_idx);
      } else {
        parse_expr(p, c);
        op_emit2(c->m, op_invokevirtual, push_idx);
      }
      if (!match(p, tok_comma))
        break;
    }
  }
  expect(p, tok_rbracket);

  emit_box_object_ref(c);
}

static void emit_dup_second_from_top(method* m) {
  op_emit(m, op_swap);
  op_emit(m, op_dup_x1);
  op_emit(m, op_swap);
}

static void emit_insert_undefined_this(compiler* c) {
  emit_undef(c->cf, c->m);
  op_emit(c->m, op_swap);
}

static int scan_call_args(parser* p, int* out_has_spread) {
  *out_has_spread = 0;
  if (check(p, tok_rparen))
    return 0;

  lexer save_lex = p->lex;
  tok save_cur = p->cur;
  tok save_prev = p->prev;

  int count = 1;
  int depth = 0;
  for (;;) {
    if (check(p, tok_eof))
      break;
    if (depth == 0 && check(p, tok_ellipsis))
      *out_has_spread = 1;
    if (check(p, tok_lparen) || check(p, tok_lbracket) ||
        check(p, tok_lbrace)) {
      depth++;
    } else if (check(p, tok_rparen) || check(p, tok_rbracket) ||
               check(p, tok_rbrace)) {
      if (depth == 0)
        break;
      depth--;
    } else if (depth == 0 && check(p, tok_comma)) {
      count++;
    }
    advance(p);
  }

  p->lex = save_lex;
  p->cur = save_cur;
  p->prev = save_prev;
  return count;
}

static void emit_args_array(parser* p, compiler* c) {
  int has_spread = 0;
  int argc = scan_call_args(p, &has_spread);

  if (!has_spread) {
    emit_iconst(c->m, argc);
    op_emit2(c->m, op_anewarray, value_class(c->cf));
    if (!check(p, tok_rparen)) {
      int i = 0;
      for (;;) {
        op_emit(c->m, op_dup);
        emit_iconst(c->m, i);
        parse_expr(p, c);
        op_emit(c->m, op_aastore);
        i++;
        if (!match(p, tok_comma))
          break;
      }
    }
    expect(p, tok_rparen);
    return;
  }

  uint16_t arr_cls = cf_class(c->cf, "V6Array");
  uint16_t arr_ctor = cf_methodref(c->cf, "V6Array", "<init>", "()V");
  uint16_t push_idx = cf_methodref(c->cf, "V6Object", "push", "(LV6Value;)V");
  uint16_t pushall_idx =
      cf_methodref(c->cf, "V6Object", "pushAll", "(LV6Value;)V");
  uint16_t tovalarr_idx =
      cf_methodref(c->cf, "V6Object", "toValueArray", "()[LV6Value;");

  op_emit2(c->m, op_new, arr_cls);
  op_emit(c->m, op_dup);
  op_emit2(c->m, op_invokespecial, arr_ctor);

  for (;;) {
    op_emit(c->m, op_dup);
    if (match(p, tok_ellipsis)) {
      parse_expr(p, c);
      op_emit2(c->m, op_invokevirtual, pushall_idx);
    } else {
      parse_expr(p, c);
      op_emit2(c->m, op_invokevirtual, push_idx);
    }
    if (!match(p, tok_comma))
      break;
  }
  expect(p, tok_rparen);
  op_emit2(c->m, op_invokevirtual, tovalarr_idx);
}

static void emit_call_args_and_invoke(parser* p, compiler* c) {
  op_emit(c->m, op_swap);
  emit_args_array(p, c);
  uint16_t call_idx =
      cf_methodref(c->cf, "V6Value", "call", "(LV6Value;[LV6Value;)LV6Value;");
  op_emit2(c->m, op_invokevirtual, call_idx);
}

static void compile_direct_call(parser* p, compiler* c, var_ref vr,
                                const char* lambda_name) {
  emit_var_read_ref(c, vr);
  advance(p);
  emit_undef(c->cf, c->m);
  emit_args_array(p, c);

  uint16_t this_slot = c->next_local_slot++;
  uint16_t args_slot = c->next_local_slot++;
  emit_astore(c->m, args_slot);
  emit_astore(c->m, this_slot);

  op_emit(c->m, op_dup);
  uint16_t ref_idx =
      cf_methodref(c->cf, "V6Value", "ref", "()Ljava/lang/Object;");
  op_emit2(c->m, op_invokevirtual, ref_idx);
  uint16_t closure_cls = cf_class(c->cf, "V6Closure");
  op_emit2(c->m, op_instanceof, closure_cls);
  size_t slow_jump = op_pos(c->m);
  op_emit2(c->m, op_ifeq, 0);

  op_emit2(c->m, op_invokevirtual, ref_idx);
  op_emit2(c->m, op_checkcast, closure_cls);
  uint16_t captures_idx =
      cf_methodref(c->cf, "V6Closure", "captures", "()[LV6Ref;");
  op_emit2(c->m, op_invokevirtual, captures_idx);
  emit_aload(c->m, this_slot);
  emit_aload(c->m, args_slot);
  uint16_t direct_idx = cf_methodref(c->cf, c->this_class_name, lambda_name,
                                     "([LV6Ref;LV6Value;[LV6Value;)LV6Value;");
  op_emit2(c->m, op_invokestatic, direct_idx);
  size_t end_jump = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);

  size_t slow_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(slow_jump + 1), (uint16_t)(slow_pos - slow_jump));
  emit_aload(c->m, this_slot);
  emit_aload(c->m, args_slot);
  uint16_t call_idx =
      cf_methodref(c->cf, "V6Value", "call", "(LV6Value;[LV6Value;)LV6Value;");
  op_emit2(c->m, op_invokevirtual, call_idx);

  size_t end_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(end_jump + 1), (uint16_t)(end_pos - end_jump));
}

static void compile_direct_new(parser* p, compiler* c, var_ref vr,
                               const char* lambda_name) {
  emit_var_read_ref(c, vr);
  uint16_t cls_val_slot = c->next_local_slot++;
  emit_astore(c->m, cls_val_slot);

  if (match(p, tok_lparen)) {
    emit_args_array(p, c);
  } else {
    emit_iconst(c->m, 0);
    op_emit2(c->m, op_anewarray, value_class(c->cf));
  }
  uint16_t args_slot = c->next_local_slot++;
  emit_astore(c->m, args_slot);

  uint16_t ref_idx =
      cf_methodref(c->cf, "V6Value", "ref", "()Ljava/lang/Object;");
  uint16_t cls_cls = cf_class(c->cf, "V6Class");
  emit_aload(c->m, cls_val_slot);
  op_emit2(c->m, op_invokevirtual, ref_idx);
  op_emit2(c->m, op_instanceof, cls_cls);
  size_t slow_jump = op_pos(c->m);
  op_emit2(c->m, op_ifeq, 0);

  uint16_t cls_obj_slot = c->next_local_slot++;
  emit_aload(c->m, cls_val_slot);
  op_emit2(c->m, op_invokevirtual, ref_idx);
  op_emit2(c->m, op_checkcast, cls_cls);
  emit_astore(c->m, cls_obj_slot);

  uint16_t alloc_idx = cf_methodref(c->cf, "V6Value", "allocateInstance",
                                    "(LV6Class;)LV6Object;");
  emit_aload(c->m, cls_obj_slot);
  op_emit2(c->m, op_invokestatic, alloc_idx);
  uint16_t inst_slot = c->next_local_slot++;
  emit_astore(c->m, inst_slot);

  uint16_t proto_str = cf_string(c->cf, "prototype");
  uint16_t get_idx =
      cf_methodref(c->cf, "V6Object", "get", "(Ljava/lang/String;)LV6Value;");
  uint16_t setprotoval_idx =
      cf_methodref(c->cf, "V6Object", "setProtoFromValue", "(LV6Value;)V");
  emit_aload(c->m, inst_slot);
  emit_aload(c->m, cls_obj_slot);
  op_emit2(c->m, op_ldc_w, proto_str);
  op_emit2(c->m, op_invokevirtual, get_idx);
  op_emit2(c->m, op_invokevirtual, setprotoval_idx);

  uint16_t new_target_field =
      cf_fieldref(c->cf, "V6Object", "newTarget", "LV6Value;");
  emit_aload(c->m, inst_slot);
  emit_aload(c->m, cls_val_slot);
  op_emit2(c->m, op_putfield, new_target_field);

  emit_aload(c->m, inst_slot);
  emit_box_object_ref(c);
  uint16_t instval_slot = c->next_local_slot++;
  emit_astore(c->m, instval_slot);

  uint16_t ctor_field = cf_fieldref(c->cf, "V6Class", "ctor", "LV6Callable;");
  uint16_t closure_cls = cf_class(c->cf, "V6Closure");
  uint16_t captures_idx =
      cf_methodref(c->cf, "V6Closure", "captures", "()[LV6Ref;");
  emit_aload(c->m, cls_obj_slot);
  op_emit2(c->m, op_getfield, ctor_field);
  op_emit2(c->m, op_checkcast, closure_cls);
  op_emit2(c->m, op_invokevirtual, captures_idx);
  emit_aload(c->m, instval_slot);
  emit_aload(c->m, args_slot);
  uint16_t direct_idx = cf_methodref(c->cf, c->this_class_name, lambda_name,
                                     "([LV6Ref;LV6Value;[LV6Value;)LV6Value;");
  op_emit2(c->m, op_invokestatic, direct_idx);
  op_emit(c->m, op_pop);
  emit_aload(c->m, instval_slot);

  size_t end_jump = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);

  size_t slow_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(slow_jump + 1), (uint16_t)(slow_pos - slow_jump));
  emit_aload(c->m, cls_val_slot);
  emit_aload(c->m, args_slot);
  uint16_t construct_idx = cf_methodref(c->cf, "V6Value", "construct",
                                        "(LV6Value;[LV6Value;)LV6Value;");
  op_emit2(c->m, op_invokestatic, construct_idx);

  size_t end_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(end_jump + 1), (uint16_t)(end_pos - end_jump));
}

static void parse_postfix(parser* p, compiler* c) {
  parse_primary(p, c);
  size_t opt_jumps[16];
  int opt_count = 0;

  for (;;) {
    if (check(p, tok_question_dot)) {
      advance(p);

      if (check(p, tok_lparen)) {
        uint16_t nullish_idx =
            cf_methodref(c->cf, "V6Value", "isNullish", "()Z");
        op_emit(c->m, op_dup);
        op_emit2(c->m, op_invokevirtual, nullish_idx);
        size_t skip_pos = op_pos(c->m);
        op_emit2(c->m, op_ifeq, 0);
        op_emit(c->m, op_pop);
        emit_undef(c->cf, c->m);
        if (opt_count < 16)
          opt_jumps[opt_count++] = op_pos(c->m);
        op_emit2(c->m, op_goto, 0);
        size_t cont_pos = op_pos(c->m);
        op_patch2(c->m, (uint16_t)(skip_pos + 1),
                  (uint16_t)(cont_pos - skip_pos));

        advance(p);
        emit_insert_undefined_this(c);
        emit_call_args_and_invoke(p, c);
        continue;
      }

      int is_bracket = check(p, tok_lbracket);
      if (is_bracket) {
        advance(p);
      } else if (!match_property_name(p)) {
        error_at(p, "expected property name");
        return;
      }

      uint16_t nullish_idx = cf_methodref(c->cf, "V6Value", "isNullish", "()Z");
      op_emit(c->m, op_dup);
      op_emit2(c->m, op_invokevirtual, nullish_idx);
      size_t skip_pos = op_pos(c->m);
      op_emit2(c->m, op_ifeq, 0);
      op_emit(c->m, op_pop);
      emit_undef(c->cf, c->m);
      if (opt_count < 16)
        opt_jumps[opt_count++] = op_pos(c->m);
      op_emit2(c->m, op_goto, 0);
      size_t cont_pos = op_pos(c->m);
      op_patch2(c->m, (uint16_t)(skip_pos + 1),
                (uint16_t)(cont_pos - skip_pos));

      if (is_bracket) {
        parse_expr(p, c);
        uint16_t tostring_idx =
            cf_methodref(c->cf, "V6Value", "toString", "()Ljava/lang/String;");
        op_emit2(c->m, op_invokevirtual, tostring_idx);
        expect(p, tok_rbracket);
      } else {
        char* key = dup_tok(p->prev);
        uint16_t key_idx = cf_string(c->cf, key);
        free(key);
        op_emit2(c->m, op_ldc_w, key_idx);
      }

      if (check(p, tok_lparen)) {
        emit_dup_second_from_top(c->m);
        uint16_t get_idx = cf_methodref(c->cf, "V6Value", "getProp",
                                        "(Ljava/lang/String;)LV6Value;");
        op_emit2(c->m, op_invokevirtual, get_idx);
        advance(p);
        emit_call_args_and_invoke(p, c);
        continue;
      }

      uint16_t get_idx = cf_methodref(c->cf, "V6Value", "getProp",
                                      "(Ljava/lang/String;)LV6Value;");
      op_emit2(c->m, op_invokevirtual, get_idx);
      continue;
    }

    if (check(p, tok_dot) || check(p, tok_lbracket)) {
      if (match(p, tok_dot)) {
        if (!match_property_name(p)) {
          error_at(p, "expected property name");
          return;
        }
        char* key = dup_tok(p->prev);
        uint16_t key_idx = cf_string(c->cf, key);
        free(key);
        op_emit2(c->m, op_ldc_w, key_idx);
      } else {
        advance(p);
        parse_expr(p, c);
        uint16_t tostring_idx =
            cf_methodref(c->cf, "V6Value", "toString", "()Ljava/lang/String;");
        op_emit2(c->m, op_invokevirtual, tostring_idx);
        expect(p, tok_rbracket);
      }

      if (match(p, tok_assign)) {
        parse_expr(p, c);
        op_emit(c->m, op_dup_x2);
        uint16_t set_idx = cf_methodref(c->cf, "V6Value", "setProp",
                                        "(Ljava/lang/String;LV6Value;)V");
        op_emit2(c->m, op_invokevirtual, set_idx);
        goto patch_and_return;
      }

      if (is_logical_assign_op(p->cur.kind)) {
        tok_kind op = p->cur.kind;
        advance(p);
        emit_logical_assign_member(p, c, op);
        goto patch_and_return;
      }

      if (check(p, tok_plus_eq) || check(p, tok_minus_eq) ||
          check(p, tok_star_eq) || check(p, tok_slash_eq) ||
          check(p, tok_percent_eq) || check(p, tok_amp_eq) ||
          check(p, tok_pipe_eq) || check(p, tok_caret_eq) ||
          check(p, tok_shl_eq) || check(p, tok_shr_eq) ||
          check(p, tok_ushr_eq)) {
        tok_kind op = p->cur.kind;
        advance(p);
        uint16_t getprop_idx2 = cf_methodref(c->cf, "V6Value", "getProp",
                                             "(Ljava/lang/String;)LV6Value;");
        uint16_t set_idx = cf_methodref(c->cf, "V6Value", "setProp",
                                        "(Ljava/lang/String;LV6Value;)V");
        op_emit(c->m, op_dup2);
        op_emit2(c->m, op_invokevirtual, getprop_idx2);

        if (op == tok_plus_eq) {
          parse_expr(p, c);
          uint16_t idx = cf_methodref(c->cf, "V6Value", "add",
                                      "(LV6Value;LV6Value;)LV6Value;");
          op_emit2(c->m, op_invokestatic, idx);
        } else if (op == tok_amp_eq || op == tok_pipe_eq ||
                   op == tok_caret_eq || op == tok_shl_eq || op == tok_shr_eq ||
                   op == tok_ushr_eq) {
          parse_expr(p, c);
          const char* mname = op == tok_amp_eq     ? "bitAnd"
                              : op == tok_pipe_eq  ? "bitOr"
                              : op == tok_caret_eq ? "bitXor"
                              : op == tok_shl_eq   ? "shl"
                              : op == tok_shr_eq   ? "shr"
                                                   : "ushr";
          uint16_t idx = cf_methodref(c->cf, "V6Value", mname,
                                      "(LV6Value;LV6Value;)LV6Value;");
          op_emit2(c->m, op_invokestatic, idx);
        } else {
          emit_to_number(c);
          parse_expr(p, c);
          emit_to_number(c);
          uint8_t bop = op == tok_minus_eq
                            ? op_dsub
                            : (op == tok_star_eq
                                   ? op_dmul
                                   : (op == tok_slash_eq ? op_ddiv : op_drem));
          op_emit(c->m, bop);
          emit_box_tag(c, op_iconst_0);
        }

        op_emit(c->m, op_dup_x2);
        op_emit2(c->m, op_invokevirtual, set_idx);
        goto patch_and_return;
      }

      if (check(p, tok_plus_plus) || check(p, tok_minus_minus)) {
        int is_inc = check(p, tok_plus_plus);
        advance(p);
        uint16_t getprop_idx2 = cf_methodref(c->cf, "V6Value", "getProp",
                                             "(Ljava/lang/String;)LV6Value;");
        uint16_t set_idx = cf_methodref(c->cf, "V6Value", "setProp",
                                        "(Ljava/lang/String;LV6Value;)V");
        op_emit(c->m, op_dup2);
        op_emit2(c->m, op_invokevirtual, getprop_idx2);
        op_emit(c->m, op_dup_x2);
        emit_to_number(c);
        op_emit(c->m, op_dconst_1);
        op_emit(c->m, is_inc ? op_dadd : op_dsub);
        emit_box_tag(c, op_iconst_0);
        op_emit2(c->m, op_invokevirtual, set_idx);
        goto patch_and_return;
      }

      if (check(p, tok_lparen)) {
        emit_dup_second_from_top(c->m);
        uint16_t get_idx = cf_methodref(c->cf, "V6Value", "getProp",
                                        "(Ljava/lang/String;)LV6Value;");
        op_emit2(c->m, op_invokevirtual, get_idx);
        advance(p);
        emit_call_args_and_invoke(p, c);
        continue;
      }

      if (check(p, tok_template)) {
        emit_dup_second_from_top(c->m);
        uint16_t get_idx = cf_methodref(c->cf, "V6Value", "getProp",
                                        "(Ljava/lang/String;)LV6Value;");
        op_emit2(c->m, op_invokevirtual, get_idx);
        emit_tagged_template_call(p, c);
        continue;
      }

      uint16_t get_idx = cf_methodref(c->cf, "V6Value", "getProp",
                                      "(Ljava/lang/String;)LV6Value;");
      op_emit2(c->m, op_invokevirtual, get_idx);
    } else if (check(p, tok_lparen)) {
      advance(p);
      emit_insert_undefined_this(c);
      emit_call_args_and_invoke(p, c);
    } else if (check(p, tok_template)) {
      emit_insert_undefined_this(c);
      emit_tagged_template_call(p, c);
    } else {
      break;
    }
  }

patch_and_return:
  for (int i = 0; i < opt_count; i++) {
    size_t end_pos = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(opt_jumps[i] + 1),
              (uint16_t)(end_pos - opt_jumps[i]));
  }
}

static void emit_string_value(compiler* c, const char* s) {
  uint16_t str_idx = cf_string(c->cf, s);
  op_emit2(c->m, op_new, value_class(c->cf));
  op_emit(c->m, op_dup);
  op_emit(c->m, op_iconst_5);
  op_emit(c->m, op_dconst_0);
  op_emit2(c->m, op_ldc_w, str_idx);
  op_emit2(c->m, op_invokespecial, value_ctor(c->cf));
}

static void parse_template_literal(parser* p, compiler* c) {
  tok t = p->cur;
  advance(p);
  const char* s = t.start;
  size_t len = t.len;

  emit_string_value(c, "");
  uint16_t add_idx =
      cf_methodref(c->cf, "V6Value", "add", "(LV6Value;LV6Value;)LV6Value;");

  size_t i = 1;
  size_t chunk_start = 1;
  while (i < len - 1) {
    if (s[i] == '\\' && i + 1 < len - 1) {
      i += 2;
      continue;
    }
    if (s[i] == '$' && i + 1 < len - 1 && s[i + 1] == '{') {
      if (i > chunk_start) {
        char* dec = decode_raw_chunk(s + chunk_start, i - chunk_start);
        emit_string_value(c, dec);
        free(dec);
        op_emit2(c->m, op_invokestatic, add_idx);
      }

      size_t expr_start = i + 2;
      int depth = 1;
      size_t j = expr_start;
      while (j < len - 1 && depth > 0) {
        if (s[j] == '{') {
          depth++;
        } else if (s[j] == '}') {
          depth--;
          if (depth == 0)
            break;
        } else if (s[j] == '"' || s[j] == '\'') {
          char q = s[j];
          j++;
          while (j < len - 1 && s[j] != q) {
            if (s[j] == '\\' && j + 1 < len - 1)
              j++;
            j++;
          }
        }
        j++;
      }

      parser ep;
      parser_init(&ep, s + expr_start);
      parse_expr(&ep, c);
      op_emit2(c->m, op_invokestatic, add_idx);

      i = j + 1;
      chunk_start = i;
      continue;
    }
    i++;
  }

  if (i > chunk_start) {
    char* dec = decode_raw_chunk(s + chunk_start, i - chunk_start);
    emit_string_value(c, dec);
    free(dec);
    op_emit2(c->m, op_invokestatic, add_idx);
  }
}

static void emit_tagged_template_call(parser* p, compiler* c) {
  uint16_t fn_slot = c->next_local_slot++;
  uint16_t this_slot = c->next_local_slot++;
  emit_astore(c->m, fn_slot);
  emit_astore(c->m, this_slot);

  tok t = p->cur;
  advance(p);
  const char* s = t.start;
  size_t len = t.len;

  uint16_t arr_cls = cf_class(c->cf, "V6Array");
  uint16_t arr_ctor = cf_methodref(c->cf, "V6Array", "<init>", "()V");
  uint16_t push_idx = cf_methodref(c->cf, "V6Object", "push", "(LV6Value;)V");
  uint16_t set_idx =
      cf_methodref(c->cf, "V6Object", "set", "(Ljava/lang/String;LV6Value;)V");
  uint16_t tovalarr_idx =
      cf_methodref(c->cf, "V6Object", "toValueArray", "()[LV6Value;");

  op_emit2(c->m, op_new, arr_cls);
  op_emit(c->m, op_dup);
  op_emit2(c->m, op_invokespecial, arr_ctor);
  uint16_t strings_slot = c->next_local_slot++;
  emit_astore(c->m, strings_slot);

  op_emit2(c->m, op_new, arr_cls);
  op_emit(c->m, op_dup);
  op_emit2(c->m, op_invokespecial, arr_ctor);
  uint16_t raw_slot = c->next_local_slot++;
  emit_astore(c->m, raw_slot);

  op_emit2(c->m, op_new, arr_cls);
  op_emit(c->m, op_dup);
  op_emit2(c->m, op_invokespecial, arr_ctor);
  uint16_t callargs_slot = c->next_local_slot++;
  emit_astore(c->m, callargs_slot);

  emit_aload(c->m, callargs_slot);
  emit_aload(c->m, strings_slot);
  emit_box_object_ref(c);
  op_emit2(c->m, op_invokevirtual, push_idx);

  size_t i = 1;
  size_t chunk_start = 1;
  while (i < len - 1) {
    if (s[i] == '\\' && i + 1 < len - 1) {
      i += 2;
      continue;
    }
    if (s[i] == '$' && i + 1 < len - 1 && s[i + 1] == '{') {
      char* dec = decode_raw_chunk(s + chunk_start, i - chunk_start);
      emit_aload(c->m, strings_slot);
      emit_string_value(c, dec);
      free(dec);
      op_emit2(c->m, op_invokevirtual, push_idx);

      char* raw = raw_chunk_copy(s + chunk_start, i - chunk_start);
      emit_aload(c->m, raw_slot);
      emit_string_value(c, raw);
      free(raw);
      op_emit2(c->m, op_invokevirtual, push_idx);

      size_t expr_start = i + 2;
      int depth = 1;
      size_t j = expr_start;
      while (j < len - 1 && depth > 0) {
        if (s[j] == '{') {
          depth++;
        } else if (s[j] == '}') {
          depth--;
          if (depth == 0)
            break;
        } else if (s[j] == '"' || s[j] == '\'') {
          char q = s[j];
          j++;
          while (j < len - 1 && s[j] != q) {
            if (s[j] == '\\' && j + 1 < len - 1)
              j++;
            j++;
          }
        }
        j++;
      }

      parser ep;
      parser_init(&ep, s + expr_start);
      emit_aload(c->m, callargs_slot);
      parse_expr(&ep, c);
      op_emit2(c->m, op_invokevirtual, push_idx);

      i = j + 1;
      chunk_start = i;
      continue;
    }
    i++;
  }

  char* dec = decode_raw_chunk(s + chunk_start, i - chunk_start);
  emit_aload(c->m, strings_slot);
  emit_string_value(c, dec);
  free(dec);
  op_emit2(c->m, op_invokevirtual, push_idx);

  char* raw = raw_chunk_copy(s + chunk_start, i - chunk_start);
  emit_aload(c->m, raw_slot);
  emit_string_value(c, raw);
  free(raw);
  op_emit2(c->m, op_invokevirtual, push_idx);

  uint16_t raw_str = cf_string(c->cf, "raw");
  emit_aload(c->m, strings_slot);
  op_emit2(c->m, op_ldc_w, raw_str);
  emit_aload(c->m, raw_slot);
  emit_box_object_ref(c);
  op_emit2(c->m, op_invokevirtual, set_idx);

  emit_aload(c->m, fn_slot);
  emit_aload(c->m, this_slot);
  emit_aload(c->m, callargs_slot);
  op_emit2(c->m, op_invokevirtual, tovalarr_idx);
  uint16_t call_idx =
      cf_methodref(c->cf, "V6Value", "call", "(LV6Value;[LV6Value;)LV6Value;");
  op_emit2(c->m, op_invokevirtual, call_idx);
}

static void parse_new(parser* p, compiler* c) {
  if (!expect(p, tok_ident))
    return;
  tok name = p->prev;
  var_ref vr = resolve_var(c, name.start, name.len);
  if (vr.kind == var_not_found) {
    error_at(p, "undeclared variable");
    return;
  }

  if (!check(p, tok_dot) && !check(p, tok_lbracket)) {
    const char* lambda_name = find_direct_fn(c, name.start, name.len);
    if (lambda_name) {
      compile_direct_new(p, c, vr, lambda_name);
      return;
    }
  }

  emit_var_read_ref(c, vr);

  while (check(p, tok_dot) || check(p, tok_lbracket)) {
    if (match(p, tok_dot)) {
      if (!match_property_name(p)) {
        error_at(p, "expected property name");
        return;
      }
      char* key = dup_tok(p->prev);
      uint16_t key_idx = cf_string(c->cf, key);
      free(key);
      op_emit2(c->m, op_ldc_w, key_idx);
    } else {
      advance(p);
      parse_expr(p, c);
      uint16_t tostring_idx =
          cf_methodref(c->cf, "V6Value", "toString", "()Ljava/lang/String;");
      op_emit2(c->m, op_invokevirtual, tostring_idx);
      if (!expect(p, tok_rbracket))
        return;
    }
    uint16_t get_idx = cf_methodref(c->cf, "V6Value", "getProp",
                                    "(Ljava/lang/String;)LV6Value;");
    op_emit2(c->m, op_invokevirtual, get_idx);
  }

  if (match(p, tok_lparen)) {
    emit_args_array(p, c);
  } else {
    emit_iconst(c->m, 0);
    op_emit2(c->m, op_anewarray, value_class(c->cf));
  }

  uint16_t construct_idx = cf_methodref(c->cf, "V6Value", "construct",
                                        "(LV6Value;[LV6Value;)LV6Value;");
  op_emit2(c->m, op_invokestatic, construct_idx);
}

static void parse_super(parser* p, compiler* c) {
  if (!c->super_name) {
    error_at(p, "'super' outside class");
    return;
  }
  var_ref base_vr = resolve_var(c, c->super_name, c->super_len);
  var_ref this_vr = resolve_var(c, "this", 4);
  if (base_vr.kind == var_not_found || this_vr.kind == var_not_found) {
    error_at(p, "'super' outside class");
    return;
  }

  if (match(p, tok_lparen)) {
    emit_var_read_ref(c, base_vr);
    emit_var_read_ref(c, this_vr);
    emit_args_array(p, c);
    uint16_t sc_idx = cf_methodref(c->cf, "V6Value", "superConstruct",
                                   "(LV6Value;LV6Value;[LV6Value;)V");
    op_emit2(c->m, op_invokestatic, sc_idx);
    emit_undef(c->cf, c->m);
    return;
  }

  if (!expect(p, tok_dot))
    return;
  if (!match_property_name(p))
    return;
  char* key = dup_tok(p->prev);
  uint16_t key_idx = cf_string(c->cf, key);
  free(key);

  uint16_t proto_str = cf_string(c->cf, "prototype");
  uint16_t getprop_idx = cf_methodref(c->cf, "V6Value", "getProp",
                                      "(Ljava/lang/String;)LV6Value;");
  emit_var_read_ref(c, base_vr);
  op_emit2(c->m, op_ldc_w, proto_str);
  op_emit2(c->m, op_invokevirtual, getprop_idx);
  op_emit2(c->m, op_ldc_w, key_idx);
  op_emit2(c->m, op_invokevirtual, getprop_idx);
  emit_var_read_ref(c, this_vr);
  op_emit(c->m, op_swap);

  if (!expect(p, tok_lparen))
    return;
  emit_call_args_and_invoke(p, c);
}

static void decode_regex(tok t, char** out_source, char** out_flags) {
  const char* s = t.start;
  size_t i = 1;
  int in_class = 0;
  while (i < t.len && (in_class || s[i] != '/')) {
    if (s[i] == '\\' && i + 1 < t.len) {
      i += 2;
      continue;
    }
    if (s[i] == '[')
      in_class = 1;
    else if (s[i] == ']')
      in_class = 0;
    i++;
  }
  size_t source_len = i - 1;
  char* source = malloc(source_len + 1);
  memcpy(source, s + 1, source_len);
  source[source_len] = '\0';
  size_t flags_start = i + 1;
  size_t flags_len = (flags_start <= t.len) ? t.len - flags_start : 0;
  char* flags = malloc(flags_len + 1);
  if (flags_len)
    memcpy(flags, s + flags_start, flags_len);
  flags[flags_len] = '\0';
  *out_source = source;
  *out_flags = flags;
}

static void emit_regex_literal(parser* p, compiler* c, tok t) {
  char* source;
  char* flags;
  decode_regex(t, &source, &flags);

  var_ref vr = resolve_var(c, "RegExp", 6);
  if (vr.kind == var_not_found) {
    error_at(p, "RegExp is not defined");
    free(source);
    free(flags);
    return;
  }
  emit_var_read_ref(c, vr);
  uint16_t cls_val_slot = c->next_local_slot++;
  emit_astore(c->m, cls_val_slot);

  emit_iconst(c->m, 2);
  op_emit2(c->m, op_anewarray, value_class(c->cf));
  op_emit(c->m, op_dup);
  emit_iconst(c->m, 0);
  emit_string_value(c, source);
  op_emit(c->m, op_aastore);
  op_emit(c->m, op_dup);
  emit_iconst(c->m, 1);
  emit_string_value(c, flags);
  op_emit(c->m, op_aastore);
  uint16_t args_slot = c->next_local_slot++;
  emit_astore(c->m, args_slot);

  emit_aload(c->m, cls_val_slot);
  emit_aload(c->m, args_slot);
  uint16_t construct_idx = cf_methodref(c->cf, "V6Value", "construct",
                                        "(LV6Value;[LV6Value;)LV6Value;");
  op_emit2(c->m, op_invokestatic, construct_idx);

  free(source);
  free(flags);
}

static void parse_primary(parser* p, compiler* c) {
  if (match(p, tok_kw_new)) {
    if (match(p, tok_dot)) {
      if (!check(p, tok_ident) || p->cur.len != 6 ||
          memcmp(p->cur.start, "target", 6) != 0) {
        error_at(p, "expected 'target'");
        return;
      }
      advance(p);
      if (c->class_name) {
        var_ref this_vr = resolve_var(c, "this", 4);
        if (this_vr.kind != var_not_found) {
          emit_var_read_ref(c, this_vr);
          uint16_t ref_idx =
              cf_methodref(c->cf, "V6Value", "ref", "()Ljava/lang/Object;");
          op_emit2(c->m, op_invokevirtual, ref_idx);
          uint16_t obj_cls = cf_class(c->cf, "V6Object");
          op_emit2(c->m, op_checkcast, obj_cls);
          uint16_t new_target_field =
              cf_fieldref(c->cf, "V6Object", "newTarget", "LV6Value;");
          op_emit2(c->m, op_getfield, new_target_field);
          op_emit(c->m, op_dup);
          size_t has_val_jump = op_pos(c->m);
          op_emit2(c->m, op_ifnonnull, 0);
          op_emit(c->m, op_pop);
          emit_undef(c->cf, c->m);
          size_t has_val_pos = op_pos(c->m);
          op_patch2(c->m, (uint16_t)(has_val_jump + 1),
                    (uint16_t)(has_val_pos - has_val_jump));
          return;
        }
      }
      emit_undef(c->cf, c->m);
      return;
    }
    parse_new(p, c);
    return;
  }

  if (match(p, tok_kw_super)) {
    parse_super(p, c);
    return;
  }

  if (match(p, tok_num)) {
    if (p->prev.is_bigint) {
      size_t digits_len = p->prev.len - 1;
      char* digits = malloc(digits_len + 1);
      memcpy(digits, p->prev.start, digits_len);
      digits[digits_len] = '\0';
      uint16_t str_idx = cf_string(c->cf, digits);
      free(digits);
      uint16_t bigint_cls = cf_class(c->cf, "java/math/BigInteger");
      uint16_t bigint_ctor = cf_methodref(c->cf, "java/math/BigInteger",
                                          "<init>", "(Ljava/lang/String;)V");
      op_emit2(c->m, op_new, value_class(c->cf));
      op_emit(c->m, op_dup);
      emit_iconst(c->m, V6_TAG_BIGINT);
      op_emit(c->m, op_dconst_0);
      op_emit2(c->m, op_new, bigint_cls);
      op_emit(c->m, op_dup);
      op_emit2(c->m, op_ldc_w, str_idx);
      op_emit2(c->m, op_invokespecial, bigint_ctor);
      op_emit2(c->m, op_invokespecial, value_ctor(c->cf));
      return;
    }
    uint16_t idx = cf_double(c->cf, p->prev.num);
    op_emit2(c->m, op_new, value_class(c->cf));
    op_emit(c->m, op_dup);
    op_emit(c->m, op_iconst_0);
    op_emit2(c->m, op_ldc2_w, idx);
    op_emit(c->m, op_aconst_null);
    op_emit2(c->m, op_invokespecial, value_ctor(c->cf));
    return;
  }

  if (match(p, tok_kw_true)) {
    emit_const_singleton(c->cf, c->m, "TRUE");
    return;
  }

  if (match(p, tok_kw_false)) {
    emit_const_singleton(c->cf, c->m, "FALSE");
    return;
  }

  if (match(p, tok_kw_null)) {
    emit_const_singleton(c->cf, c->m, "NUL");
    return;
  }

  if (match(p, tok_kw_undefined)) {
    emit_undef(c->cf, c->m);
    return;
  }

  if (check(p, tok_str)) {
    tok t = p->cur;
    advance(p);
    char* s = decode_string(t);
    emit_string_value(c, s);
    free(s);
    return;
  }

  if (check(p, tok_template)) {
    parse_template_literal(p, c);
    return;
  }

  if (check(p, tok_plus_plus) || check(p, tok_minus_minus)) {
    int is_inc = check(p, tok_plus_plus);
    advance(p);
    if (!expect(p, tok_ident))
      return;
    tok name = p->prev;

    if (check(p, tok_dot) || check(p, tok_lbracket)) {
      var_ref vr = resolve_var(c, name.start, name.len);
      if (vr.kind == var_not_found) {
        error_at(p, "undeclared variable");
        return;
      }
      emit_var_read_ref(c, vr);
      if (match(p, tok_dot)) {
        if (!match_property_name(p)) {
          error_at(p, "expected property name");
          return;
        }
        char* key = dup_tok(p->prev);
        uint16_t key_idx = cf_string(c->cf, key);
        free(key);
        op_emit2(c->m, op_ldc_w, key_idx);
      } else {
        advance(p);
        parse_expr(p, c);
        uint16_t tostring_idx =
            cf_methodref(c->cf, "V6Value", "toString", "()Ljava/lang/String;");
        op_emit2(c->m, op_invokevirtual, tostring_idx);
        if (!expect(p, tok_rbracket))
          return;
      }
      uint16_t getprop_idx2 = cf_methodref(c->cf, "V6Value", "getProp",
                                           "(Ljava/lang/String;)LV6Value;");
      uint16_t set_idx = cf_methodref(c->cf, "V6Value", "setProp",
                                      "(Ljava/lang/String;LV6Value;)V");
      op_emit(c->m, op_dup2);
      op_emit2(c->m, op_invokevirtual, getprop_idx2);
      emit_to_number(c);
      op_emit(c->m, op_dconst_1);
      op_emit(c->m, is_inc ? op_dadd : op_dsub);
      emit_box_tag(c, op_iconst_0);
      op_emit(c->m, op_dup_x2);
      op_emit2(c->m, op_invokevirtual, set_idx);
      return;
    }

    var_ref vr = resolve_var(c, name.start, name.len);
    if (vr.kind == var_not_found) {
      error_at(p, "undeclared variable");
      return;
    }
    local* le = find_local_entry(c, name.start, name.len);
    if (le && le->is_const) {
      error_at(p, "assignment to constant variable");
      return;
    }
    emit_var_read_ref(c, vr);
    emit_to_number(c);
    op_emit(c->m, op_dconst_1);
    op_emit(c->m, is_inc ? op_dadd : op_dsub);
    emit_box_tag(c, op_iconst_0);
    emit_var_write_ref(c, vr);
    return;
  }

  if (match(p, tok_kw_this)) {
    var_ref vr = resolve_var(c, "this", 4);
    if (vr.kind == var_not_found)
      emit_undef(c->cf, c->m);
    else
      emit_var_read_ref(c, vr);
    return;
  }

  if (match(p, tok_kw_await)) {
    parse_unary(p, c);
    uint16_t yield_idx = cf_methodref(
        c->cf, "V6Generator", c->is_async_gen ? "currentAwait" : "currentYield",
        "(LV6Value;)LV6Value;");
    op_emit2(c->m, op_invokestatic, yield_idx);
    return;
  }

  if (match(p, tok_kw_yield)) {
    int delegate = match(p, tok_star);
    int has_operand =
        !(check(p, tok_semi) || check(p, tok_rparen) || check(p, tok_rbrace) ||
          check(p, tok_rbracket) || check(p, tok_comma) || check(p, tok_eof));
    if (has_operand)
      parse_expr(p, c);
    else
      emit_undef(c->cf, c->m);

    if (delegate) {
      uint16_t iter_cls = cf_class(c->cf, "V6Iterator");
      uint16_t iter_ctor =
          cf_methodref(c->cf, "V6Iterator", "<init>", "(LV6Value;)V");
      uint16_t iter_slot = c->next_local_slot++;
      op_emit2(c->m, op_new, iter_cls);
      op_emit(c->m, op_dup_x1);
      op_emit(c->m, op_swap);
      op_emit2(c->m, op_invokespecial, iter_ctor);
      emit_astore(c->m, iter_slot);

      uint16_t has_next_idx =
          cf_methodref(c->cf, "V6Iterator", "hasNext", "()Z");
      uint16_t next_idx =
          cf_methodref(c->cf, "V6Iterator", "next", "()LV6Value;");
      uint16_t yield_idx = cf_methodref(c->cf, "V6Generator", "currentYield",
                                        "(LV6Value;)LV6Value;");

      size_t loop_pos = op_pos(c->m);
      emit_aload(c->m, iter_slot);
      op_emit2(c->m, op_invokevirtual, has_next_idx);
      size_t exit_jump = op_pos(c->m);
      op_emit2(c->m, op_ifeq, 0);

      emit_aload(c->m, iter_slot);
      op_emit2(c->m, op_invokevirtual, next_idx);
      op_emit2(c->m, op_invokestatic, yield_idx);
      op_emit(c->m, op_pop);

      size_t back_jump = op_pos(c->m);
      op_emit2(c->m, op_goto, 0);
      op_patch2(c->m, (uint16_t)(back_jump + 1),
                (uint16_t)(loop_pos - back_jump));

      size_t end_pos = op_pos(c->m);
      op_patch2(c->m, (uint16_t)(exit_jump + 1),
                (uint16_t)(end_pos - exit_jump));

      emit_undef(c->cf, c->m);
      return;
    }

    uint16_t yield_idx = cf_methodref(c->cf, "V6Generator", "currentYield",
                                      "(LV6Value;)LV6Value;");
    op_emit2(c->m, op_invokestatic, yield_idx);
    return;
  }

  if (check(p, tok_kw_async)) {
    lexer async_save_lex = p->lex;
    tok async_save_cur = p->cur;
    tok async_save_prev = p->prev;
    advance(p);
    if (match(p, tok_kw_function)) {
      int is_gen = match(p, tok_star);
      if (check(p, tok_ident))
        advance(p);
      c->pending_async_gen = is_gen;
      compile_closure_value(p, c, 0, 1, NULL);
      if (is_gen)
        emit_wrap_async_generator(c);
      else
        emit_wrap_async(c);
      return;
    }
    if (check(p, tok_ident) && peek_is_arrow(p)) {
      compile_closure_value(p, c, 1, 0, NULL);
      emit_wrap_async(c);
      return;
    }
    if (check(p, tok_lparen) && peek_arrow_after_parens(p)) {
      compile_closure_value(p, c, 1, 1, NULL);
      emit_wrap_async(c);
      return;
    }
    p->lex = async_save_lex;
    p->cur = async_save_cur;
    p->prev = async_save_prev;
    error_at(p, "expected function or arrow after 'async'");
    return;
  }

  if (match(p, tok_kw_function)) {
    int is_gen = match(p, tok_star);
    if (check(p, tok_ident))
      advance(p);
    compile_closure_value(p, c, 0, 1, NULL);
    if (is_gen)
      emit_wrap_generator(c);
    return;
  }

  if (check(p, tok_ident) && peek_is_arrow(p)) {
    compile_closure_value(p, c, 1, 0, NULL);
    return;
  }

  if (match(p, tok_ident)) {
    tok name = p->prev;

    if (name.len == 7 && memcmp(name.start, "require", 7) == 0 &&
        check(p, tok_lparen)) {
      var_ref existing = resolve_var(c, name.start, name.len);
      if (existing.kind == var_not_found) {
        emit_require_expr(p, c);
        return;
      }
    }

    if (is_logical_assign_op(p->cur.kind)) {
      tok_kind op = p->cur.kind;
      advance(p);

      var_ref vr = resolve_var(c, name.start, name.len);
      if (vr.kind == var_not_found) {
        error_at(p, "undeclared variable");
        return;
      }
      local* le = find_local_entry(c, name.start, name.len);
      if (le && le->is_const) {
        error_at(p, "assignment to constant variable");
        return;
      }
      emit_logical_assign_ident(p, c, vr, op);
      return;
    }

    if (check(p, tok_assign) || check(p, tok_plus_eq) ||
        check(p, tok_minus_eq) || check(p, tok_star_eq) ||
        check(p, tok_slash_eq) || check(p, tok_percent_eq) ||
        check(p, tok_amp_eq) || check(p, tok_pipe_eq) ||
        check(p, tok_caret_eq) || check(p, tok_shl_eq) ||
        check(p, tok_shr_eq) || check(p, tok_ushr_eq)) {
      tok_kind op = p->cur.kind;
      advance(p);

      var_ref vr = resolve_var(c, name.start, name.len);
      if (vr.kind == var_not_found) {
        error_at(p, "undeclared variable");
        return;
      }
      local* le = find_local_entry(c, name.start, name.len);
      if (le && le->is_const) {
        error_at(p, "assignment to constant variable");
        return;
      }

      if (op == tok_assign) {
        parse_expr(p, c);
      } else if (op == tok_plus_eq) {
        emit_var_read_ref(c, vr);
        parse_expr(p, c);
        uint16_t idx = cf_methodref(c->cf, "V6Value", "add",
                                    "(LV6Value;LV6Value;)LV6Value;");
        op_emit2(c->m, op_invokestatic, idx);
      } else if (op == tok_amp_eq || op == tok_pipe_eq || op == tok_caret_eq ||
                 op == tok_shl_eq || op == tok_shr_eq || op == tok_ushr_eq) {
        emit_var_read_ref(c, vr);
        parse_expr(p, c);
        const char* mname = op == tok_amp_eq     ? "bitAnd"
                            : op == tok_pipe_eq  ? "bitOr"
                            : op == tok_caret_eq ? "bitXor"
                            : op == tok_shl_eq   ? "shl"
                            : op == tok_shr_eq   ? "shr"
                                                 : "ushr";
        uint16_t idx = cf_methodref(c->cf, "V6Value", mname,
                                    "(LV6Value;LV6Value;)LV6Value;");
        op_emit2(c->m, op_invokestatic, idx);
      } else {
        emit_var_read_ref(c, vr);
        emit_to_number(c);
        parse_expr(p, c);
        emit_to_number(c);
        uint8_t bop = op == tok_minus_eq
                          ? op_dsub
                          : (op == tok_star_eq
                                 ? op_dmul
                                 : (op == tok_slash_eq ? op_ddiv : op_drem));
        op_emit(c->m, bop);
        emit_box_tag(c, op_iconst_0);
      }

      emit_var_write_ref(c, vr);
      return;
    }

    if (check(p, tok_plus_plus) || check(p, tok_minus_minus)) {
      int is_inc = check(p, tok_plus_plus);
      advance(p);
      var_ref vr = resolve_var(c, name.start, name.len);
      if (vr.kind == var_not_found) {
        error_at(p, "undeclared variable");
        return;
      }
      local* le = find_local_entry(c, name.start, name.len);
      if (le && le->is_const) {
        error_at(p, "assignment to constant variable");
        return;
      }
      emit_var_read_ref(c, vr);
      op_emit(c->m, op_dup);
      emit_to_number(c);
      op_emit(c->m, op_dconst_1);
      op_emit(c->m, is_inc ? op_dadd : op_dsub);
      emit_box_tag(c, op_iconst_0);
      emit_var_write_ref(c, vr);
      op_emit(c->m, op_pop);
      return;
    }

    var_ref vr = resolve_var(c, name.start, name.len);
    if (vr.kind == var_not_found) {
      error_at(p, "undeclared variable");
      return;
    }
    if (check(p, tok_lparen)) {
      const char* lambda_name = find_direct_fn(c, name.start, name.len);
      if (lambda_name) {
        compile_direct_call(p, c, vr, lambda_name);
        return;
      }
    }
    emit_var_read_ref(c, vr);
    return;
  }

  if (check(p, tok_lparen) && peek_arrow_after_parens(p)) {
    compile_closure_value(p, c, 1, 1, NULL);
    return;
  }

  if (match(p, tok_lparen)) {
    if (check(p, tok_lbrace)) {
      lexer save_lex = p->lex;
      tok save_cur = p->cur;
      tok save_prev = p->prev;
      advance(p);
      const char* pattern_start = p->cur.start;
      skip_balanced(p, tok_lbrace, tok_rbrace);
      if (check(p, tok_assign)) {
        advance(p);
        parse_expr(p, c);
        op_emit(c->m, op_dup);
        uint16_t src_slot = c->next_local_slot++;
        emit_astore(c->m, src_slot);
        parser pp;
        parser_init(&pp, pattern_start);
        parse_object_pattern(&pp, c, tok_kw_var, src_slot);
        if (!match(p, tok_rparen))
          error_at(p, "expected ')'");
        return;
      }
      p->lex = save_lex;
      p->cur = save_cur;
      p->prev = save_prev;
    }
    parse_expr(p, c);
    if (!match(p, tok_rparen))
      error_at(p, "expected ')'");
    return;
  }

  if (match(p, tok_lbrace)) {
    parse_object_literal(p, c);
    return;
  }

  if (match(p, tok_lbracket)) {
    parse_array_literal(p, c);
    return;
  }

  if (check(p, tok_slash) || check(p, tok_slash_eq)) {
    lexer regex_lex;
    regex_lex.src = p->lex.src;
    regex_lex.cur = p->cur.start;
    regex_lex.line = p->cur.line;
    tok regex_tok = lex_regex_literal(&regex_lex);
    p->lex = regex_lex;
    p->cur = regex_tok;
    advance(p);
    emit_regex_literal(p, c, p->prev);
    return;
  }

  error_at(p, "expected expression");
  advance(p);
}

static void parse_unary(parser* p, compiler* c) {
  if (match(p, tok_minus)) {
    parse_unary(p, c);
    uint16_t idx =
        cf_methodref(c->cf, "V6Value", "neg", "(LV6Value;)LV6Value;");
    op_emit2(c->m, op_invokestatic, idx);
    return;
  }
  if (match(p, tok_bang)) {
    parse_unary(p, c);
    emit_truthy(c);
    op_emit(c->m, op_iconst_1);
    op_emit(c->m, op_ixor);
    op_emit(c->m, op_i2d);
    emit_box_bool(c);
    return;
  }
  if (match(p, tok_tilde)) {
    parse_unary(p, c);
    uint16_t idx =
        cf_methodref(c->cf, "V6Value", "bitNot", "(LV6Value;)LV6Value;");
    op_emit2(c->m, op_invokestatic, idx);
    return;
  }
  if (match(p, tok_kw_typeof)) {
    parse_unary(p, c);
    uint16_t idx =
        cf_methodref(c->cf, "V6Value", "typeOf", "()Ljava/lang/String;");
    op_emit2(c->m, op_invokevirtual, idx);
    emit_box_ref_computed(c, V6_TAG_STR);
    return;
  }
  parse_postfix(p, c);
}

static void parse_exp(parser* p, compiler* c) {
  parse_unary(p, c);
  if (check(p, tok_star_star)) {
    advance(p);
    parse_exp(p, c);
    uint16_t idx =
        cf_methodref(c->cf, "V6Value", "pow", "(LV6Value;LV6Value;)LV6Value;");
    op_emit2(c->m, op_invokestatic, idx);
  }
}

static void parse_mul(parser* p, compiler* c) {
  parse_exp(p, c);
  while (check(p, tok_star) || check(p, tok_slash) || check(p, tok_percent)) {
    tok_kind k = p->cur.kind;
    advance(p);
    parse_exp(p, c);
    const char* mname =
        k == tok_star ? "mul" : (k == tok_slash ? "div" : "mod");
    uint16_t idx =
        cf_methodref(c->cf, "V6Value", mname, "(LV6Value;LV6Value;)LV6Value;");
    op_emit2(c->m, op_invokestatic, idx);
  }
}

static void parse_add(parser* p, compiler* c) {
  parse_mul(p, c);
  while (check(p, tok_plus) || check(p, tok_minus)) {
    tok_kind k = p->cur.kind;
    advance(p);
    if (k == tok_plus) {
      parse_mul(p, c);
      uint16_t idx = cf_methodref(c->cf, "V6Value", "add",
                                  "(LV6Value;LV6Value;)LV6Value;");
      op_emit2(c->m, op_invokestatic, idx);
    } else {
      parse_mul(p, c);
      uint16_t idx = cf_methodref(c->cf, "V6Value", "sub",
                                  "(LV6Value;LV6Value;)LV6Value;");
      op_emit2(c->m, op_invokestatic, idx);
    }
  }
}

static void parse_shift(parser* p, compiler* c) {
  parse_add(p, c);
  while (check(p, tok_shl) || check(p, tok_shr) || check(p, tok_ushr)) {
    tok_kind k = p->cur.kind;
    advance(p);
    parse_add(p, c);
    const char* name = k == tok_shl ? "shl" : (k == tok_shr ? "shr" : "ushr");
    uint16_t idx =
        cf_methodref(c->cf, "V6Value", name, "(LV6Value;LV6Value;)LV6Value;");
    op_emit2(c->m, op_invokestatic, idx);
  }
}

static void parse_cmp(parser* p, compiler* c) {
  parse_shift(p, c);
  for (;;) {
    if (check(p, tok_lt) || check(p, tok_gt) || check(p, tok_le) ||
        check(p, tok_ge)) {
      tok_kind k = p->cur.kind;
      advance(p);
      parse_shift(p, c);
      const char* name = k == tok_lt   ? "lt"
                         : k == tok_le ? "le"
                         : k == tok_gt ? "gt"
                                       : "ge";
      uint16_t idx =
          cf_methodref(c->cf, "V6Value", name, "(LV6Value;LV6Value;)Z");
      op_emit2(c->m, op_invokestatic, idx);
      op_emit(c->m, op_i2d);
      emit_box_bool(c);
    } else if (match(p, tok_kw_instanceof)) {
      parse_shift(p, c);
      uint16_t idx =
          cf_methodref(c->cf, "V6Value", "instanceOf", "(LV6Value;LV6Value;)Z");
      op_emit2(c->m, op_invokestatic, idx);
      op_emit(c->m, op_i2d);
      emit_box_bool(c);
    } else if (match(p, tok_kw_in)) {
      parse_shift(p, c);
      uint16_t idx =
          cf_methodref(c->cf, "V6Value", "hasProp", "(LV6Value;LV6Value;)Z");
      op_emit2(c->m, op_invokestatic, idx);
      op_emit(c->m, op_i2d);
      emit_box_bool(c);
    } else {
      break;
    }
  }
}

static void parse_eq(parser* p, compiler* c) {
  parse_cmp(p, c);
  while (check(p, tok_eq) || check(p, tok_neq) || check(p, tok_eq_strict) ||
         check(p, tok_neq_strict)) {
    tok_kind k = p->cur.kind;
    advance(p);
    parse_cmp(p, c);
    int strict = k == tok_eq_strict || k == tok_neq_strict;
    int negate = k == tok_neq || k == tok_neq_strict;
    uint16_t idx =
        cf_methodref(c->cf, "V6Value", strict ? "strictEquals" : "looseEquals",
                     "(LV6Value;LV6Value;)Z");
    op_emit2(c->m, op_invokestatic, idx);
    if (negate) {
      op_emit(c->m, op_iconst_1);
      op_emit(c->m, op_ixor);
    }
    op_emit(c->m, op_i2d);
    emit_box_bool(c);
  }
}

static void parse_bitand(parser* p, compiler* c) {
  parse_eq(p, c);
  while (check(p, tok_amp)) {
    advance(p);
    parse_eq(p, c);
    uint16_t idx = cf_methodref(c->cf, "V6Value", "bitAnd",
                                "(LV6Value;LV6Value;)LV6Value;");
    op_emit2(c->m, op_invokestatic, idx);
  }
}

static void parse_bitxor(parser* p, compiler* c) {
  parse_bitand(p, c);
  while (check(p, tok_caret)) {
    advance(p);
    parse_bitand(p, c);
    uint16_t idx = cf_methodref(c->cf, "V6Value", "bitXor",
                                "(LV6Value;LV6Value;)LV6Value;");
    op_emit2(c->m, op_invokestatic, idx);
  }
}

static void parse_bitor(parser* p, compiler* c) {
  parse_bitxor(p, c);
  while (check(p, tok_pipe)) {
    advance(p);
    parse_bitxor(p, c);
    uint16_t idx = cf_methodref(c->cf, "V6Value", "bitOr",
                                "(LV6Value;LV6Value;)LV6Value;");
    op_emit2(c->m, op_invokestatic, idx);
  }
}

static void parse_and(parser* p, compiler* c) {
  parse_bitor(p, c);
  while (match(p, tok_amp_amp)) {
    op_emit(c->m, op_dup);
    emit_truthy(c);
    size_t is_left_pos = op_pos(c->m);
    op_emit2(c->m, op_ifeq, 0);
    op_emit(c->m, op_pop);
    parse_bitor(p, c);
    size_t end_pos = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(is_left_pos + 1),
              (uint16_t)(end_pos - is_left_pos));
  }
}

static void parse_or(parser* p, compiler* c) {
  parse_and(p, c);
  while (match(p, tok_pipe_pipe)) {
    op_emit(c->m, op_dup);
    emit_truthy(c);
    size_t is_left_pos = op_pos(c->m);
    op_emit2(c->m, op_ifne, 0);
    op_emit(c->m, op_pop);
    parse_and(p, c);
    size_t end_pos = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(is_left_pos + 1),
              (uint16_t)(end_pos - is_left_pos));
  }
}

static void parse_nullish(parser* p, compiler* c) {
  parse_or(p, c);
  while (match(p, tok_question_question)) {
    op_emit(c->m, op_dup);
    uint16_t idx = cf_methodref(c->cf, "V6Value", "isNullish", "()Z");
    op_emit2(c->m, op_invokevirtual, idx);
    size_t is_left_pos = op_pos(c->m);
    op_emit2(c->m, op_ifeq, 0);
    op_emit(c->m, op_pop);
    parse_or(p, c);
    size_t end_pos = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(is_left_pos + 1),
              (uint16_t)(end_pos - is_left_pos));
  }
}

static void parse_ternary(parser* p, compiler* c) {
  parse_nullish(p, c);
  if (match(p, tok_question)) {
    emit_truthy(c);
    size_t else_jump = op_pos(c->m);
    op_emit2(c->m, op_ifeq, 0);

    parse_expr(p, c);
    size_t end_jump = op_pos(c->m);
    op_emit2(c->m, op_goto, 0);

    size_t else_pos = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(else_jump + 1),
              (uint16_t)(else_pos - else_jump));

    expect(p, tok_colon);
    parse_expr(p, c);

    size_t end_pos = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(end_jump + 1), (uint16_t)(end_pos - end_jump));
  }
}

static void parse_expr(parser* p, compiler* c) {
  parse_ternary(p, c);
}

int compile_expr(parser* p, compiler* c) {
  parse_expr(p, c);
  return p->had_error ? -1 : 0;
}

static void parse_stmt(parser* p, compiler* c);
static void prescan_decls(compiler* c, const char* src, int hoist_functions);

static void parse_one_declarator_named(parser* p, compiler* c, tok_kind kind,
                                       tok name) {
  if (kind == tok_kw_var) {
    var_ref vr = resolve_var(c, name.start, name.len);
    if (vr.kind == var_not_found) {
      error_at(p, "internal: hoisted var missing");
      return;
    }
    if (match(p, tok_assign)) {
      parse_expr(p, c);
      emit_var_write_ref(c, vr);
      op_emit(c->m, op_pop);
    }
    return;
  }

  if (c->brace_depth == 0) {
    local* le = find_local_entry(c, name.start, name.len);
    if (le) {
      if (match(p, tok_assign)) {
        parse_expr(p, c);
        var_ref vr;
        vr.kind = var_local;
        vr.index = le->slot;
        emit_var_write_ref(c, vr);
        op_emit(c->m, op_pop);
      }
      return;
    }
  }

  if (match(p, tok_assign))
    parse_expr(p, c);
  else
    emit_undef(c->cf, c->m);

  uint16_t slot = c->next_local_slot++;
  emit_var_declare(c, slot);
  add_local(c, name, slot, 0, kind == tok_kw_const);
}

static void skip_balanced(parser* p, tok_kind open, tok_kind close) {
  int depth = 1;
  while (depth > 0 && !check(p, tok_eof)) {
    if (check(p, open))
      depth++;
    else if (check(p, close))
      depth--;
    advance(p);
  }
}

static void skip_field_init(parser* p) {
  int depth = 0;
  while (!check(p, tok_eof)) {
    if (check(p, tok_lparen) || check(p, tok_lbracket) ||
        check(p, tok_lbrace)) {
      depth++;
    } else if (check(p, tok_rparen) || check(p, tok_rbracket)) {
      depth--;
    } else if (check(p, tok_rbrace)) {
      if (depth == 0)
        return;
      depth--;
    } else if (check(p, tok_semi) && depth == 0) {
      advance(p);
      return;
    }
    advance(p);
  }
}

static void bind_pattern_target(parser* p, compiler* c, tok_kind kind,
                                tok name) {
  if (kind == tok_kw_var) {
    var_ref vr = resolve_var(c, name.start, name.len);
    if (vr.kind == var_not_found) {
      error_at(p, "internal: hoisted var missing");
      return;
    }
    emit_var_write_ref(c, vr);
    op_emit(c->m, op_pop);
  } else if (c->brace_depth == 0 && find_local_entry(c, name.start, name.len)) {
    local* le = find_local_entry(c, name.start, name.len);
    var_ref vr;
    vr.kind = var_local;
    vr.index = le->slot;
    emit_var_write_ref(c, vr);
    op_emit(c->m, op_pop);
  } else {
    uint16_t slot = c->next_local_slot++;
    emit_var_declare(c, slot);
    add_local(c, name, slot, 0, kind == tok_kw_const);
  }
}

static void emit_pattern_default(parser* p, compiler* c) {
  if (!match(p, tok_assign))
    return;
  op_emit(c->m, op_dup);
  uint16_t isundef_idx = cf_methodref(c->cf, "V6Value", "isUndefined", "()Z");
  op_emit2(c->m, op_invokevirtual, isundef_idx);
  size_t skip_jump = op_pos(c->m);
  op_emit2(c->m, op_ifeq, 0);
  op_emit(c->m, op_pop);
  parse_expr(p, c);
  size_t after = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(skip_jump + 1), (uint16_t)(after - skip_jump));
}

static void parse_array_pattern(parser* p, compiler* c, tok_kind kind,
                                uint16_t src_slot);
static void parse_object_pattern(parser* p, compiler* c, tok_kind kind,
                                 uint16_t src_slot);

static void parse_array_pattern(parser* p, compiler* c, tok_kind kind,
                                uint16_t src_slot) {
  uint16_t getprop_idx = cf_methodref(c->cf, "V6Value", "getProp",
                                      "(Ljava/lang/String;)LV6Value;");
  int idx = 0;
  for (;;) {
    if (check(p, tok_rbracket))
      break;
    if (match(p, tok_comma)) {
      idx++;
      continue;
    }
    if (match(p, tok_ellipsis)) {
      if (!expect(p, tok_ident))
        return;
      tok name = p->prev;
      uint16_t restfrom_idx =
          cf_methodref(c->cf, "V6Value", "restFrom", "(I)LV6Array;");
      emit_aload(c->m, src_slot);
      emit_iconst(c->m, idx);
      op_emit2(c->m, op_invokevirtual, restfrom_idx);
      emit_box_object_ref(c);
      bind_pattern_target(p, c, kind, name);
      break;
    }

    if (check(p, tok_lbracket) || check(p, tok_lbrace)) {
      int nested_is_array = check(p, tok_lbracket);
      tok_kind open = nested_is_array ? tok_lbracket : tok_lbrace;
      tok_kind close = nested_is_array ? tok_rbracket : tok_rbrace;
      advance(p);
      const char* nested_pattern_start = p->cur.start;
      skip_balanced(p, open, close);

      char idxbuf[16];
      snprintf(idxbuf, sizeof(idxbuf), "%d", idx);
      uint16_t key_idx = cf_string(c->cf, idxbuf);
      emit_aload(c->m, src_slot);
      op_emit2(c->m, op_ldc_w, key_idx);
      op_emit2(c->m, op_invokevirtual, getprop_idx);

      emit_pattern_default(p, c);

      uint16_t nested_slot = c->next_local_slot++;
      emit_astore(c->m, nested_slot);

      parser pp;
      parser_init(&pp, nested_pattern_start);
      if (nested_is_array)
        parse_array_pattern(&pp, c, kind, nested_slot);
      else
        parse_object_pattern(&pp, c, kind, nested_slot);

      idx++;
      if (!match(p, tok_comma))
        break;
      continue;
    }

    if (!expect(p, tok_ident))
      return;
    tok name = p->prev;

    char idxbuf[16];
    snprintf(idxbuf, sizeof(idxbuf), "%d", idx);
    uint16_t key_idx = cf_string(c->cf, idxbuf);
    emit_aload(c->m, src_slot);
    op_emit2(c->m, op_ldc_w, key_idx);
    op_emit2(c->m, op_invokevirtual, getprop_idx);

    emit_pattern_default(p, c);
    bind_pattern_target(p, c, kind, name);
    idx++;
    if (!match(p, tok_comma))
      break;
  }
}

static void parse_object_pattern(parser* p, compiler* c, tok_kind kind,
                                 uint16_t src_slot) {
  uint16_t getprop_idx = cf_methodref(c->cf, "V6Value", "getProp",
                                      "(Ljava/lang/String;)LV6Value;");
  for (;;) {
    if (check(p, tok_rbrace))
      break;
    if (!expect(p, tok_ident))
      return;
    tok key_name = p->prev;

    char* keystr = dup_tok(key_name);
    uint16_t key_idx = cf_string(c->cf, keystr);
    free(keystr);

    int has_colon = match(p, tok_colon);

    if (has_colon && (check(p, tok_lbracket) || check(p, tok_lbrace))) {
      int nested_is_array = check(p, tok_lbracket);
      tok_kind open = nested_is_array ? tok_lbracket : tok_lbrace;
      tok_kind close = nested_is_array ? tok_rbracket : tok_rbrace;
      advance(p);
      const char* nested_pattern_start = p->cur.start;
      skip_balanced(p, open, close);

      emit_aload(c->m, src_slot);
      op_emit2(c->m, op_ldc_w, key_idx);
      op_emit2(c->m, op_invokevirtual, getprop_idx);

      emit_pattern_default(p, c);

      uint16_t nested_slot = c->next_local_slot++;
      emit_astore(c->m, nested_slot);

      parser pp;
      parser_init(&pp, nested_pattern_start);
      if (nested_is_array)
        parse_array_pattern(&pp, c, kind, nested_slot);
      else
        parse_object_pattern(&pp, c, kind, nested_slot);

      if (!match(p, tok_comma))
        break;
      continue;
    }

    tok target_name = key_name;
    if (has_colon) {
      if (!expect(p, tok_ident))
        return;
      target_name = p->prev;
    }

    emit_aload(c->m, src_slot);
    op_emit2(c->m, op_ldc_w, key_idx);
    op_emit2(c->m, op_invokevirtual, getprop_idx);

    emit_pattern_default(p, c);
    bind_pattern_target(p, c, kind, target_name);
    if (!match(p, tok_comma))
      break;
  }
}

static void parse_one_declarator(parser* p, compiler* c, tok_kind kind) {
  if (check(p, tok_lbracket) || check(p, tok_lbrace)) {
    int is_array = check(p, tok_lbracket);
    tok_kind open = is_array ? tok_lbracket : tok_lbrace;
    tok_kind close = is_array ? tok_rbracket : tok_rbrace;
    advance(p);
    const char* pattern_start = p->cur.start;
    skip_balanced(p, open, close);
    if (!expect(p, tok_assign))
      return;
    parse_expr(p, c);
    uint16_t src_slot = c->next_local_slot++;
    emit_astore(c->m, src_slot);

    parser pp;
    parser_init(&pp, pattern_start);
    if (is_array)
      parse_array_pattern(&pp, c, kind, src_slot);
    else
      parse_object_pattern(&pp, c, kind, src_slot);
    return;
  }

  if (!expect(p, tok_ident))
    return;
  parse_one_declarator_named(p, c, kind, p->prev);
}

static void parse_var_decl(parser* p, compiler* c, tok_kind kind) {
  parse_one_declarator(p, c, kind);
  while (match(p, tok_comma))
    parse_one_declarator(p, c, kind);
}

static void parse_block(parser* p, compiler* c) {
  int saved_count = c->local_count;
  c->brace_depth++;
  while (!check(p, tok_rbrace) && !check(p, tok_eof))
    parse_stmt(p, c);
  expect(p, tok_rbrace);
  c->brace_depth--;
  for (int i = saved_count; i < c->local_count; i++) {
    if (!c->locals[i].is_var)
      c->locals[i].dead = 1;
  }
}

static void parse_if(parser* p, compiler* c) {
  expect(p, tok_lparen);
  parse_expr(p, c);
  expect(p, tok_rparen);
  emit_truthy(c);

  size_t else_jump = op_pos(c->m);
  op_emit2(c->m, op_ifeq, 0);

  parse_stmt(p, c);

  size_t end_jump = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);

  size_t else_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(else_jump + 1), (uint16_t)(else_pos - else_jump));

  if (match(p, tok_kw_else))
    parse_stmt(p, c);

  size_t end_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(end_jump + 1), (uint16_t)(end_pos - end_jump));
}

static void push_loop(compiler* c, size_t continue_target) {
  c->breaks[c->break_depth].count = 0;
  c->break_depth++;
  c->continues[c->continue_depth] = continue_target;
  int continue_idx = c->continue_depth;
  c->continue_depth++;
  for (int i = 0; i < c->pending_label_count; i++) {
    for (int j = c->label_count - 1; j >= 0; j--) {
      if (c->label_lens[j] == c->pending_label_lens[i] &&
          memcmp(c->label_names[j], c->pending_label_names[i],
                 c->label_lens[j]) == 0 &&
          c->label_continue_depth[j] == -1) {
        c->label_continue_depth[j] = continue_idx;
        break;
      }
    }
  }
}

static void patch_breaks(compiler* c, size_t end_pos) {
  break_ctx* bc = &c->breaks[c->break_depth - 1];
  for (size_t i = 0; i < bc->count; i++)
    op_patch2(c->m, (uint16_t)(bc->jumps[i] + 1),
              (uint16_t)(end_pos - bc->jumps[i]));
}

static void pop_loop(compiler* c, size_t end_pos) {
  patch_breaks(c, end_pos);
  c->break_depth--;
  c->continue_depth--;
}

static void parse_while(parser* p, compiler* c) {
  size_t start_pos = op_pos(c->m);

  expect(p, tok_lparen);
  parse_expr(p, c);
  expect(p, tok_rparen);
  emit_truthy(c);

  size_t exit_jump = op_pos(c->m);
  op_emit2(c->m, op_ifeq, 0);

  push_loop(c, start_pos);
  parse_stmt(p, c);

  size_t back_jump = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);
  op_patch2(c->m, (uint16_t)(back_jump + 1), (uint16_t)(start_pos - back_jump));

  size_t end_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(exit_jump + 1), (uint16_t)(end_pos - exit_jump));
  pop_loop(c, end_pos);
}

static void parse_for_in(parser* p, compiler* c, tok_kind kind, tok name) {
  parse_expr(p, c);
  expect(p, tok_rparen);

  uint16_t keys_slot = c->next_local_slot++;
  uint16_t idx_slot = c->next_local_slot++;
  uint16_t len_slot = c->next_local_slot++;

  uint16_t enumkeys_idx =
      cf_methodref(c->cf, "V6Value", "enumKeys", "()LV6Value;");
  op_emit2(c->m, op_invokevirtual, enumkeys_idx);
  emit_astore(c->m, keys_slot);

  emit_box_const(c->cf, c->m, op_iconst_0, op_dconst_0);
  emit_astore(c->m, idx_slot);

  uint16_t length_str = cf_string(c->cf, "length");
  uint16_t getprop_idx = cf_methodref(c->cf, "V6Value", "getProp",
                                      "(Ljava/lang/String;)LV6Value;");
  emit_aload(c->m, keys_slot);
  op_emit2(c->m, op_ldc_w, length_str);
  op_emit2(c->m, op_invokevirtual, getprop_idx);
  emit_astore(c->m, len_slot);

  size_t cond_pos = op_pos(c->m);
  emit_aload(c->m, idx_slot);
  emit_to_number(c);
  emit_aload(c->m, len_slot);
  emit_to_number(c);
  op_emit(c->m, op_dcmpg);
  size_t exit_jump = op_pos(c->m);
  op_emit2(c->m, op_ifge, 0);

  size_t body_jump = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);

  size_t inc_pos = op_pos(c->m);
  emit_aload(c->m, idx_slot);
  emit_to_number(c);
  op_emit(c->m, op_dconst_1);
  op_emit(c->m, op_dadd);
  emit_box_tag(c, op_iconst_0);
  emit_astore(c->m, idx_slot);
  size_t inc_to_cond = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);
  op_patch2(c->m, (uint16_t)(inc_to_cond + 1),
            (uint16_t)(cond_pos - inc_to_cond));

  size_t body_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(body_jump + 1), (uint16_t)(body_pos - body_jump));

  var_ref var_vr;
  var_vr.kind = var_local;
  var_vr.index = 0;
  if (kind == tok_kw_var) {
    var_vr = resolve_var(c, name.start, name.len);
    if (var_vr.kind == var_not_found) {
      error_at(p, "internal: hoisted var missing");
      return;
    }
  } else {
    var_vr.index = c->next_local_slot++;
    add_local(c, name, var_vr.index, 0, kind == tok_kw_const);
  }

  uint16_t tostring_idx =
      cf_methodref(c->cf, "V6Value", "toString", "()Ljava/lang/String;");
  emit_aload(c->m, keys_slot);
  emit_aload(c->m, idx_slot);
  op_emit2(c->m, op_invokevirtual, tostring_idx);
  op_emit2(c->m, op_invokevirtual, getprop_idx);
  if (kind == tok_kw_var) {
    emit_var_write_ref(c, var_vr);
    op_emit(c->m, op_pop);
  } else {
    emit_var_declare(c, var_vr.index);
  }

  push_loop(c, inc_pos);
  parse_stmt(p, c);

  size_t body_to_inc = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);
  op_patch2(c->m, (uint16_t)(body_to_inc + 1),
            (uint16_t)(inc_pos - body_to_inc));

  size_t end_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(exit_jump + 1), (uint16_t)(end_pos - exit_jump));
  pop_loop(c, end_pos);
}

static void parse_for_of(parser* p, compiler* c, tok_kind kind, tok name) {
  uint16_t iter_slot = c->next_local_slot++;
  uint16_t iter_cls = cf_class(c->cf, "V6Iterator");
  uint16_t iter_ctor =
      cf_methodref(c->cf, "V6Iterator", "<init>", "(LV6Value;)V");

  op_emit2(c->m, op_new, iter_cls);
  op_emit(c->m, op_dup);
  parse_expr(p, c);
  expect(p, tok_rparen);
  op_emit2(c->m, op_invokespecial, iter_ctor);
  emit_astore(c->m, iter_slot);

  uint16_t has_next_idx = cf_methodref(c->cf, "V6Iterator", "hasNext", "()Z");
  uint16_t next_idx = cf_methodref(c->cf, "V6Iterator", "next", "()LV6Value;");

  size_t cond_pos = op_pos(c->m);
  emit_aload(c->m, iter_slot);
  op_emit2(c->m, op_invokevirtual, has_next_idx);
  size_t exit_jump = op_pos(c->m);
  op_emit2(c->m, op_ifeq, 0);

  var_ref var_vr;
  var_vr.kind = var_local;
  var_vr.index = 0;
  if (kind == tok_kw_var) {
    var_vr = resolve_var(c, name.start, name.len);
    if (var_vr.kind == var_not_found) {
      error_at(p, "internal: hoisted var missing");
      return;
    }
  } else {
    var_vr.index = c->next_local_slot++;
    add_local(c, name, var_vr.index, 0, kind == tok_kw_const);
  }

  emit_aload(c->m, iter_slot);
  op_emit2(c->m, op_invokevirtual, next_idx);
  if (kind == tok_kw_var) {
    emit_var_write_ref(c, var_vr);
    op_emit(c->m, op_pop);
  } else {
    emit_var_declare(c, var_vr.index);
  }

  push_loop(c, cond_pos);
  parse_stmt(p, c);

  size_t back_jump = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);
  op_patch2(c->m, (uint16_t)(back_jump + 1), (uint16_t)(cond_pos - back_jump));

  size_t end_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(exit_jump + 1), (uint16_t)(end_pos - exit_jump));
  pop_loop(c, end_pos);
}

static void parse_for_await_of(parser* p, compiler* c, tok_kind kind,
                               tok name) {
  uint16_t iterable_slot = c->next_local_slot++;
  parse_expr(p, c);
  expect(p, tok_rparen);
  emit_astore(c->m, iterable_slot);

  uint16_t next_str = cf_string(c->cf, "next");
  uint16_t value_str = cf_string(c->cf, "value");
  uint16_t done_str = cf_string(c->cf, "done");
  uint16_t getprop_idx = cf_methodref(c->cf, "V6Value", "getProp",
                                      "(Ljava/lang/String;)LV6Value;");
  uint16_t call_idx =
      cf_methodref(c->cf, "V6Value", "call", "(LV6Value;[LV6Value;)LV6Value;");
  uint16_t await_idx = cf_methodref(
      c->cf, "V6Generator", c->is_async_gen ? "currentAwait" : "currentYield",
      "(LV6Value;)LV6Value;");

  size_t cond_pos = op_pos(c->m);
  emit_aload(c->m, iterable_slot);
  op_emit2(c->m, op_ldc_w, next_str);
  op_emit2(c->m, op_invokevirtual, getprop_idx);
  emit_aload(c->m, iterable_slot);
  emit_iconst(c->m, 0);
  op_emit2(c->m, op_anewarray, value_class(c->cf));
  op_emit2(c->m, op_invokevirtual, call_idx);
  op_emit2(c->m, op_invokestatic, await_idx);
  uint16_t result_slot = c->next_local_slot++;
  emit_astore(c->m, result_slot);

  emit_aload(c->m, result_slot);
  op_emit2(c->m, op_ldc_w, done_str);
  op_emit2(c->m, op_invokevirtual, getprop_idx);
  emit_truthy(c);
  size_t exit_jump = op_pos(c->m);
  op_emit2(c->m, op_ifne, 0);

  var_ref var_vr;
  var_vr.kind = var_local;
  var_vr.index = 0;
  if (kind == tok_kw_var) {
    var_vr = resolve_var(c, name.start, name.len);
    if (var_vr.kind == var_not_found) {
      error_at(p, "internal: hoisted var missing");
      return;
    }
  } else {
    var_vr.index = c->next_local_slot++;
    add_local(c, name, var_vr.index, 0, kind == tok_kw_const);
  }

  emit_aload(c->m, result_slot);
  op_emit2(c->m, op_ldc_w, value_str);
  op_emit2(c->m, op_invokevirtual, getprop_idx);
  if (kind == tok_kw_var) {
    emit_var_write_ref(c, var_vr);
    op_emit(c->m, op_pop);
  } else {
    emit_var_declare(c, var_vr.index);
  }

  push_loop(c, cond_pos);
  parse_stmt(p, c);

  size_t back_jump = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);
  op_patch2(c->m, (uint16_t)(back_jump + 1), (uint16_t)(cond_pos - back_jump));

  size_t end_pos2 = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(exit_jump + 1), (uint16_t)(end_pos2 - exit_jump));
  pop_loop(c, end_pos2);
}

static void parse_for_of_pattern(parser* p, compiler* c, tok_kind kind,
                                 int is_array, const char* pattern_start) {
  uint16_t iter_slot = c->next_local_slot++;
  uint16_t iter_cls = cf_class(c->cf, "V6Iterator");
  uint16_t iter_ctor =
      cf_methodref(c->cf, "V6Iterator", "<init>", "(LV6Value;)V");

  op_emit2(c->m, op_new, iter_cls);
  op_emit(c->m, op_dup);
  parse_expr(p, c);
  expect(p, tok_rparen);
  op_emit2(c->m, op_invokespecial, iter_ctor);
  emit_astore(c->m, iter_slot);

  uint16_t has_next_idx = cf_methodref(c->cf, "V6Iterator", "hasNext", "()Z");
  uint16_t next_idx = cf_methodref(c->cf, "V6Iterator", "next", "()LV6Value;");

  size_t cond_pos = op_pos(c->m);
  emit_aload(c->m, iter_slot);
  op_emit2(c->m, op_invokevirtual, has_next_idx);
  size_t exit_jump = op_pos(c->m);
  op_emit2(c->m, op_ifeq, 0);

  emit_aload(c->m, iter_slot);
  op_emit2(c->m, op_invokevirtual, next_idx);
  uint16_t val_slot = c->next_local_slot++;
  emit_astore(c->m, val_slot);

  parser pp;
  parser_init(&pp, pattern_start);
  if (is_array)
    parse_array_pattern(&pp, c, kind, val_slot);
  else
    parse_object_pattern(&pp, c, kind, val_slot);

  push_loop(c, cond_pos);
  parse_stmt(p, c);

  size_t back_jump = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);
  op_patch2(c->m, (uint16_t)(back_jump + 1), (uint16_t)(cond_pos - back_jump));

  size_t end_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(exit_jump + 1), (uint16_t)(end_pos - exit_jump));
  pop_loop(c, end_pos);
}

#define v6_max_raw_accums 8
#define v6_raw_ctx_init 0
#define v6_raw_ctx_header 1
#define v6_raw_ctx_body 2

typedef struct raw_accum {
  const char* name;
  size_t len;
  uint16_t real_slot;
  uint16_t shadow_slot;
} raw_accum;

typedef struct raw_loop_ctx {
  const char* counter_name;
  size_t counter_len;
  uint16_t counter_slot;
  raw_accum accums[v6_max_raw_accums];
  int accum_count;
} raw_loop_ctx;

static int raw_ident_is_counter(raw_loop_ctx* rl, tok name) {
  return name.len == rl->counter_len &&
         memcmp(name.start, rl->counter_name, rl->counter_len) == 0;
}

static raw_accum* raw_find_accum(raw_loop_ctx* rl, const char* name,
                                 size_t len) {
  for (int i = 0; i < rl->accum_count; i++) {
    if (rl->accums[i].len == len && memcmp(rl->accums[i].name, name, len) == 0)
      return &rl->accums[i];
  }
  return NULL;
}

static void emit_dconst_val(class_file* cf, method* m, double v) {
  uint16_t idx = cf_double(cf, v);
  op_emit2(m, op_ldc2_w, idx);
}

static const char* raw_find_body_start(parser* p) {
  lexer lx = p->lex;
  tok t = p->cur;
  int depth = 1;
  for (;;) {
    if (t.kind == tok_eof)
      return NULL;
    if (t.kind == tok_lparen) {
      depth++;
    } else if (t.kind == tok_rparen) {
      depth--;
      if (depth == 0) {
        t = lex_next(&lx);
        return t.start;
      }
    }
    t = lex_next(&lx);
  }
}

static int raw_scan_body_accums(const char* body_start, raw_loop_ctx* rl) {
  lexer lx;
  lex_init(&lx, body_start);
  tok t = lex_next(&lx);
  if (t.kind != tok_lbrace)
    return 0;
  int depth = 1;
  t = lex_next(&lx);
  while (depth > 0) {
    if (t.kind == tok_eof)
      return 0;
    if (t.kind == tok_lbrace) {
      depth++;
      t = lex_next(&lx);
      continue;
    }
    if (t.kind == tok_rbrace) {
      depth--;
      t = lex_next(&lx);
      continue;
    }
    if (t.kind == tok_ident) {
      tok name = t;
      tok next = lex_next(&lx);
      if ((next.kind == tok_plus_eq || next.kind == tok_minus_eq ||
           next.kind == tok_star_eq || next.kind == tok_assign) &&
          !raw_ident_is_counter(rl, name) &&
          !raw_find_accum(rl, name.start, name.len)) {
        if (rl->accum_count >= v6_max_raw_accums)
          return 0;
        rl->accums[rl->accum_count].name = name.start;
        rl->accums[rl->accum_count].len = name.len;
        rl->accum_count++;
      }
      t = next;
      continue;
    }
    t = lex_next(&lx);
  }
  return 1;
}

static int compile_raw_expr(parser* p, compiler* c, raw_loop_ctx* rl, int emit,
                            int mode);

static int compile_raw_primary(parser* p, compiler* c, raw_loop_ctx* rl,
                               int emit, int mode) {
  if (check(p, tok_num)) {
    double v = p->cur.num;
    advance(p);
    if (emit)
      emit_dconst_val(c->cf, c->m, v);
    return 1;
  }
  if (check(p, tok_lparen)) {
    advance(p);
    if (!compile_raw_expr(p, c, rl, emit, mode))
      return 0;
    if (!check(p, tok_rparen))
      return 0;
    advance(p);
    return 1;
  }
  if (check(p, tok_ident)) {
    tok name = p->cur;
    if (raw_ident_is_counter(rl, name)) {
      if (mode == v6_raw_ctx_init)
        return 0;
      advance(p);
      if (emit)
        emit_dload(c->m, rl->counter_slot);
      return 1;
    }
    raw_accum* acc = raw_find_accum(rl, name.start, name.len);
    if (acc) {
      if (mode != v6_raw_ctx_body)
        return 0;
      advance(p);
      if (emit)
        emit_dload(c->m, acc->shadow_slot);
      return 1;
    }
    uint16_t slot;
    if (!find_slot(c, name.start, name.len, &slot))
      return 0;
    advance(p);
    if (emit) {
      var_ref vr;
      vr.kind = var_local;
      vr.index = slot;
      emit_var_read_ref(c, vr);
      emit_to_number(c);
    }
    return 1;
  }
  return 0;
}

static int compile_raw_unary(parser* p, compiler* c, raw_loop_ctx* rl, int emit,
                             int mode) {
  if (check(p, tok_minus)) {
    advance(p);
    if (!compile_raw_unary(p, c, rl, emit, mode))
      return 0;
    if (emit)
      op_emit(c->m, op_dneg);
    return 1;
  }
  return compile_raw_primary(p, c, rl, emit, mode);
}

static int compile_raw_mul(parser* p, compiler* c, raw_loop_ctx* rl, int emit,
                           int mode) {
  if (!compile_raw_unary(p, c, rl, emit, mode))
    return 0;
  for (;;) {
    uint8_t bop;
    if (check(p, tok_star))
      bop = op_dmul;
    else if (check(p, tok_slash))
      bop = op_ddiv;
    else if (check(p, tok_percent))
      bop = op_drem;
    else
      break;
    advance(p);
    if (!compile_raw_unary(p, c, rl, emit, mode))
      return 0;
    if (emit)
      op_emit(c->m, bop);
  }
  return 1;
}

static int compile_raw_expr(parser* p, compiler* c, raw_loop_ctx* rl, int emit,
                            int mode) {
  if (!compile_raw_mul(p, c, rl, emit, mode))
    return 0;
  for (;;) {
    uint8_t bop;
    if (check(p, tok_plus))
      bop = op_dadd;
    else if (check(p, tok_minus))
      bop = op_dsub;
    else
      break;
    advance(p);
    if (!compile_raw_mul(p, c, rl, emit, mode))
      return 0;
    if (emit)
      op_emit(c->m, bop);
  }
  return 1;
}

static int compile_raw_push_stmt(parser* p, compiler* c, raw_loop_ctx* rl,
                                 int emit, uint16_t arr_slot) {
  var_ref arr_vr;
  arr_vr.kind = var_local;
  arr_vr.index = arr_slot;

  uint16_t arrval_slot = 0;
  if (emit) {
    emit_var_read_ref(c, arr_vr);
    arrval_slot = c->next_local_slot++;
    emit_astore(c->m, arrval_slot);
  }

  if (!compile_raw_expr(p, c, rl, emit, v6_raw_ctx_body))
    return 0;

  uint16_t argval_slot = 0;
  if (emit) {
    emit_box_tag(c, op_iconst_0);
    argval_slot = c->next_local_slot++;
    emit_astore(c->m, argval_slot);
  }

  if (!check(p, tok_rparen))
    return 0;
  advance(p);
  if (!check(p, tok_semi))
    return 0;
  advance(p);

  if (emit) {
    uint16_t arr_cls = cf_class(c->cf, "V6Array");
    uint16_t ref_idx =
        cf_methodref(c->cf, "V6Value", "ref", "()Ljava/lang/Object;");
    emit_aload(c->m, arrval_slot);
    op_emit2(c->m, op_invokevirtual, ref_idx);
    op_emit2(c->m, op_instanceof, arr_cls);
    size_t slow_jump = op_pos(c->m);
    op_emit2(c->m, op_ifeq, 0);

    emit_aload(c->m, arrval_slot);
    op_emit2(c->m, op_invokevirtual, ref_idx);
    op_emit2(c->m, op_checkcast, arr_cls);
    emit_aload(c->m, argval_slot);
    uint16_t push_idx = cf_methodref(c->cf, "V6Object", "push", "(LV6Value;)V");
    op_emit2(c->m, op_invokevirtual, push_idx);
    size_t end_jump = op_pos(c->m);
    op_emit2(c->m, op_goto, 0);

    size_t slow_pos = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(slow_jump + 1),
              (uint16_t)(slow_pos - slow_jump));

    uint16_t push_str = cf_string(c->cf, "push");
    uint16_t getprop_idx = cf_methodref(c->cf, "V6Value", "getProp",
                                        "(Ljava/lang/String;)LV6Value;");
    uint16_t call_idx = cf_methodref(c->cf, "V6Value", "call",
                                     "(LV6Value;[LV6Value;)LV6Value;");
    emit_aload(c->m, arrval_slot);
    op_emit2(c->m, op_ldc_w, push_str);
    op_emit2(c->m, op_invokevirtual, getprop_idx);
    emit_aload(c->m, arrval_slot);
    emit_iconst(c->m, 1);
    op_emit2(c->m, op_anewarray, value_class(c->cf));
    op_emit(c->m, op_dup);
    emit_iconst(c->m, 0);
    emit_aload(c->m, argval_slot);
    op_emit(c->m, op_aastore);
    op_emit2(c->m, op_invokevirtual, call_idx);
    op_emit(c->m, op_pop);

    size_t end_pos = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(end_jump + 1), (uint16_t)(end_pos - end_jump));
  }

  return 1;
}

static int compile_raw_stmt(parser* p, compiler* c, raw_loop_ctx* rl,
                            int emit) {
  if (!check(p, tok_ident))
    return 0;
  tok name = p->cur;
  if (raw_ident_is_counter(rl, name))
    return 0;

  raw_accum* acc = raw_find_accum(rl, name.start, name.len);
  if (acc) {
    advance(p);

    uint8_t bop = 0;
    int is_plain = 0;
    if (check(p, tok_plus_eq))
      bop = op_dadd;
    else if (check(p, tok_minus_eq))
      bop = op_dsub;
    else if (check(p, tok_star_eq))
      bop = op_dmul;
    else if (check(p, tok_assign))
      is_plain = 1;
    else
      return 0;
    advance(p);

    if (emit && !is_plain)
      emit_dload(c->m, acc->shadow_slot);

    if (!compile_raw_expr(p, c, rl, emit, v6_raw_ctx_body))
      return 0;

    if (emit) {
      if (!is_plain)
        op_emit(c->m, bop);
      emit_dstore(c->m, acc->shadow_slot);
    }

    if (!check(p, tok_semi))
      return 0;
    advance(p);
    return 1;
  }

  uint16_t arr_slot;
  if (!find_slot(c, name.start, name.len, &arr_slot))
    return 0;
  advance(p);
  if (!check(p, tok_dot))
    return 0;
  advance(p);
  if (!check(p, tok_ident) || p->cur.len != 4 ||
      memcmp(p->cur.start, "push", 4) != 0)
    return 0;
  advance(p);
  if (!check(p, tok_lparen))
    return 0;
  advance(p);

  return compile_raw_push_stmt(p, c, rl, emit, arr_slot);
}

static int compile_raw_for_rest(parser* p, compiler* c, raw_loop_ctx* rl,
                                int emit) {
  if (!compile_raw_expr(p, c, rl, emit, v6_raw_ctx_init))
    return 0;
  if (emit)
    emit_dstore(c->m, rl->counter_slot);

  if (!check(p, tok_semi))
    return 0;
  advance(p);

  if (!check(p, tok_ident) || !raw_ident_is_counter(rl, p->cur))
    return 0;
  advance(p);

  uint8_t cmp_op;
  if (check(p, tok_lt))
    cmp_op = op_ifge;
  else if (check(p, tok_le))
    cmp_op = op_ifgt;
  else if (check(p, tok_gt))
    cmp_op = op_ifle;
  else if (check(p, tok_ge))
    cmp_op = op_iflt;
  else
    return 0;
  advance(p);

  size_t cond_pos = 0;
  if (emit) {
    cond_pos = op_pos(c->m);
    emit_dload(c->m, rl->counter_slot);
  }
  if (!compile_raw_expr(p, c, rl, emit, v6_raw_ctx_header))
    return 0;

  size_t exit_jump = 0;
  if (emit) {
    op_emit(c->m, op_dcmpg);
    exit_jump = op_pos(c->m);
    op_emit2(c->m, cmp_op, 0);
  }

  if (!check(p, tok_semi))
    return 0;
  advance(p);

  if (!check(p, tok_ident) || !raw_ident_is_counter(rl, p->cur))
    return 0;
  advance(p);

  int is_dec = 0;
  int has_step_expr = 0;
  if (check(p, tok_plus_plus)) {
    advance(p);
  } else if (check(p, tok_minus_minus)) {
    is_dec = 1;
    advance(p);
  } else if (check(p, tok_plus_eq)) {
    has_step_expr = 1;
    advance(p);
  } else if (check(p, tok_minus_eq)) {
    has_step_expr = 1;
    is_dec = 1;
    advance(p);
  } else {
    return 0;
  }

  size_t body_jump = 0, inc_pos = 0;
  if (emit) {
    body_jump = op_pos(c->m);
    op_emit2(c->m, op_goto, 0);
    inc_pos = op_pos(c->m);
    emit_dload(c->m, rl->counter_slot);
  }
  if (has_step_expr) {
    if (!compile_raw_expr(p, c, rl, emit, v6_raw_ctx_header))
      return 0;
  } else if (emit) {
    emit_dconst_val(c->cf, c->m, 1.0);
  }
  if (emit) {
    op_emit(c->m, is_dec ? op_dsub : op_dadd);
    emit_dstore(c->m, rl->counter_slot);
    size_t inc_to_cond = op_pos(c->m);
    op_emit2(c->m, op_goto, 0);
    op_patch2(c->m, (uint16_t)(inc_to_cond + 1),
              (uint16_t)(cond_pos - inc_to_cond));
  }

  if (!check(p, tok_rparen))
    return 0;
  advance(p);

  if (!check(p, tok_lbrace))
    return 0;
  advance(p);

  size_t body_pos = 0;
  if (emit) {
    body_pos = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(body_jump + 1),
              (uint16_t)(body_pos - body_jump));
  }

  while (!check(p, tok_rbrace)) {
    if (check(p, tok_eof))
      return 0;
    if (!compile_raw_stmt(p, c, rl, emit))
      return 0;
  }
  advance(p);

  if (emit) {
    size_t body_to_inc = op_pos(c->m);
    op_emit2(c->m, op_goto, 0);
    op_patch2(c->m, (uint16_t)(body_to_inc + 1),
              (uint16_t)(inc_pos - body_to_inc));
    size_t end_pos = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(exit_jump + 1), (uint16_t)(end_pos - exit_jump));
  }

  return 1;
}

static int try_compile_raw_for(parser* p, compiler* c) {
  size_t checkpoint = c->m->code.len;
  lexer saved_lex = p->lex;
  tok saved_cur = p->cur;
  tok saved_prev = p->prev;

  const char* body_start = raw_find_body_start(p);
  if (!body_start)
    return 0;

  if (!check(p, tok_kw_let))
    goto bail;
  advance(p);
  if (!check(p, tok_ident))
    goto bail;
  tok counter_name = p->cur;
  advance(p);
  if (!check(p, tok_assign))
    goto bail;
  advance(p);

  raw_loop_ctx rl;
  rl.counter_name = counter_name.start;
  rl.counter_len = counter_name.len;
  rl.accum_count = 0;

  if (!raw_scan_body_accums(body_start, &rl))
    goto bail;

  lexer after_assign_lex = p->lex;
  tok after_assign_cur = p->cur;
  tok after_assign_prev = p->prev;

  if (!compile_raw_for_rest(p, c, &rl, 0))
    goto bail;

  for (int i = 0; i < rl.accum_count; i++) {
    raw_accum* acc = &rl.accums[i];
    uint16_t slot;
    if (!find_slot(c, acc->name, acc->len, &slot))
      goto bail;
    local* le = find_local_entry(c, acc->name, acc->len);
    if (le && le->is_const)
      goto bail;
    acc->real_slot = slot;
    acc->shadow_slot = c->next_local_slot;
    c->next_local_slot += 2;
  }
  rl.counter_slot = c->next_local_slot;
  c->next_local_slot += 2;

  p->lex = after_assign_lex;
  p->cur = after_assign_cur;
  p->prev = after_assign_prev;

  for (int i = 0; i < rl.accum_count; i++) {
    raw_accum* acc = &rl.accums[i];
    var_ref vr;
    vr.kind = var_local;
    vr.index = acc->real_slot;
    emit_var_read_ref(c, vr);
    emit_to_number(c);
    emit_dstore(c->m, acc->shadow_slot);
  }

  if (!compile_raw_for_rest(p, c, &rl, 1)) {
    c->m->code.len = checkpoint;
    goto bail;
  }

  for (int i = 0; i < rl.accum_count; i++) {
    raw_accum* acc = &rl.accums[i];
    emit_dload(c->m, acc->shadow_slot);
    emit_box_tag(c, op_iconst_0);
    var_ref vr;
    vr.kind = var_local;
    vr.index = acc->real_slot;
    emit_var_write_ref(c, vr);
    op_emit(c->m, op_pop);
  }

  return 1;

bail:
  c->m->code.len = checkpoint;
  p->lex = saved_lex;
  p->cur = saved_cur;
  p->prev = saved_prev;
  return 0;
}

static void parse_for(parser* p, compiler* c) {
  int is_await = match(p, tok_kw_await);
  expect(p, tok_lparen);

  if (!is_await && try_compile_raw_for(p, c)) {
    return;
  }

  int saved_count = c->local_count;

  if (check(p, tok_kw_var) || check(p, tok_kw_let) || check(p, tok_kw_const)) {
    lexer save_lex = p->lex;
    tok save_cur = p->cur;
    tok save_prev = p->prev;
    advance(p);
    tok_kind kind = p->prev.kind;
    if (check(p, tok_lbracket) || check(p, tok_lbrace)) {
      int is_array = check(p, tok_lbracket);
      tok_kind open = is_array ? tok_lbracket : tok_lbrace;
      tok_kind close = is_array ? tok_rbracket : tok_rbrace;
      advance(p);
      const char* pattern_start = p->cur.start;
      skip_balanced(p, open, close);
      if (match(p, tok_kw_of)) {
        parse_for_of_pattern(p, c, kind, is_array, pattern_start);
        for (int i = saved_count; i < c->local_count; i++) {
          if (!c->locals[i].is_var)
            c->locals[i].dead = 1;
        }
        return;
      }
    }
    p->lex = save_lex;
    p->cur = save_cur;
    p->prev = save_prev;
  }

  if (match(p, tok_kw_var) || match(p, tok_kw_let) || match(p, tok_kw_const)) {
    tok_kind kind = p->prev.kind;
    if (!expect(p, tok_ident))
      return;
    tok name = p->prev;
    if (match(p, tok_kw_in) || match(p, tok_kw_of)) {
      int is_of = p->prev.kind == tok_kw_of;
      if (is_of && is_await)
        parse_for_await_of(p, c, kind, name);
      else if (is_of)
        parse_for_of(p, c, kind, name);
      else
        parse_for_in(p, c, kind, name);
      for (int i = saved_count; i < c->local_count; i++) {
        if (!c->locals[i].is_var)
          c->locals[i].dead = 1;
      }
      return;
    }
    parse_one_declarator_named(p, c, kind, name);
    while (match(p, tok_comma))
      parse_one_declarator(p, c, kind);
  } else if (!check(p, tok_semi)) {
    parse_expr(p, c);
    op_emit(c->m, op_pop);
  }
  expect(p, tok_semi);

  size_t cond_pos = op_pos(c->m);
  int has_cond = !check(p, tok_semi);
  size_t exit_jump = 0;
  if (has_cond) {
    parse_expr(p, c);
    emit_truthy(c);
    exit_jump = op_pos(c->m);
    op_emit2(c->m, op_ifeq, 0);
  }
  expect(p, tok_semi);

  size_t body_jump = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);

  size_t inc_pos = op_pos(c->m);
  if (!check(p, tok_rparen)) {
    parse_expr(p, c);
    op_emit(c->m, op_pop);
  }
  size_t inc_to_cond = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);
  op_patch2(c->m, (uint16_t)(inc_to_cond + 1),
            (uint16_t)(cond_pos - inc_to_cond));

  expect(p, tok_rparen);

  size_t body_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(body_jump + 1), (uint16_t)(body_pos - body_jump));

  push_loop(c, inc_pos);
  parse_stmt(p, c);

  size_t body_to_inc = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);
  op_patch2(c->m, (uint16_t)(body_to_inc + 1),
            (uint16_t)(inc_pos - body_to_inc));

  size_t end_pos = op_pos(c->m);
  if (has_cond)
    op_patch2(c->m, (uint16_t)(exit_jump + 1), (uint16_t)(end_pos - exit_jump));
  pop_loop(c, end_pos);

  for (int i = saved_count; i < c->local_count; i++) {
    if (!c->locals[i].is_var)
      c->locals[i].dead = 1;
  }
}

static void parse_switch(parser* p, compiler* c) {
  expect(p, tok_lparen);
  parse_expr(p, c);
  expect(p, tok_rparen);
  expect(p, tok_lbrace);

  emit_astore(c->m, c->scratch_slot);

  c->breaks[c->break_depth].count = 0;
  c->break_depth++;

  size_t prev_case_jump = 0;
  int have_prev_case_jump = 0;
  int default_seen = 0;

  while (!check(p, tok_rbrace) && !check(p, tok_eof)) {
    if (match(p, tok_kw_case)) {
      emit_aload(c->m, c->scratch_slot);
      parse_expr(p, c);
      uint16_t idx = cf_methodref(c->cf, "V6Value", "strictEquals",
                                  "(LV6Value;LV6Value;)Z");
      op_emit2(c->m, op_invokestatic, idx);
      size_t skip_jump = op_pos(c->m);
      op_emit2(c->m, op_ifeq, 0);
      expect(p, tok_colon);

      if (have_prev_case_jump) {
        size_t here = op_pos(c->m);
        op_patch2(c->m, (uint16_t)(prev_case_jump + 1),
                  (uint16_t)(here - prev_case_jump));
        have_prev_case_jump = 0;
      }

      while (!check(p, tok_kw_case) && !check(p, tok_kw_default) &&
             !check(p, tok_rbrace) && !check(p, tok_eof))
        parse_stmt(p, c);

      prev_case_jump = op_pos(c->m);
      op_emit2(c->m, op_goto, 0);
      have_prev_case_jump = 1;

      size_t after = op_pos(c->m);
      op_patch2(c->m, (uint16_t)(skip_jump + 1), (uint16_t)(after - skip_jump));
    } else if (match(p, tok_kw_default)) {
      default_seen = 1;
      expect(p, tok_colon);

      if (have_prev_case_jump) {
        size_t here = op_pos(c->m);
        op_patch2(c->m, (uint16_t)(prev_case_jump + 1),
                  (uint16_t)(here - prev_case_jump));
        have_prev_case_jump = 0;
      }

      while (!check(p, tok_kw_case) && !check(p, tok_kw_default) &&
             !check(p, tok_rbrace) && !check(p, tok_eof))
        parse_stmt(p, c);
    } else {
      error_at(p, "expected 'case' or 'default'");
      break;
    }
  }
  (void)default_seen;

  if (have_prev_case_jump) {
    size_t here = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(prev_case_jump + 1),
              (uint16_t)(here - prev_case_jump));
  }

  expect(p, tok_rbrace);

  size_t end_pos = op_pos(c->m);
  patch_breaks(c, end_pos);
  c->break_depth--;
}

static uint16_t v6ref_arr_class(class_file* cf) {
  return cf_class(cf, "V6Ref");
}

static void bind_param(compiler* fc, parser* p, tok name, int idx) {
  uint16_t slot = fc->next_local_slot++;
  emit_aload(fc->m, 2);
  emit_iconst(fc->m, idx);
  uint16_t argat_idx =
      cf_methodref(fc->cf, "V6Value", "argAt", "([LV6Value;I)LV6Value;");
  op_emit2(fc->m, op_invokestatic, argat_idx);

  if (match(p, tok_assign)) {
    op_emit(fc->m, op_dup);
    uint16_t isundef_idx =
        cf_methodref(fc->cf, "V6Value", "isUndefined", "()Z");
    op_emit2(fc->m, op_invokevirtual, isundef_idx);
    size_t has_val_jump = op_pos(fc->m);
    op_emit2(fc->m, op_ifeq, 0);
    op_emit(fc->m, op_pop);
    parse_expr(p, fc);
    size_t end_jump = op_pos(fc->m);
    op_emit2(fc->m, op_goto, 0);
    size_t has_val_pos = op_pos(fc->m);
    op_patch2(fc->m, (uint16_t)(has_val_jump + 1),
              (uint16_t)(has_val_pos - has_val_jump));
    size_t end_pos = op_pos(fc->m);
    op_patch2(fc->m, (uint16_t)(end_jump + 1), (uint16_t)(end_pos - end_jump));
  }

  emit_var_declare(fc, slot);
  fc->params[fc->param_count].name = name.start;
  fc->params[fc->param_count].len = name.len;
  fc->params[fc->param_count].slot = slot;
  fc->param_count++;
}

static void bind_rest_param(compiler* fc, tok name, int idx) {
  uint16_t slot = fc->next_local_slot++;
  emit_aload(fc->m, 2);
  emit_iconst(fc->m, idx);
  uint16_t restargs_idx = cf_methodref(fc->cf, "V6Object", "restFromArgs",
                                       "([LV6Value;I)LV6Array;");
  op_emit2(fc->m, op_invokestatic, restargs_idx);
  emit_box_object_ref(fc);
  emit_var_declare(fc, slot);
  fc->params[fc->param_count].name = name.start;
  fc->params[fc->param_count].len = name.len;
  fc->params[fc->param_count].slot = slot;
  fc->param_count++;
}

static void parse_function_params(parser* p, compiler* fc) {
  expect(p, tok_lparen);
  int idx = 0;
  if (!check(p, tok_rparen)) {
    for (;;) {
      if (match(p, tok_ellipsis)) {
        if (!expect(p, tok_ident))
          break;
        bind_rest_param(fc, p->prev, idx);
        break;
      }
      if (check(p, tok_lbracket) || check(p, tok_lbrace)) {
        int is_array = check(p, tok_lbracket);
        tok_kind open = is_array ? tok_lbracket : tok_lbrace;
        tok_kind close = is_array ? tok_rbracket : tok_rbrace;
        advance(p);
        const char* pattern_start = p->cur.start;
        skip_balanced(p, open, close);

        emit_aload(fc->m, 2);
        emit_iconst(fc->m, idx);
        uint16_t argat_idx =
            cf_methodref(fc->cf, "V6Value", "argAt", "([LV6Value;I)LV6Value;");
        op_emit2(fc->m, op_invokestatic, argat_idx);

        if (match(p, tok_assign)) {
          op_emit(fc->m, op_dup);
          uint16_t isundef_idx =
              cf_methodref(fc->cf, "V6Value", "isUndefined", "()Z");
          op_emit2(fc->m, op_invokevirtual, isundef_idx);
          size_t has_val_jump = op_pos(fc->m);
          op_emit2(fc->m, op_ifeq, 0);
          op_emit(fc->m, op_pop);
          parse_expr(p, fc);
          size_t end_jump = op_pos(fc->m);
          op_emit2(fc->m, op_goto, 0);
          size_t has_val_pos = op_pos(fc->m);
          op_patch2(fc->m, (uint16_t)(has_val_jump + 1),
                    (uint16_t)(has_val_pos - has_val_jump));
          size_t end_pos = op_pos(fc->m);
          op_patch2(fc->m, (uint16_t)(end_jump + 1),
                    (uint16_t)(end_pos - end_jump));
        }

        uint16_t src_slot = fc->next_local_slot++;
        emit_astore(fc->m, src_slot);

        parser pp;
        parser_init(&pp, pattern_start);
        if (is_array)
          parse_array_pattern(&pp, fc, tok_kw_let, src_slot);
        else
          parse_object_pattern(&pp, fc, tok_kw_let, src_slot);

        idx++;
        if (!match(p, tok_comma))
          break;
        continue;
      }

      if (!expect(p, tok_ident))
        break;
      tok pname = p->prev;
      bind_param(fc, p, pname, idx);
      idx++;
      if (!match(p, tok_comma))
        break;
    }
  }
  expect(p, tok_rparen);
}

static void bind_this(compiler* fc) {
  uint16_t slot = fc->next_local_slot++;
  emit_aload(fc->m, 1);
  emit_var_declare(fc, slot);
  tok tt;
  tt.kind = tok_kw_this;
  tt.start = "this";
  tt.len = 4;
  tt.line = 0;
  tt.num = 0;
  add_local(fc, tt, slot, 0, 0);
}

static int compile_body_has_closures(parser* p, int parens_params,
                                     int is_arrow) {
  lexer save_lex = p->lex;
  tok save_cur = p->cur;
  tok save_prev = p->prev;

  tok t = p->cur;
  if (parens_params) {
    if (t.kind == tok_lparen) {
      int pdepth = 1;
      t = lex_next(&p->lex);
      while (t.kind != tok_eof && pdepth > 0) {
        if (t.kind == tok_lparen)
          pdepth++;
        else if (t.kind == tok_rparen)
          pdepth--;
        if (pdepth > 0)
          t = lex_next(&p->lex);
      }
      t = lex_next(&p->lex);
    }
  } else {
    t = lex_next(&p->lex);
  }

  if (is_arrow && t.kind == tok_arrow)
    t = lex_next(&p->lex);

  int found = 0;
  if (t.kind == tok_lbrace) {
    int depth = 0;
    for (;;) {
      if (t.kind == tok_lbrace) {
        depth++;
      } else if (t.kind == tok_rbrace) {
        depth--;
        if (depth == 0)
          break;
      } else if (t.kind == tok_kw_function || t.kind == tok_arrow ||
                 t.kind == tok_kw_class) {
        found = 1;
        break;
      } else if (t.kind == tok_eof) {
        break;
      }
      t = lex_next(&p->lex);
    }
  } else {
    int depth = 0;
    for (;;) {
      if (t.kind == tok_lparen || t.kind == tok_lbracket ||
          t.kind == tok_lbrace) {
        depth++;
      } else if (t.kind == tok_rparen || t.kind == tok_rbracket ||
                 t.kind == tok_rbrace) {
        if (depth == 0)
          break;
        depth--;
      } else if (depth == 0 && (t.kind == tok_comma || t.kind == tok_semi)) {
        break;
      } else if (t.kind == tok_kw_function || t.kind == tok_arrow ||
                 t.kind == tok_kw_class) {
        found = 1;
        break;
      } else if (t.kind == tok_eof) {
        break;
      }
      t = lex_next(&p->lex);
    }
  }

  p->lex = save_lex;
  p->cur = save_cur;
  p->prev = save_prev;
  return found;
}

static void compile_closure_value(parser* p, compiler* c, int is_arrow,
                                  int parens_params, char* out_lambda_name) {
  int id = (*c->lambda_counter)++;
  char mname[24];
  snprintf(mname, sizeof(mname), "lambda%d", id);
  if (out_lambda_name)
    strcpy(out_lambda_name, mname);

  method* m = cf_method(c->cf, acc_static, mname,
                        "([LV6Ref;LV6Value;[LV6Value;)LV6Value;");
  m->max_stack = 64;

  compiler fc;
  fc.cf = c->cf;
  fc.m = m;
  fc.parent = c;
  fc.lambda_counter = c->lambda_counter;
  fc.is_arrow = is_arrow;
  fc.param_count = 0;
  fc.local_count = 0;
  fc.scratch_slot = 3;
  fc.next_local_slot = 5;
  fc.upvalue_count = 0;
  fc.break_depth = 0;
  fc.continue_depth = 0;
  fc.catch_depth = 0;
  fc.brace_depth = -1;
  fc.super_name = c->super_name;
  fc.super_len = c->super_len;
  fc.class_name = c->class_name;
  fc.class_name_len = c->class_name_len;
  fc.pending_field_count = 0;
  fc.box_locals = compile_body_has_closures(p, parens_params, is_arrow);
  fc.label_count = 0;
  fc.pending_label_count = 0;
  fc.finally_depth = 0;
  fc.is_async_gen = c->pending_async_gen;
  fc.pending_async_gen = 0;
  c->pending_async_gen = 0;
  fc.is_module = c->is_module;
  fc.this_class_name = c->this_class_name;
  fc.modctx = c->modctx;
  fc.module_dir = c->module_dir;

  if (parens_params) {
    parse_function_params(p, &fc);
  } else if (expect(p, tok_ident)) {
    bind_param(&fc, p, p->prev, 0);
  }

  if (!is_arrow)
    bind_this(&fc);

  if (!is_arrow && c->pending_field_count > 0) {
    var_ref this_vr = resolve_var(&fc, "this", 4);
    for (int i = 0; i < c->pending_field_count; i++) {
      field_init* fi = &c->pending_fields[i];
      emit_var_read_ref(&fc, this_vr);
      char* keystr = malloc(fi->name_len + 1);
      memcpy(keystr, fi->name, fi->name_len);
      keystr[fi->name_len] = '\0';
      uint16_t key_idx = cf_string(fc.cf, keystr);
      free(keystr);
      op_emit2(fc.m, op_ldc_w, key_idx);
      if (fi->init_src) {
        parser fieldp;
        parser_init(&fieldp, fi->init_src);
        parse_expr(&fieldp, &fc);
      } else {
        emit_undef(fc.cf, fc.m);
      }
      uint16_t setprop_idx = cf_methodref(fc.cf, "V6Value", "setProp",
                                          "(Ljava/lang/String;LV6Value;)V");
      op_emit2(fc.m, op_invokevirtual, setprop_idx);
    }
  }

  if (is_arrow && !expect(p, tok_arrow))
    return;

  if (check(p, tok_lbrace)) {
    advance(p);
    prescan_decls(&fc, p->cur.start, 1);
    parse_block(p, &fc);
    emit_undef(fc.cf, fc.m);
    op_emit(fc.m, op_areturn);
  } else {
    parse_expr(p, &fc);
    op_emit(fc.m, op_areturn);
  }

  fc.m->max_locals = fc.next_local_slot;

  uint16_t closure_cls = cf_class(c->cf, "V6Closure");
  uint16_t closure_ctor =
      cf_methodref(c->cf, "V6Closure", "<init>",
                   "(Ljava/lang/Class;Ljava/lang/String;[LV6Ref;)V");
  uint16_t main_cls_idx = cf_class(c->cf, c->this_class_name);
  uint16_t name_str = cf_string(c->cf, mname);

  op_emit2(c->m, op_new, closure_cls);
  op_emit(c->m, op_dup);
  op_emit2(c->m, op_ldc_w, main_cls_idx);
  op_emit2(c->m, op_ldc_w, name_str);

  emit_iconst(c->m, fc.upvalue_count);
  op_emit2(c->m, op_anewarray, v6ref_arr_class(c->cf));
  for (int i = 0; i < fc.upvalue_count; i++) {
    op_emit(c->m, op_dup);
    emit_iconst(c->m, i);
    emit_ref_push(c, !fc.upvalues[i].from_parent_local,
                  fc.upvalues[i].parent_index);
    op_emit(c->m, op_aastore);
  }
  op_emit2(c->m, op_invokespecial, closure_ctor);
  emit_box_ref_computed(c, V6_TAG_FUNC);
}

static void skip_function_tokens(parser* p) {
  match(p, tok_star);
  if (!expect(p, tok_ident))
    return;
  if (!expect(p, tok_lparen))
    return;
  int depth = 1;
  while (depth > 0 && !check(p, tok_eof)) {
    if (check(p, tok_lparen))
      depth++;
    else if (check(p, tok_rparen))
      depth--;
    advance(p);
  }
  if (!expect(p, tok_lbrace))
    return;
  depth = 1;
  while (depth > 0 && !check(p, tok_eof)) {
    if (check(p, tok_lbrace))
      depth++;
    else if (check(p, tok_rbrace))
      depth--;
    advance(p);
  }
}

static void parse_function_decl(parser* p, compiler* c) {
  if (c->brace_depth == 0) {
    skip_function_tokens(p);
    return;
  }

  int is_gen = match(p, tok_star);
  if (!expect(p, tok_ident))
    return;
  tok name = p->prev;
  uint16_t slot = c->next_local_slot++;
  emit_undef(c->cf, c->m);
  emit_var_declare(c, slot);
  add_local(c, name, slot, 0, 0);
  compile_closure_value(p, c, 0, 1, NULL);
  if (is_gen)
    emit_wrap_generator(c);
  var_ref vr = resolve_var(c, name.start, name.len);
  emit_var_write_ref(c, vr);
  op_emit(c->m, op_pop);
}

static void parse_class_decl(parser* p, compiler* c) {
  if (!expect(p, tok_ident))
    return;
  tok name = p->prev;

  tok base_name;
  int has_base = 0;
  if (match(p, tok_kw_extends)) {
    if (!expect(p, tok_ident))
      return;
    base_name = p->prev;
    has_base = 1;
  }

  if (!expect(p, tok_lbrace))
    return;

  uint16_t cls_tmp = c->next_local_slot++;
  uint16_t proto_tmp = c->next_local_slot++;

  uint16_t cls_cls = cf_class(c->cf, "V6Class");
  uint16_t cls_ctor_idx = cf_methodref(c->cf, "V6Class", "<init>", "()V");
  op_emit2(c->m, op_new, cls_cls);
  op_emit(c->m, op_dup);
  op_emit2(c->m, op_invokespecial, cls_ctor_idx);
  emit_astore(c->m, cls_tmp);

  uint16_t obj_ctor_idx = cf_methodref(c->cf, "V6Object", "<init>", "()V");
  op_emit2(c->m, op_new, object_class(c->cf));
  op_emit(c->m, op_dup);
  op_emit2(c->m, op_invokespecial, obj_ctor_idx);
  emit_astore(c->m, proto_tmp);

  const char* saved_super_name = c->super_name;
  size_t saved_super_len = c->super_len;
  const char* saved_class_name = c->class_name;
  size_t saved_class_name_len = c->class_name_len;
  c->class_name = name.start;
  c->class_name_len = name.len;
  char* ctor_lambda_name = NULL;

  var_ref base_vr;
  base_vr.kind = var_not_found;
  base_vr.index = 0;
  if (has_base) {
    base_vr = resolve_var(c, base_name.start, base_name.len);
    if (base_vr.kind == var_not_found) {
      error_at(p, "undeclared variable");
      return;
    }
    c->super_name = base_name.start;
    c->super_len = base_name.len;

    uint16_t setproto_idx =
        cf_methodref(c->cf, "V6Object", "setProtoFromValue", "(LV6Value;)V");
    uint16_t proto_str = cf_string(c->cf, "prototype");
    uint16_t getprop_idx = cf_methodref(c->cf, "V6Value", "getProp",
                                        "(Ljava/lang/String;)LV6Value;");
    emit_aload(c->m, proto_tmp);
    emit_var_read_ref(c, base_vr);
    op_emit2(c->m, op_ldc_w, proto_str);
    op_emit2(c->m, op_invokevirtual, getprop_idx);
    op_emit2(c->m, op_invokevirtual, setproto_idx);
  }

  uint16_t set_idx =
      cf_methodref(c->cf, "V6Object", "set", "(Ljava/lang/String;LV6Value;)V");
  uint16_t proto_prop_str = cf_string(c->cf, "prototype");
  emit_aload(c->m, cls_tmp);
  op_emit2(c->m, op_ldc_w, proto_prop_str);
  emit_aload(c->m, proto_tmp);
  emit_box_object_ref(c);
  op_emit2(c->m, op_invokevirtual, set_idx);

  field_init pending_instance_fields[v6_max_fields];
  int pending_instance_field_count = 0;

  while (!check(p, tok_rbrace) && !check(p, tok_eof)) {
    int is_static = match(p, tok_kw_static);

    int is_async = 0;
    if (check(p, tok_kw_async)) {
      lexer save_lex = p->lex;
      tok save_cur = p->cur;
      tok save_prev = p->prev;
      advance(p);
      if (check(p, tok_lparen)) {
        p->lex = save_lex;
        p->cur = save_cur;
        p->prev = save_prev;
      } else {
        is_async = 1;
      }
    }

    int is_gen = match(p, tok_star);

    int is_getter = 0, is_setter = 0;
    if (!is_async && (check(p, tok_kw_get) || check(p, tok_kw_set))) {
      tok_kind modifier = p->cur.kind;
      lexer save_lex = p->lex;
      tok save_cur = p->cur;
      tok save_prev = p->prev;
      advance(p);
      if (check(p, tok_lparen)) {
        p->lex = save_lex;
        p->cur = save_cur;
        p->prev = save_prev;
      } else {
        is_getter = modifier == tok_kw_get;
        is_setter = modifier == tok_kw_set;
      }
    }

    int computed = 0;
    uint16_t computed_key_slot = 0;
    tok member_name;
    member_name.start = NULL;
    member_name.len = 0;
    if (match(p, tok_lbracket)) {
      computed = 1;
      parse_expr(p, c);
      uint16_t tostring_idx =
          cf_methodref(c->cf, "V6Value", "toString", "()Ljava/lang/String;");
      op_emit2(c->m, op_invokevirtual, tostring_idx);
      if (!expect(p, tok_rbracket))
        return;
      computed_key_slot = c->next_local_slot++;
      emit_astore(c->m, computed_key_slot);
    } else if (!match_property_name(p)) {
      break;
    } else {
      member_name = p->prev;
    }
    int is_ctor = !computed && !is_static && !is_getter && !is_setter &&
                  member_name.len == 11 &&
                  memcmp(member_name.start, "constructor", 11) == 0;

    if (is_getter || is_setter) {
      uint16_t target_slot = is_static ? cls_tmp : proto_tmp;
      emit_aload(c->m, target_slot);
      if (computed) {
        emit_aload(c->m, computed_key_slot);
      } else {
        char* mkey = dup_tok(member_name);
        uint16_t mkey_idx = cf_string(c->cf, mkey);
        free(mkey);
        op_emit2(c->m, op_ldc_w, mkey_idx);
      }
      compile_closure_value(p, c, 0, 1, NULL);
      uint16_t ascall_idx =
          cf_methodref(c->cf, "V6Value", "asCallable", "()LV6Callable;");
      op_emit2(c->m, op_invokevirtual, ascall_idx);
      uint16_t def_idx = cf_methodref(
          c->cf, "V6Object", is_getter ? "defineGetter" : "defineSetter",
          "(Ljava/lang/String;LV6Callable;)V");
      op_emit2(c->m, op_invokevirtual, def_idx);
    } else if (is_ctor) {
      emit_aload(c->m, cls_tmp);
      ctor_lambda_name = malloc(24);
      for (int i = 0; i < pending_instance_field_count; i++)
        c->pending_fields[i] = pending_instance_fields[i];
      c->pending_field_count = pending_instance_field_count;
      compile_closure_value(p, c, 0, 1, ctor_lambda_name);
      c->pending_field_count = 0;
      uint16_t ascall_idx =
          cf_methodref(c->cf, "V6Value", "asCallable", "()LV6Callable;");
      op_emit2(c->m, op_invokevirtual, ascall_idx);
      uint16_t ctor_field =
          cf_fieldref(c->cf, "V6Class", "ctor", "LV6Callable;");
      op_emit2(c->m, op_putfield, ctor_field);
    } else if (check(p, tok_lparen)) {
      uint16_t target_slot = is_static ? cls_tmp : proto_tmp;
      emit_aload(c->m, target_slot);
      if (computed) {
        emit_aload(c->m, computed_key_slot);
      } else {
        char* mkey = dup_tok(member_name);
        uint16_t mkey_idx = cf_string(c->cf, mkey);
        free(mkey);
        op_emit2(c->m, op_ldc_w, mkey_idx);
      }
      compile_closure_value(p, c, 0, 1, NULL);
      if (is_gen && is_async) {
        error_at(p, "async generator methods are not supported");
        return;
      } else if (is_gen) {
        emit_wrap_generator(c);
      } else if (is_async) {
        emit_wrap_async(c);
      }
      op_emit2(c->m, op_invokevirtual, set_idx);
    } else if (computed) {
      error_at(p, "computed field names are not supported");
      return;
    } else {
      const char* init_src = NULL;
      if (match(p, tok_assign)) {
        init_src = p->cur.start;
        skip_field_init(p);
      }
      match(p, tok_semi);
      if (is_static) {
        emit_aload(c->m, cls_tmp);
        char* mkey = dup_tok(member_name);
        uint16_t mkey_idx = cf_string(c->cf, mkey);
        free(mkey);
        op_emit2(c->m, op_ldc_w, mkey_idx);
        if (init_src) {
          parser fieldp;
          parser_init(&fieldp, init_src);
          parse_expr(&fieldp, c);
        } else {
          emit_undef(c->cf, c->m);
        }
        op_emit2(c->m, op_invokevirtual, set_idx);
      } else if (pending_instance_field_count < v6_max_fields) {
        pending_instance_fields[pending_instance_field_count].name =
            member_name.start;
        pending_instance_fields[pending_instance_field_count].name_len =
            member_name.len;
        pending_instance_fields[pending_instance_field_count].init_src =
            init_src;
        pending_instance_field_count++;
      }
    }
  }
  expect(p, tok_rbrace);

  if ((has_base || pending_instance_field_count > 0) && !ctor_lambda_name) {
    emit_aload(c->m, cls_tmp);
    ctor_lambda_name = malloc(24);
    for (int i = 0; i < pending_instance_field_count; i++)
      c->pending_fields[i] = pending_instance_fields[i];
    c->pending_field_count = pending_instance_field_count;
    parser synth;
    parser_init(&synth, has_base ? "(...args) { super(...args); }" : "() {}");
    compile_closure_value(&synth, c, 0, 1, ctor_lambda_name);
    c->pending_field_count = 0;
    uint16_t ascall_idx =
        cf_methodref(c->cf, "V6Value", "asCallable", "()LV6Callable;");
    op_emit2(c->m, op_invokevirtual, ascall_idx);
    uint16_t ctor_field = cf_fieldref(c->cf, "V6Class", "ctor", "LV6Callable;");
    op_emit2(c->m, op_putfield, ctor_field);
  }

  c->super_name = saved_super_name;
  c->super_len = saved_super_len;
  c->class_name = saved_class_name;
  c->class_name_len = saved_class_name_len;

  emit_aload(c->m, cls_tmp);
  emit_box_object_ref(c);
  if (c->brace_depth == 0) {
    var_ref vr = resolve_var(c, name.start, name.len);
    if (vr.kind == var_not_found) {
      error_at(p, "internal: hoisted class missing");
      return;
    }
    emit_var_write_ref(c, vr);
    op_emit(c->m, op_pop);
  } else {
    uint16_t slot = c->next_local_slot++;
    emit_var_declare(c, slot);
    add_local(c, name, slot, 0, 0);
  }

  if (ctor_lambda_name &&
      !name_reassigned_in_scope(p->lex.src, name.start, name.len)) {
    local* le = find_local_entry(c, name.start, name.len);
    if (le) {
      le->direct_fn = 1;
      le->fn_method_name = ctor_lambda_name;
    }
  }
}

static void parse_try(parser* p, compiler* c) {
  expect(p, tok_lbrace);

  const char* pending_finally_src = NULL;
  {
    lexer save_lex = p->lex;
    tok save_cur = p->cur;
    tok save_prev = p->prev;

    int depth = 1;
    while (depth > 0 && !check(p, tok_eof)) {
      if (check(p, tok_lbrace))
        depth++;
      else if (check(p, tok_rbrace))
        depth--;
      if (depth > 0)
        advance(p);
    }
    if (check(p, tok_rbrace))
      advance(p);

    if (check(p, tok_kw_catch)) {
      advance(p);
      if (match(p, tok_lparen)) {
        while (!check(p, tok_rparen) && !check(p, tok_eof))
          advance(p);
        match(p, tok_rparen);
      }
      if (match(p, tok_lbrace)) {
        int cdepth = 1;
        while (cdepth > 0 && !check(p, tok_eof)) {
          if (check(p, tok_lbrace))
            cdepth++;
          else if (check(p, tok_rbrace))
            cdepth--;
          if (cdepth > 0)
            advance(p);
        }
        if (check(p, tok_rbrace))
          advance(p);
      }
    }

    if (check(p, tok_kw_finally)) {
      advance(p);
      if (match(p, tok_lbrace)) {
        pending_finally_src = p->cur.start;
      }
    }

    p->lex = save_lex;
    p->cur = save_cur;
    p->prev = save_prev;
  }

  int pushed_finally = 0;
  if (pending_finally_src && c->finally_depth < v6_max_pending_finally) {
    c->finally_src[c->finally_depth] = pending_finally_src;
    c->finally_break_depth[c->finally_depth] = c->break_depth;
    c->finally_continue_depth[c->finally_depth] = c->continue_depth;
    c->finally_depth++;
    pushed_finally = 1;
  }

  size_t try_start = op_pos(c->m);
  parse_block(p, c);
  size_t try_end = op_pos(c->m);

  int has_catch = match(p, tok_kw_catch);
  size_t goto_after_try = 0;

  if (has_catch) {
    goto_after_try = op_pos(c->m);
    op_emit2(c->m, op_goto, 0);

    size_t catch_handler_pc = op_pos(c->m);
    uint16_t throw_cls = cf_class(c->cf, "V6Throw");
    method_add_exception(c->m, (uint16_t)try_start, (uint16_t)try_end,
                         (uint16_t)catch_handler_pc, throw_cls);

    int catch_scope_saved = c->local_count;
    int has_binding = 0;
    tok err_name;
    err_name.kind = tok_ident;
    err_name.start = "";
    err_name.len = 0;
    err_name.line = 0;
    err_name.num = 0;

    expect(p, tok_lparen);
    if (check(p, tok_ident)) {
      has_binding = 1;
      advance(p);
      err_name = p->prev;
    }
    expect(p, tok_rparen);

    uint16_t value_field = cf_fieldref(c->cf, "V6Throw", "value", "LV6Value;");
    op_emit2(c->m, op_getfield, value_field);
    if (has_binding) {
      uint16_t err_slot = c->next_local_slot++;
      emit_var_declare(c, err_slot);
      add_local(c, err_name, err_slot, 0, 0);
    } else {
      op_emit(c->m, op_pop);
    }

    expect(p, tok_lbrace);
    parse_block(p, c);

    for (int i = catch_scope_saved; i < c->local_count; i++) {
      if (!c->locals[i].is_var)
        c->locals[i].dead = 1;
    }

    size_t normal_after = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(goto_after_try + 1),
              (uint16_t)(normal_after - goto_after_try));
  }

  if (pushed_finally) {
    c->finally_depth--;
  }

  if (match(p, tok_kw_finally)) {
    size_t guard_start = try_start;
    size_t guard_end = op_pos(c->m);

    expect(p, tok_lbrace);
    const char* finally_body_start = p->cur.start;
    parse_block(p, c);

    size_t skip_guard_jump = op_pos(c->m);
    op_emit2(c->m, op_goto, 0);

    size_t guard_handler_pc = op_pos(c->m);
    uint16_t guard_scratch = c->next_local_slot++;
    emit_astore(c->m, guard_scratch);

    parser fp2;
    parser_init(&fp2, finally_body_start);
    parse_block(&fp2, c);

    emit_aload(c->m, guard_scratch);
    op_emit(c->m, op_athrow);

    size_t after_guard = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(skip_guard_jump + 1),
              (uint16_t)(after_guard - skip_guard_jump));

    method_add_exception(c->m, (uint16_t)guard_start, (uint16_t)guard_end,
                         (uint16_t)guard_handler_pc, 0);
  } else if (!has_catch) {
    error_at(p, "expected 'catch' or 'finally'");
  }
}

static void emit_inline_finally_at(compiler* c, int idx) {
  int saved_depth = c->finally_depth;
  c->finally_depth = idx;
  parser fp;
  parser_init(&fp, c->finally_src[idx]);
  parse_block(&fp, c);
  c->finally_depth = saved_depth;
}

static void emit_all_pending_finally(compiler* c) {
  for (int i = c->finally_depth - 1; i >= 0; i--) {
    emit_inline_finally_at(c, i);
  }
}

static void emit_pending_finally_for_break(compiler* c, int target_break_idx) {
  for (int i = c->finally_depth - 1; i >= 0; i--) {
    if (target_break_idx >= c->finally_break_depth[i])
      break;
    emit_inline_finally_at(c, i);
  }
}

static void emit_pending_finally_for_continue(compiler* c,
                                              int target_continue_idx) {
  for (int i = c->finally_depth - 1; i >= 0; i--) {
    if (target_continue_idx >= c->finally_continue_depth[i])
      break;
    emit_inline_finally_at(c, i);
  }
}

static void parse_labeled_stmt(parser* p, compiler* c, tok label) {
  if (c->break_depth >= v6_max_loops || c->label_count >= v6_max_labels) {
    error_at(p, "too many nested labels/loops");
    return;
  }

  int bidx = c->break_depth;
  c->breaks[bidx].count = 0;
  c->break_depth++;

  int lidx = c->label_count;
  for (int i = 0; i < c->label_count; i++) {
    if (c->label_lens[i] == label.len &&
        memcmp(c->label_names[i], label.start, label.len) == 0) {
      error_at(p, "label already declared in this scope");
      return;
    }
  }
  c->label_names[lidx] = label.start;
  c->label_lens[lidx] = label.len;
  c->label_break_depth[lidx] = bidx;
  c->label_continue_depth[lidx] = -1;
  c->label_count++;

  int save_pending = c->pending_label_count;
  if (c->pending_label_count < v6_max_pending_labels) {
    c->pending_label_names[c->pending_label_count] = label.start;
    c->pending_label_lens[c->pending_label_count] = label.len;
    c->pending_label_count++;
  }

  parse_stmt(p, c);

  c->pending_label_count = save_pending;
  c->label_count = lidx;
  c->break_depth--;

  size_t end_pos = op_pos(c->m);
  break_ctx* bc = &c->breaks[bidx];
  for (size_t i = 0; i < bc->count; i++) {
    op_patch2(c->m, (uint16_t)(bc->jumps[i] + 1),
              (uint16_t)(end_pos - bc->jumps[i]));
  }
}

typedef struct {
  const char* name;
  const char* field;
} node_builtin_ref;

static const node_builtin_ref v6_node_builtin_table[] = {
    {"path", "NODE_PATH"},
    {"util", "NODE_UTIL"},
    {"os", "NODE_OS"},
    {"fs", "NODE_FS"},
    {"events", "NODE_EVENTS"},
    {"assert", "NODE_ASSERT"},
    {"querystring", "NODE_QUERYSTRING"},
    {"perf_hooks", "NODE_PERF_HOOKS"},
    {"dns", "NODE_DNS"},
    {"string_decoder", "NODE_STRING_DECODER"},
    {"url", "NODE_URL"},
    {"zlib", "NODE_ZLIB"},
    {"crypto", "NODE_CRYPTO"},
    {"stream", "NODE_STREAM"},
    {"child_process", "NODE_CHILD_PROCESS"},
    {"net", "NODE_NET"},
    {"http", "NODE_HTTP"},
    {"https", "NODE_HTTPS"},
    {"readline", "NODE_READLINE"},
    {"worker_threads", "NODE_WORKER_THREADS"},
};

static int emit_node_builtin_ref(compiler* c, const char* specifier) {
  if (strncmp(specifier, "node:", 5) == 0)
    specifier += 5;
  size_t n = sizeof(v6_node_builtin_table) / sizeof(v6_node_builtin_table[0]);
  for (size_t i = 0; i < n; i++) {
    if (strcmp(specifier, v6_node_builtin_table[i].name) == 0) {
      uint16_t fidx = cf_fieldref(c->cf, "V6Builtins", v6_node_builtin_table[i].field,
                                  "LV6Value;");
      op_emit2(c->m, op_getstatic, fidx);
      return 1;
    }
  }
  return 0;
}

static compiled_module* get_or_compile_module(module_ctx* modctx,
                                              const char* importer_dir,
                                              const char* specifier, int kind,
                                              parser* p) {
  char abs_path[v6_max_path];
  char err[256];
  if (resolve_module_specifier(importer_dir, specifier, abs_path,
                               sizeof(abs_path), err, sizeof(err)) != 0) {
    error_at(p, err);
    return NULL;
  }
  for (int i = 0; i < modctx->count; i++) {
    if (modctx->modules[i].kind == kind &&
        strcmp(modctx->modules[i].abs_path, abs_path) == 0)
      return &modctx->modules[i];
  }
  if (modctx->count >= v6_max_modules) {
    error_at(p, "too many modules");
    return NULL;
  }
  int idx = modctx->count++;
  compiled_module* mod = &modctx->modules[idx];
  snprintf(mod->abs_path, sizeof(mod->abs_path), "%s", abs_path);
  mod->kind = kind;
  snprintf(mod->class_name, sizeof(mod->class_name), "Mod%d", idx);
  mod->state = 1;
  mod->cf = malloc(sizeof(class_file));
  cf_init(mod->cf, mod->class_name, "java/lang/Object");

  FILE* f = fopen(abs_path, "rb");
  if (!f) {
    error_at(p, "cannot read module file");
    mod->state = 2;
    return mod;
  }
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  char* modsrc = malloc((size_t)n + 1);
  fread(modsrc, 1, (size_t)n, f);
  modsrc[n] = '\0';
  fclose(f);

  char moddir[v6_max_path];
  path_dirname(abs_path, moddir, sizeof(moddir));

  compile_result r = compile_module_impl(mod->cf, mod->class_name, modsrc,
                                         moddir, modctx, 0, kind == 1);
  free(modsrc);
  mod->state = 2;
  if (!r.ok)
    error_at(p, r.message);
  return mod;
}

static void emit_require_expr(parser* p, compiler* c) {
  advance(p);
  if (!check(p, tok_str)) {
    error_at(p, "require() only supports a string literal argument");
    return;
  }
  tok spec_tok = p->cur;
  advance(p);
  if (!expect(p, tok_rparen))
    return;
  char* spec = decode_string(spec_tok);
  if (emit_node_builtin_ref(c, spec)) {
    free(spec);
    return;
  }
  if (!c->modctx) {
    error_at(p, "require() is not supported in this context");
    free(spec);
    return;
  }
  compiled_module* mod =
      get_or_compile_module(c->modctx, c->module_dir, spec, 1, p);
  free(spec);
  if (!mod)
    return;
  uint16_t req_idx =
      cf_methodref(c->cf, mod->class_name, "moduleExports", "()LV6Value;");
  op_emit2(c->m, op_invokestatic, req_idx);
}

static void declare_or_assign_module_binding(compiler* c, tok name) {
  if (c->brace_depth == 0) {
    local* le = find_local_entry(c, name.start, name.len);
    if (le) {
      var_ref vr;
      vr.kind = var_local;
      vr.index = le->slot;
      emit_var_write_ref(c, vr);
      op_emit(c->m, op_pop);
      return;
    }
  }
  uint16_t slot = c->next_local_slot++;
  emit_var_declare(c, slot);
  add_local(c, name, slot, 0, 0);
}

static void parse_import_stmt(parser* p, compiler* c) {
  if (!c->modctx) {
    error_at(p, "imports are not supported in this context");
    return;
  }

  if (check(p, tok_str)) {
    tok spec_tok = p->cur;
    advance(p);
    char* spec = decode_string(spec_tok);
    if (emit_node_builtin_ref(c, spec)) {
      free(spec);
      op_emit(c->m, op_pop);
      expect(p, tok_semi);
      return;
    }
    compiled_module* mod =
        get_or_compile_module(c->modctx, c->module_dir, spec, 0, p);
    free(spec);
    expect(p, tok_semi);
    if (!mod)
      return;
    if (mod->kind == 1) {
      uint16_t exp_idx =
          cf_methodref(c->cf, mod->class_name, "moduleExports", "()LV6Value;");
      op_emit2(c->m, op_invokestatic, exp_idx);
    } else {
      uint16_t exp_idx =
          cf_methodref(c->cf, mod->class_name, "exports", "()LV6Object;");
      op_emit2(c->m, op_invokestatic, exp_idx);
    }
    op_emit(c->m, op_pop);
    return;
  }

  int has_default = 0;
  tok default_name;
  int has_namespace = 0;
  tok namespace_name;
  int has_named = 0;
  tok named_local[64];
  tok named_key[64];
  int named_count = 0;

  if (check(p, tok_ident)) {
    has_default = 1;
    default_name = p->cur;
    advance(p);
    if (match(p, tok_comma)) {
      if (match(p, tok_star)) {
        if (!expect(p, tok_kw_as))
          return;
        if (!expect(p, tok_ident))
          return;
        has_namespace = 1;
        namespace_name = p->prev;
      } else if (match(p, tok_lbrace)) {
        has_named = 1;
        if (!check(p, tok_rbrace)) {
          for (;;) {
            if (!expect(p, tok_ident))
              return;
            tok key = p->prev;
            tok local = key;
            if (match(p, tok_kw_as)) {
              if (!expect(p, tok_ident))
                return;
              local = p->prev;
            }
            if (named_count < 64) {
              named_key[named_count] = key;
              named_local[named_count] = local;
              named_count++;
            }
            if (!match(p, tok_comma))
              break;
            if (check(p, tok_rbrace))
              break;
          }
        }
        if (!expect(p, tok_rbrace))
          return;
      }
    }
  } else if (match(p, tok_star)) {
    if (!expect(p, tok_kw_as))
      return;
    if (!expect(p, tok_ident))
      return;
    has_namespace = 1;
    namespace_name = p->prev;
  } else if (match(p, tok_lbrace)) {
    has_named = 1;
    if (!check(p, tok_rbrace)) {
      for (;;) {
        if (!expect(p, tok_ident))
          return;
        tok key = p->prev;
        tok local = key;
        if (match(p, tok_kw_as)) {
          if (!expect(p, tok_ident))
            return;
          local = p->prev;
        }
        if (named_count < 64) {
          named_key[named_count] = key;
          named_local[named_count] = local;
          named_count++;
        }
        if (!match(p, tok_comma))
          break;
        if (check(p, tok_rbrace))
          break;
      }
    }
    if (!expect(p, tok_rbrace))
      return;
  }

  if (!expect(p, tok_kw_from))
    return;
  if (!check(p, tok_str)) {
    error_at(p, "expected module specifier string");
    return;
  }
  tok spec_tok = p->cur;
  advance(p);
  char* spec = decode_string(spec_tok);

  int is_value_shape;
  uint16_t exports_slot = c->next_local_slot++;

  if (emit_node_builtin_ref(c, spec)) {
    free(spec);
    is_value_shape = 1;
    emit_astore(c->m, exports_slot);
  } else {
    compiled_module* mod =
        get_or_compile_module(c->modctx, c->module_dir, spec, 0, p);
    free(spec);
    if (!mod) {
      expect(p, tok_semi);
      return;
    }
    if (mod->kind == 1) {
      uint16_t exp_idx =
          cf_methodref(c->cf, mod->class_name, "moduleExports", "()LV6Value;");
      op_emit2(c->m, op_invokestatic, exp_idx);
      is_value_shape = 1;
    } else {
      uint16_t exp_idx =
          cf_methodref(c->cf, mod->class_name, "exports", "()LV6Object;");
      op_emit2(c->m, op_invokestatic, exp_idx);
      is_value_shape = 0;
    }
    emit_astore(c->m, exports_slot);
  }
  expect(p, tok_semi);

  uint16_t get_obj_idx =
      cf_methodref(c->cf, "V6Object", "get", "(Ljava/lang/String;)LV6Value;");
  uint16_t get_val_idx =
      cf_methodref(c->cf, "V6Value", "getProp", "(Ljava/lang/String;)LV6Value;");
  uint16_t get_idx = is_value_shape ? get_val_idx : get_obj_idx;

  if (has_default) {
    emit_aload(c->m, exports_slot);
    if (!is_value_shape) {
      uint16_t key_idx = cf_string(c->cf, "default");
      op_emit2(c->m, op_ldc_w, key_idx);
      op_emit2(c->m, op_invokevirtual, get_obj_idx);
    }
    declare_or_assign_module_binding(c, default_name);
  }

  if (has_namespace) {
    emit_aload(c->m, exports_slot);
    if (!is_value_shape)
      emit_box_object_ref(c);
    declare_or_assign_module_binding(c, namespace_name);
  }

  if (has_named) {
    for (int i = 0; i < named_count; i++) {
      emit_aload(c->m, exports_slot);
      char* keystr = dup_tok(named_key[i]);
      uint16_t key_idx = cf_string(c->cf, keystr);
      free(keystr);
      op_emit2(c->m, op_ldc_w, key_idx);
      op_emit2(c->m, op_invokevirtual, get_idx);
      declare_or_assign_module_binding(c, named_local[i]);
    }
  }
}

static void parse_stmt(parser* p, compiler* c) {
  if (match(p, tok_lbrace)) {
    parse_block(p, c);
    return;
  }

  if (match(p, tok_kw_class)) {
    parse_class_decl(p, c);
    return;
  }

  if (match(p, tok_kw_import)) {
    parse_import_stmt(p, c);
    return;
  }

  if (match(p, tok_kw_if)) {
    parse_if(p, c);
    return;
  }

  if (match(p, tok_kw_while)) {
    parse_while(p, c);
    return;
  }

  if (match(p, tok_kw_for)) {
    parse_for(p, c);
    return;
  }

  if (match(p, tok_kw_switch)) {
    parse_switch(p, c);
    return;
  }

  if (match(p, tok_kw_break)) {
    if (check(p, tok_ident)) {
      tok label = p->cur;
      advance(p);
      int found = -1;
      for (int i = c->label_count - 1; i >= 0; i--) {
        if (c->label_lens[i] == label.len &&
            memcmp(c->label_names[i], label.start, label.len) == 0) {
          found = i;
          break;
        }
      }
      if (found < 0) {
        error_at(p, "undefined label");
      } else {
        emit_pending_finally_for_break(c, c->label_break_depth[found]);
        break_ctx* bc = &c->breaks[c->label_break_depth[found]];
        bc->jumps[bc->count++] = op_pos(c->m);
        op_emit2(c->m, op_goto, 0);
      }
    } else if (c->break_depth == 0) {
      error_at(p, "'break' outside loop or switch");
    } else {
      emit_pending_finally_for_break(c, c->break_depth - 1);
      break_ctx* bc = &c->breaks[c->break_depth - 1];
      bc->jumps[bc->count++] = op_pos(c->m);
      op_emit2(c->m, op_goto, 0);
    }
    expect(p, tok_semi);
    return;
  }

  if (match(p, tok_kw_continue)) {
    if (check(p, tok_ident)) {
      tok label = p->cur;
      advance(p);
      int found = -1;
      for (int i = c->label_count - 1; i >= 0; i--) {
        if (c->label_lens[i] == label.len &&
            memcmp(c->label_names[i], label.start, label.len) == 0) {
          found = i;
          break;
        }
      }
      if (found < 0 || c->label_continue_depth[found] < 0) {
        error_at(p, "undefined label or label does not denote a loop");
      } else {
        emit_pending_finally_for_continue(c, c->label_continue_depth[found]);
        size_t target = c->continues[c->label_continue_depth[found]];
        size_t here = op_pos(c->m);
        op_emit2(c->m, op_goto, 0);
        op_patch2(c->m, (uint16_t)(here + 1), (uint16_t)(target - here));
      }
    } else if (c->continue_depth == 0) {
      error_at(p, "'continue' outside loop");
    } else {
      emit_pending_finally_for_continue(c, c->continue_depth - 1);
      size_t target = c->continues[c->continue_depth - 1];
      size_t here = op_pos(c->m);
      op_emit2(c->m, op_goto, 0);
      op_patch2(c->m, (uint16_t)(here + 1), (uint16_t)(target - here));
    }
    expect(p, tok_semi);
    return;
  }

  if (match(p, tok_kw_throw)) {
    parse_expr(p, c);
    expect(p, tok_semi);
    uint16_t throw_cls = cf_class(c->cf, "V6Throw");
    uint16_t throw_ctor =
        cf_methodref(c->cf, "V6Throw", "<init>", "(LV6Value;)V");
    op_emit2(c->m, op_new, throw_cls);
    op_emit(c->m, op_dup_x1);
    op_emit(c->m, op_swap);
    op_emit2(c->m, op_invokespecial, throw_ctor);
    op_emit(c->m, op_athrow);
    return;
  }

  if (match(p, tok_kw_try)) {
    parse_try(p, c);
    return;
  }

  if (match(p, tok_kw_var) || match(p, tok_kw_let) || match(p, tok_kw_const)) {
    parse_var_decl(p, c, p->prev.kind);
    expect(p, tok_semi);
    return;
  }

  if (match(p, tok_kw_return)) {
    if (check(p, tok_semi))
      emit_undef(c->cf, c->m);
    else
      parse_expr(p, c);
    expect(p, tok_semi);
    emit_all_pending_finally(c);
    op_emit(c->m, op_areturn);
    return;
  }

  if (check(p, tok_ident)) {
    lexer save_lex = p->lex;
    tok save_cur = p->cur;
    tok save_prev = p->prev;
    tok label = p->cur;
    advance(p);
    if (check(p, tok_colon)) {
      advance(p);
      parse_labeled_stmt(p, c, label);
      return;
    }
    p->lex = save_lex;
    p->cur = save_cur;
    p->prev = save_prev;
  }

  if (check(p, tok_lbracket)) {
    lexer save_lex = p->lex;
    tok save_cur = p->cur;
    tok save_prev = p->prev;
    advance(p);
    const char* pattern_start = p->cur.start;
    skip_balanced(p, tok_lbracket, tok_rbracket);
    if (check(p, tok_assign)) {
      advance(p);
      parse_expr(p, c);
      uint16_t src_slot = c->next_local_slot++;
      emit_astore(c->m, src_slot);
      parser pp;
      parser_init(&pp, pattern_start);
      parse_array_pattern(&pp, c, tok_kw_var, src_slot);
      expect(p, tok_semi);
      return;
    }
    p->lex = save_lex;
    p->cur = save_cur;
    p->prev = save_prev;
  }

  parse_expr(p, c);
  op_emit(c->m, op_pop);
  expect(p, tok_semi);
}

static void prescan_hoist_one(compiler* c, tok name) {
  uint16_t slot = c->next_local_slot++;
  emit_undef(c->cf, c->m);
  emit_var_declare(c, slot);
  add_local(c, name, slot, 1, 0);
}

static tok prescan_pattern_hoist(compiler* c, lexer* lx, int is_array) {
  tok_kind close = is_array ? tok_rbracket : tok_rbrace;
  tok t = lex_next(lx);
  int want_key = !is_array;

  while (t.kind != close && t.kind != tok_eof) {
    if (t.kind == tok_comma) {
      want_key = !is_array;
      t = lex_next(lx);
      continue;
    }
    if (t.kind == tok_ellipsis) {
      t = lex_next(lx);
      continue;
    }
    if (t.kind == tok_ident) {
      if (is_array) {
        prescan_hoist_one(c, t);
        t = lex_next(lx);
      } else if (want_key) {
        tok key = t;
        t = lex_next(lx);
        if (t.kind == tok_colon) {
          t = lex_next(lx);
          if (t.kind == tok_ident) {
            prescan_hoist_one(c, t);
            t = lex_next(lx);
          }
        } else {
          prescan_hoist_one(c, key);
        }
        want_key = 0;
      } else {
        t = lex_next(lx);
      }
      continue;
    }
    if (t.kind == tok_assign) {
      int edepth = 0;
      t = lex_next(lx);
      while (t.kind != tok_eof) {
        if (t.kind == tok_lparen || t.kind == tok_lbracket ||
            t.kind == tok_lbrace) {
          edepth++;
        } else if (t.kind == tok_rparen || t.kind == tok_rbracket ||
                   t.kind == tok_rbrace) {
          if (edepth == 0)
            break;
          edepth--;
        } else if (edepth == 0 && t.kind == tok_comma) {
          break;
        }
        t = lex_next(lx);
      }
      continue;
    }
    t = lex_next(lx);
  }
  if (t.kind == close)
    t = lex_next(lx);
  return t;
}

#define v6_max_pending_fns 64

static void prescan_decls(compiler* c, const char* src, int hoist_functions) {
  const char* pending_fns[v6_max_pending_fns];
  int pending_fns_async[v6_max_pending_fns];
  int pending_count = 0;

  lexer lx;
  lex_init(&lx, src);
  int depth = 0;
  tok t = lex_next(&lx);
  while (t.kind != tok_eof) {
    if (t.kind == tok_lbrace) {
      depth++;
    } else if (t.kind == tok_rbrace) {
      if (depth == 0)
        break;
      depth--;
    } else if (t.kind == tok_kw_var) {
      for (;;) {
        tok name = lex_next(&lx);
        if (name.kind == tok_lbracket || name.kind == tok_lbrace) {
          t = prescan_pattern_hoist(c, &lx, name.kind == tok_lbracket);
          if (t.kind != tok_assign)
            break;
          int edepth2 = 0;
          t = lex_next(&lx);
          while (t.kind != tok_eof) {
            if (t.kind == tok_lparen || t.kind == tok_lbracket ||
                t.kind == tok_lbrace) {
              edepth2++;
            } else if (t.kind == tok_rparen || t.kind == tok_rbracket ||
                       t.kind == tok_rbrace) {
              if (edepth2 == 0)
                break;
              edepth2--;
            } else if (edepth2 == 0 &&
                       (t.kind == tok_comma || t.kind == tok_semi)) {
              break;
            }
            t = lex_next(&lx);
          }
          if (t.kind != tok_comma)
            break;
          continue;
        }
        if (name.kind != tok_ident) {
          t = name;
          break;
        }
        uint16_t slot = c->next_local_slot++;
        emit_undef(c->cf, c->m);
        emit_var_declare(c, slot);
        add_local(c, name, slot, 1, 0);

        t = lex_next(&lx);
        if (t.kind == tok_assign) {
          int edepth = 0;
          t = lex_next(&lx);
          while (t.kind != tok_eof) {
            if (t.kind == tok_lparen || t.kind == tok_lbracket ||
                t.kind == tok_lbrace) {
              edepth++;
            } else if (t.kind == tok_rparen || t.kind == tok_rbracket ||
                       t.kind == tok_rbrace) {
              if (edepth == 0)
                break;
              edepth--;
            } else if (edepth == 0 &&
                       (t.kind == tok_comma || t.kind == tok_semi)) {
              break;
            }
            t = lex_next(&lx);
          }
        }
        if (t.kind != tok_comma)
          break;
      }
      continue;
    } else if (hoist_functions && depth == 0 &&
               (t.kind == tok_kw_let || t.kind == tok_kw_const)) {
      int is_const_decl = t.kind == tok_kw_const;
      for (;;) {
        tok name = lex_next(&lx);
        if (name.kind == tok_lbracket || name.kind == tok_lbrace) {
          t = prescan_pattern_hoist(c, &lx, name.kind == tok_lbracket);
          if (t.kind != tok_assign)
            break;
          int edepth2 = 0;
          t = lex_next(&lx);
          while (t.kind != tok_eof) {
            if (t.kind == tok_lparen || t.kind == tok_lbracket ||
                t.kind == tok_lbrace) {
              edepth2++;
            } else if (t.kind == tok_rparen || t.kind == tok_rbracket ||
                       t.kind == tok_rbrace) {
              if (edepth2 == 0)
                break;
              edepth2--;
            } else if (edepth2 == 0 &&
                       (t.kind == tok_comma || t.kind == tok_semi)) {
              break;
            }
            t = lex_next(&lx);
          }
          if (t.kind != tok_comma)
            break;
          continue;
        }
        if (name.kind != tok_ident) {
          t = name;
          break;
        }
        uint16_t slot = c->next_local_slot++;
        emit_undef(c->cf, c->m);
        emit_var_declare(c, slot);
        add_local(c, name, slot, 0, is_const_decl);

        t = lex_next(&lx);
        if (t.kind == tok_assign) {
          int edepth = 0;
          t = lex_next(&lx);
          while (t.kind != tok_eof) {
            if (t.kind == tok_lparen || t.kind == tok_lbracket ||
                t.kind == tok_lbrace) {
              edepth++;
            } else if (t.kind == tok_rparen || t.kind == tok_rbracket ||
                       t.kind == tok_rbrace) {
              if (edepth == 0)
                break;
              edepth--;
            } else if (edepth == 0 &&
                       (t.kind == tok_comma || t.kind == tok_semi)) {
              break;
            }
            t = lex_next(&lx);
          }
        }
        if (t.kind != tok_comma)
          break;
      }
      continue;
    } else if (hoist_functions && depth == 0 && t.kind == tok_kw_import) {
      t = lex_next(&lx);
      if (t.kind == tok_str) {
        t = lex_next(&lx);
      } else if (t.kind == tok_ident) {
        prescan_hoist_one(c, t);
        t = lex_next(&lx);
        if (t.kind == tok_comma) {
          t = lex_next(&lx);
          if (t.kind == tok_star) {
            t = lex_next(&lx);
            if (t.kind == tok_kw_as) {
              t = lex_next(&lx);
              if (t.kind == tok_ident) {
                prescan_hoist_one(c, t);
                t = lex_next(&lx);
              }
            }
          } else if (t.kind == tok_lbrace) {
            t = lex_next(&lx);
            while (t.kind != tok_rbrace && t.kind != tok_eof) {
              if (t.kind == tok_ident) {
                tok local = t;
                t = lex_next(&lx);
                if (t.kind == tok_kw_as) {
                  t = lex_next(&lx);
                  if (t.kind == tok_ident) {
                    local = t;
                    t = lex_next(&lx);
                  }
                }
                prescan_hoist_one(c, local);
              } else {
                t = lex_next(&lx);
              }
              if (t.kind == tok_comma)
                t = lex_next(&lx);
            }
            if (t.kind == tok_rbrace)
              t = lex_next(&lx);
          }
        }
      } else if (t.kind == tok_star) {
        t = lex_next(&lx);
        if (t.kind == tok_kw_as) {
          t = lex_next(&lx);
          if (t.kind == tok_ident) {
            prescan_hoist_one(c, t);
            t = lex_next(&lx);
          }
        }
      } else if (t.kind == tok_lbrace) {
        t = lex_next(&lx);
        while (t.kind != tok_rbrace && t.kind != tok_eof) {
          if (t.kind == tok_ident) {
            tok local = t;
            t = lex_next(&lx);
            if (t.kind == tok_kw_as) {
              t = lex_next(&lx);
              if (t.kind == tok_ident) {
                local = t;
                t = lex_next(&lx);
              }
            }
            prescan_hoist_one(c, local);
          } else {
            t = lex_next(&lx);
          }
          if (t.kind == tok_comma)
            t = lex_next(&lx);
        }
        if (t.kind == tok_rbrace)
          t = lex_next(&lx);
      }
      while (t.kind != tok_semi && t.kind != tok_eof)
        t = lex_next(&lx);
      if (t.kind == tok_semi)
        t = lex_next(&lx);
      continue;
    } else if (hoist_functions && depth == 0 && t.kind == tok_kw_function) {
      const char* body_start = lx.cur;
      tok name = lex_next(&lx);
      if (name.kind == tok_star)
        name = lex_next(&lx);
      if (name.kind == tok_ident) {
        prescan_hoist_one(c, name);
        if (pending_count < v6_max_pending_fns) {
          pending_fns[pending_count] = body_start;
          pending_fns_async[pending_count] = 0;
          pending_count++;
        }
      }

      t = lex_next(&lx);
      if (t.kind == tok_lparen) {
        int pdepth = 1;
        t = lex_next(&lx);
        while (t.kind != tok_eof && pdepth > 0) {
          if (t.kind == tok_lparen)
            pdepth++;
          else if (t.kind == tok_rparen)
            pdepth--;
          t = lex_next(&lx);
        }
      }
      if (t.kind == tok_lbrace) {
        int bdepth = 1;
        t = lex_next(&lx);
        while (t.kind != tok_eof && bdepth > 0) {
          if (t.kind == tok_lbrace)
            bdepth++;
          else if (t.kind == tok_rbrace)
            bdepth--;
          t = lex_next(&lx);
        }
      }
      continue;
    } else if (hoist_functions && depth == 0 && t.kind == tok_kw_async) {
      lexer peek_lx = lx;
      tok next = lex_next(&peek_lx);
      if (next.kind != tok_kw_function) {
        t = lex_next(&lx);
        continue;
      }
      lx = peek_lx;
      const char* body_start = lx.cur;
      tok name = lex_next(&lx);
      if (name.kind == tok_star)
        name = lex_next(&lx);
      if (name.kind == tok_ident) {
        prescan_hoist_one(c, name);
        if (pending_count < v6_max_pending_fns) {
          pending_fns[pending_count] = body_start;
          pending_fns_async[pending_count] = 1;
          pending_count++;
        }
      }

      t = lex_next(&lx);
      if (t.kind == tok_lparen) {
        int pdepth = 1;
        t = lex_next(&lx);
        while (t.kind != tok_eof && pdepth > 0) {
          if (t.kind == tok_lparen)
            pdepth++;
          else if (t.kind == tok_rparen)
            pdepth--;
          t = lex_next(&lx);
        }
      }
      if (t.kind == tok_lbrace) {
        int bdepth = 1;
        t = lex_next(&lx);
        while (t.kind != tok_eof && bdepth > 0) {
          if (t.kind == tok_lbrace)
            bdepth++;
          else if (t.kind == tok_rbrace)
            bdepth--;
          t = lex_next(&lx);
        }
      }
      continue;
    } else if (hoist_functions && depth == 0 && t.kind == tok_kw_class) {
      tok name = lex_next(&lx);
      if (name.kind == tok_ident)
        prescan_hoist_one(c, name);

      t = lex_next(&lx);
      if (t.kind == tok_kw_extends) {
        lex_next(&lx);
        t = lex_next(&lx);
      }
      if (t.kind == tok_lbrace) {
        int cdepth = 1;
        t = lex_next(&lx);
        while (t.kind != tok_eof && cdepth > 0) {
          if (t.kind == tok_lbrace)
            cdepth++;
          else if (t.kind == tok_rbrace)
            cdepth--;
          t = lex_next(&lx);
        }
      }
      continue;
    }
    t = lex_next(&lx);
  }

  for (int i = 0; i < pending_count; i++) {
    parser fp;
    parser_init(&fp, pending_fns[i]);
    int is_async = pending_fns_async[i];
    int is_gen = match(&fp, tok_star);
    if (!check(&fp, tok_ident))
      continue;
    tok name = fp.cur;
    advance(&fp);
    char* lambda_name = malloc(24);
    if (!is_gen && !is_async &&
        !name_reassigned_in_scope(src, name.start, name.len)) {
      local* le = find_local_entry(c, name.start, name.len);
      if (le) {
        le->direct_fn = 1;
        le->fn_method_name = lambda_name;
      }
    }
    c->pending_async_gen = is_gen && is_async;
    compile_closure_value(&fp, c, 0, 1, lambda_name);
    if (is_gen && is_async)
      emit_wrap_async_generator(c);
    else if (is_gen)
      emit_wrap_generator(c);
    else if (is_async)
      emit_wrap_async(c);
    var_ref vr = resolve_var(c, name.start, name.len);
    if (vr.kind != var_not_found) {
      emit_var_write_ref(c, vr);
      op_emit(c->m, op_pop);
    }
  }
}

static void parse_program(parser* p, compiler* c) {
  while (!check(p, tok_eof)) {
    if (match(p, tok_kw_function)) {
      parse_function_decl(p, c);
    } else if (check(p, tok_kw_async)) {
      lexer save_lex = p->lex;
      tok save_cur = p->cur;
      tok save_prev = p->prev;
      advance(p);
      if (match(p, tok_kw_function)) {
        parse_function_decl(p, c);
      } else {
        p->lex = save_lex;
        p->cur = save_cur;
        p->prev = save_prev;
        parse_stmt(p, c);
      }
    } else {
      parse_stmt(p, c);
    }
  }
}

#define v6_max_exports 128

typedef struct export_binding {
  char local_name[64];
  char export_key[64];
} export_binding;

static void blank_range(char* src, size_t start, size_t end) {
  for (size_t i = start; i < end; i++)
    if (src[i] != '\n')
      src[i] = ' ';
}

static void record_export(export_binding* bindings, int* count, tok local,
                          tok key) {
  if (*count >= v6_max_exports)
    return;
  size_t nlen = local.len < 63 ? local.len : 63;
  memcpy(bindings[*count].local_name, local.start, nlen);
  bindings[*count].local_name[nlen] = '\0';
  size_t klen = key.len < 63 ? key.len : 63;
  memcpy(bindings[*count].export_key, key.start, klen);
  bindings[*count].export_key[klen] = '\0';
  (*count)++;
}

static void preprocess_exports(char* src, export_binding* bindings,
                               int* count) {
  lexer lx;
  lex_init(&lx, src);
  int depth = 0;
  tok t = lex_next(&lx);
  while (t.kind != tok_eof) {
    if (t.kind == tok_lbrace) {
      depth++;
      t = lex_next(&lx);
      continue;
    }
    if (t.kind == tok_rbrace) {
      depth--;
      t = lex_next(&lx);
      continue;
    }
    if (depth != 0 || t.kind != tok_kw_export) {
      t = lex_next(&lx);
      continue;
    }

    size_t export_start = (size_t)(t.start - src);
    tok next = lex_next(&lx);

    if (next.kind == tok_kw_default) {
      size_t default_end = (size_t)(next.start - src) + next.len;
      tok after = lex_next(&lx);
      if (after.kind == tok_kw_function || after.kind == tok_kw_class) {
        tok maybe = lex_next(&lx);
        if (maybe.kind == tok_star)
          maybe = lex_next(&lx);
        if (maybe.kind == tok_ident) {
          tok key = maybe;
          key.start = "default";
          key.len = 7;
          record_export(bindings, count, maybe, key);
        }
        blank_range(src, export_start, default_end);
        t = lex_next(&lx);
        continue;
      }
      const char* tmpl = "let $dflt$   =";
      memcpy(src + export_start, tmpl, 14);
      tok synth;
      synth.start = "$dflt$";
      synth.len = 6;
      tok key;
      key.start = "default";
      key.len = 7;
      record_export(bindings, count, synth, key);
      t = after;
      continue;
    }

    if (next.kind == tok_kw_function || next.kind == tok_kw_class) {
      blank_range(src, export_start, export_start + 6);
      tok maybe = lex_next(&lx);
      if (maybe.kind == tok_star)
        maybe = lex_next(&lx);
      if (maybe.kind == tok_ident)
        record_export(bindings, count, maybe, maybe);
      t = lex_next(&lx);
      continue;
    }

    if (next.kind == tok_kw_async) {
      blank_range(src, export_start, export_start + 6);
      tok fnkw = lex_next(&lx);
      if (fnkw.kind == tok_kw_function) {
        tok maybe = lex_next(&lx);
        if (maybe.kind == tok_star)
          maybe = lex_next(&lx);
        if (maybe.kind == tok_ident)
          record_export(bindings, count, maybe, maybe);
      }
      t = lex_next(&lx);
      continue;
    }

    if (next.kind == tok_kw_var || next.kind == tok_kw_let ||
        next.kind == tok_kw_const) {
      blank_range(src, export_start, export_start + 6);
      for (;;) {
        tok name_tok = lex_next(&lx);
        if (name_tok.kind == tok_ident)
          record_export(bindings, count, name_tok, name_tok);
        int ed = 0;
        tok tt = lex_next(&lx);
        while (tt.kind != tok_eof) {
          if (tt.kind == tok_lparen || tt.kind == tok_lbracket ||
              tt.kind == tok_lbrace) {
            ed++;
          } else if (tt.kind == tok_rparen || tt.kind == tok_rbracket ||
                     tt.kind == tok_rbrace) {
            if (ed == 0)
              break;
            ed--;
          } else if (ed == 0 && (tt.kind == tok_comma || tt.kind == tok_semi)) {
            break;
          }
          tt = lex_next(&lx);
        }
        if (tt.kind != tok_comma) {
          t = lex_next(&lx);
          break;
        }
      }
      continue;
    }

    if (next.kind == tok_lbrace) {
      int ed = 1;
      tok inner = lex_next(&lx);
      while (ed > 0 && inner.kind != tok_eof) {
        if (inner.kind == tok_ident) {
          tok name_tok = inner;
          tok peek = lex_next(&lx);
          tok export_name_tok = name_tok;
          if (peek.kind == tok_kw_as) {
            export_name_tok = lex_next(&lx);
            inner = lex_next(&lx);
          } else {
            inner = peek;
          }
          record_export(bindings, count, name_tok, export_name_tok);
          continue;
        }
        if (inner.kind == tok_lbrace)
          ed++;
        else if (inner.kind == tok_rbrace) {
          ed--;
          if (ed == 0)
            break;
        }
        inner = lex_next(&lx);
      }
      tok after_brace = lex_next(&lx);
      size_t stmt_end;
      if (after_brace.kind == tok_semi) {
        stmt_end = (size_t)(after_brace.start - src) + after_brace.len;
        t = lex_next(&lx);
      } else {
        stmt_end = (size_t)(after_brace.start - src);
        t = after_brace;
      }
      blank_range(src, export_start, stmt_end);
      continue;
    }

    blank_range(src, export_start, export_start + 6);
    t = next;
  }
}

static void bind_builtin(compiler* c, const char* name, const char* field) {
  uint16_t slot = c->next_local_slot++;
  uint16_t fidx = cf_fieldref(c->cf, "V6Builtins", field, "LV6Value;");
  op_emit2(c->m, op_getstatic, fidx);
  emit_var_declare(c, slot);
  tok t;
  t.kind = tok_ident;
  t.start = name;
  t.len = strlen(name);
  t.line = 0;
  t.num = 0;
  add_local(c, t, slot, 1, 0);
}

static const char* const v6_prelude_src =
    "class Error {\n"
    "  constructor(message) {\n"
    "    this.message = message === undefined ? \"\" : message;\n"
    "    this.name = \"Error\";\n"
    "  }\n"
    "  toString() {\n"
    "    return this.message ? (this.name + \": \" + this.message) : "
    "this.name;\n"
    "  }\n"
    "}\n"
    "class TypeError extends Error {\n"
    "  constructor(message) {\n"
    "    super(message);\n"
    "    this.name = \"TypeError\";\n"
    "  }\n"
    "}\n"
    "class RangeError extends Error {\n"
    "  constructor(message) {\n"
    "    super(message);\n"
    "    this.name = \"RangeError\";\n"
    "  }\n"
    "}\n"
    "class SyntaxError extends Error {\n"
    "  constructor(message) {\n"
    "    super(message);\n"
    "    this.name = \"SyntaxError\";\n"
    "  }\n"
    "}\n"
    "class ReferenceError extends Error {\n"
    "  constructor(message) {\n"
    "    super(message);\n"
    "    this.name = \"ReferenceError\";\n"
    "  }\n"
    "}\n"
    "class EvalError extends Error {\n"
    "  constructor(message) {\n"
    "    super(message);\n"
    "    this.name = \"EvalError\";\n"
    "  }\n"
    "}\n"
    "class URIError extends Error {\n"
    "  constructor(message) {\n"
    "    super(message);\n"
    "    this.name = \"URIError\";\n"
    "  }\n"
    "}\n";

static int count_newlines(const char* s) {
  int n = 0;
  for (; *s; s++)
    if (*s == '\n')
      n++;
  return n;
}

static compile_result
compile_module_impl(class_file* cf, const char* this_class_name,
                    const char* user_src, const char* module_dir,
                    module_ctx* modctx, int is_entry, int is_cjs) {
  size_t prelude_len = strlen(v6_prelude_src);
  size_t user_len = strlen(user_src);
  char* src = malloc(prelude_len + user_len + 1);
  memcpy(src, v6_prelude_src, prelude_len);
  memcpy(src + prelude_len, user_src, user_len + 1);
  int prelude_lines = count_newlines(v6_prelude_src);

  export_binding exports_list[v6_max_exports];
  int exports_count = 0;
  if (!is_cjs)
    preprocess_exports(src + prelude_len, exports_list, &exports_count);

  method* main_m;
  if (is_entry) {
    main_m = cf_method(cf, acc_public | acc_static, "main",
                       "([Ljava/lang/String;)V");
  } else if (is_cjs) {
    main_m =
        cf_method(cf, acc_public | acc_static, "moduleExports", "()LV6Value;");
    cf_field(cf, acc_public | acc_static, "MODULE_CACHE", "LV6Object;");
  } else {
    main_m = cf_method(cf, acc_public | acc_static, "exports", "()LV6Object;");
    cf_field(cf, acc_public | acc_static, "EXPORTS_CACHE", "LV6Object;");
  }
  main_m->max_stack = 64;

  int lambda_counter = 0;

  compiler c;
  c.cf = cf;
  c.m = main_m;
  c.parent = NULL;
  c.lambda_counter = &lambda_counter;
  c.is_arrow = 0;
  c.param_count = 0;
  c.local_count = 0;
  c.scratch_slot = 1;
  c.next_local_slot = 3;
  c.upvalue_count = 0;
  c.break_depth = 0;
  c.continue_depth = 0;
  c.catch_depth = 0;
  c.brace_depth = 0;
  c.super_name = NULL;
  c.super_len = 0;
  c.class_name = NULL;
  c.class_name_len = 0;
  c.pending_field_count = 0;
  c.label_count = 0;
  c.pending_label_count = 0;
  c.finally_depth = 0;
  c.is_async_gen = 0;
  c.pending_async_gen = 0;
  c.is_module = !is_entry;
  c.this_class_name = this_class_name;
  c.modctx = modctx;
  c.module_dir = module_dir;

  int top_has_closures = 0;
  {
    lexer scan_lx;
    lex_init(&scan_lx, src);
    tok st = lex_next(&scan_lx);
    while (st.kind != tok_eof) {
      if (st.kind == tok_kw_function || st.kind == tok_arrow ||
          st.kind == tok_kw_class) {
        top_has_closures = 1;
        break;
      }
      st = lex_next(&scan_lx);
    }
  }
  c.box_locals = top_has_closures;

  bind_builtin(&c, "console", "CONSOLE");
  bind_builtin(&c, "Math", "MATH");
  bind_builtin(&c, "Object", "OBJECT");
  bind_builtin(&c, "Array", "ARRAY");
  bind_builtin(&c, "atob", "ATOB");
  bind_builtin(&c, "btoa", "BTOA");
  bind_builtin(&c, "Number", "NUMBER");
  bind_builtin(&c, "parseInt", "PARSE_INT");
  bind_builtin(&c, "BigInt", "BIGINT");
  bind_builtin(&c, "parseFloat", "PARSE_FLOAT");
  bind_builtin(&c, "isNaN", "IS_NAN");
  bind_builtin(&c, "isFinite", "IS_FINITE");
  bind_builtin(&c, "NaN", "NAN_VALUE");
  bind_builtin(&c, "Infinity", "INFINITY_VALUE");
  bind_builtin(&c, "Map", "MAP");
  bind_builtin(&c, "Set", "SET");
  bind_builtin(&c, "WeakMap", "WEAK_MAP");
  bind_builtin(&c, "WeakSet", "WEAK_SET");
  bind_builtin(&c, "Symbol", "SYMBOL");
  bind_builtin(&c, "Promise", "PROMISE");
  bind_builtin(&c, "RegExp", "REGEXP");
  bind_builtin(&c, "JSON", "JSON");
  bind_builtin(&c, "Buffer", "BUFFER");
  bind_builtin(&c, "process", "PROCESS");
  bind_builtin(&c, "URL", "URL_CTOR");
  bind_builtin(&c, "URLSearchParams", "URL_SEARCH_PARAMS_CTOR");
  bind_builtin(&c, "setTimeout", "SET_TIMEOUT");
  bind_builtin(&c, "clearTimeout", "CLEAR_TIMEOUT");
  bind_builtin(&c, "setInterval", "SET_INTERVAL");
  bind_builtin(&c, "clearInterval", "CLEAR_INTERVAL");
  bind_builtin(&c, "setImmediate", "SET_IMMEDIATE");
  bind_builtin(&c, "clearImmediate", "CLEAR_IMMEDIATE");
  bind_builtin(&c, "queueMicrotask", "QUEUE_MICROTASK");

  if (is_entry) {
    uint16_t setargv_idx = cf_methodref(cf, "V6Process", "setArgv",
                                        "([Ljava/lang/String;)V");
    emit_aload(main_m, 0);
    op_emit2(main_m, op_invokestatic, setargv_idx);
  }

  uint16_t this_slot = c.next_local_slot++;
  emit_undef(cf, main_m);
  emit_var_declare(&c, this_slot);
  tok this_tok;
  this_tok.kind = tok_kw_this;
  this_tok.start = "this";
  this_tok.len = 4;
  this_tok.line = 0;
  this_tok.num = 0;
  add_local(&c, this_tok, this_slot, 1, 0);

  uint16_t module_local_slot = 0;
  uint16_t exports_local_slot = 0;

  if (is_cjs) {
    uint16_t cache_field =
        cf_fieldref(cf, this_class_name, "MODULE_CACHE", "LV6Object;");
    uint16_t get_idx =
        cf_methodref(cf, "V6Object", "get", "(Ljava/lang/String;)LV6Value;");
    uint16_t exports_key = cf_string(cf, "exports");
    op_emit2(main_m, op_getstatic, cache_field);
    op_emit(main_m, op_dup);
    size_t null_jump = op_pos(main_m);
    op_emit2(main_m, op_ifnull, 0);
    op_emit2(main_m, op_ldc_w, exports_key);
    op_emit2(main_m, op_invokevirtual, get_idx);
    op_emit(main_m, op_areturn);
    size_t create_pos = op_pos(main_m);
    op_patch2(main_m, (uint16_t)(null_jump + 1),
              (uint16_t)(create_pos - null_jump));
    op_emit(main_m, op_pop);
    uint16_t obj_cls = cf_class(cf, "V6Object");
    uint16_t obj_ctor = cf_methodref(cf, "V6Object", "<init>", "()V");
    uint16_t set_idx =
        cf_methodref(cf, "V6Object", "set", "(Ljava/lang/String;LV6Value;)V");
    op_emit2(main_m, op_new, obj_cls);
    op_emit(main_m, op_dup);
    op_emit2(main_m, op_invokespecial, obj_ctor);
    op_emit(main_m, op_dup);
    op_emit2(main_m, op_putstatic, cache_field);
    op_emit(main_m, op_dup);
    op_emit2(main_m, op_ldc_w, exports_key);
    op_emit2(main_m, op_new, obj_cls);
    op_emit(main_m, op_dup);
    op_emit2(main_m, op_invokespecial, obj_ctor);
    emit_box_object_ref(&c);
    op_emit2(main_m, op_invokevirtual, set_idx);
    module_local_slot = c.next_local_slot++;
    emit_box_object_ref(&c);
    emit_var_declare(&c, module_local_slot);
    tok module_tok;
    module_tok.kind = tok_ident;
    module_tok.start = "module";
    module_tok.len = 6;
    module_tok.line = 0;
    module_tok.num = 0;
    add_local(&c, module_tok, module_local_slot, 0, 0);

    var_ref module_vr;
    module_vr.kind = var_local;
    module_vr.index = module_local_slot;
    emit_var_read_ref(&c, module_vr);
    op_emit2(main_m, op_ldc_w, exports_key);
    uint16_t getprop_idx_early =
        cf_methodref(cf, "V6Value", "getProp", "(Ljava/lang/String;)LV6Value;");
    op_emit2(main_m, op_invokevirtual, getprop_idx_early);
    exports_local_slot = c.next_local_slot++;
    emit_var_declare(&c, exports_local_slot);
    tok exports_tok;
    exports_tok.kind = tok_ident;
    exports_tok.start = "exports";
    exports_tok.len = 7;
    exports_tok.line = 0;
    exports_tok.num = 0;
    add_local(&c, exports_tok, exports_local_slot, 0, 0);
  } else if (!is_entry) {
    uint16_t cache_field =
        cf_fieldref(cf, this_class_name, "EXPORTS_CACHE", "LV6Object;");
    op_emit2(main_m, op_getstatic, cache_field);
    op_emit(main_m, op_dup);
    size_t null_jump = op_pos(main_m);
    op_emit2(main_m, op_ifnull, 0);
    op_emit(main_m, op_areturn);
    size_t create_pos = op_pos(main_m);
    op_patch2(main_m, (uint16_t)(null_jump + 1),
              (uint16_t)(create_pos - null_jump));
    op_emit(main_m, op_pop);
    uint16_t obj_cls = cf_class(cf, "V6Object");
    uint16_t obj_ctor = cf_methodref(cf, "V6Object", "<init>", "()V");
    op_emit2(main_m, op_new, obj_cls);
    op_emit(main_m, op_dup);
    op_emit2(main_m, op_invokespecial, obj_ctor);
    op_emit(main_m, op_dup);
    op_emit2(main_m, op_putstatic, cache_field);
    c.exports_slot = c.next_local_slot++;
    emit_astore(main_m, c.exports_slot);
  }

  prescan_decls(&c, src, 1);

  parser p;
  parser_init(&p, src);
  parse_program(&p, &c);

  if (is_entry) {
    uint16_t run_idx = cf_methodref(cf, "V6EventLoop", "run", "()V");
    op_emit2(main_m, op_invokestatic, run_idx);
    op_emit(main_m, op_return);
  } else if (is_cjs) {
    var_ref module_vr;
    module_vr.kind = var_local;
    module_vr.index = module_local_slot;
    emit_var_read_ref(&c, module_vr);
    uint16_t exports_key = cf_string(cf, "exports");
    op_emit2(main_m, op_ldc_w, exports_key);
    uint16_t getprop_idx =
        cf_methodref(cf, "V6Value", "getProp", "(Ljava/lang/String;)LV6Value;");
    op_emit2(main_m, op_invokevirtual, getprop_idx);
    op_emit(main_m, op_areturn);
  } else {
    for (int i = 0; i < exports_count; i++) {
      var_ref vr = resolve_var(&c, exports_list[i].local_name,
                               strlen(exports_list[i].local_name));
      if (vr.kind == var_not_found)
        continue;
      emit_aload(main_m, c.exports_slot);
      uint16_t key_idx = cf_string(cf, exports_list[i].export_key);
      op_emit2(main_m, op_ldc_w, key_idx);
      emit_var_read_ref(&c, vr);
      uint16_t set_idx =
          cf_methodref(cf, "V6Object", "set", "(Ljava/lang/String;LV6Value;)V");
      op_emit2(main_m, op_invokevirtual, set_idx);
    }
    emit_aload(main_m, c.exports_slot);
    op_emit(main_m, op_areturn);
  }
  (void)exports_local_slot;
  main_m->max_locals = c.next_local_slot;

  compile_result r;
  r.ok = p.had_error ? 0 : 1;
  r.line = p.err_line - prelude_lines;
  if (r.line < 1)
    r.line = 1;
  memcpy(r.message, p.err_msg, sizeof(r.message));
  free(src);
  return r;
}

compile_result compile_program(const char* src, class_file* cf,
                               const char* entry_path, module_ctx* modctx) {
  char dir[v6_max_path];
  if (entry_path) {
    path_dirname(entry_path, dir, sizeof(dir));
  } else {
    snprintf(dir, sizeof(dir), ".");
  }
  if (modctx)
    module_ctx_init(modctx);
  return compile_module_impl(cf, "Main", src, dir, modctx, 1, 0);
}
