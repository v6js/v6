#include "v6/parser.h"

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

static void emit_const_singleton(class_file* cf, method* m,
                                 const char* field) {
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

static char* dup_tok(tok t) {
  char* s = malloc(t.len + 1);
  memcpy(s, t.start, t.len);
  s[t.len] = '\0';
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
static void parse_object_literal(parser* p, compiler* c);
static void parse_array_literal(parser* p, compiler* c);
static void parse_primary(parser* p, compiler* c);
static void compile_closure_value(parser* p, compiler* c, int is_arrow,
                                  int parens_params, char* out_lambda_name);

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

static void parse_object_entry(parser* p, compiler* c) {
  if (match(p, tok_ellipsis)) {
    op_emit(c->m, op_dup);
    parse_expr(p, c);
    uint16_t spread_idx =
        cf_methodref(c->cf, "V6Object", "spreadFrom", "(LV6Value;)V");
    op_emit2(c->m, op_invokevirtual, spread_idx);
    return;
  }

  char* key;
  if (check(p, tok_str)) {
    tok t = p->cur;
    advance(p);
    key = decode_string(t);
  } else if (match_property_name(p)) {
    key = dup_tok(p->prev);
  } else {
    return;
  }
  if (!expect(p, tok_colon)) {
    free(key);
    return;
  }

  uint16_t key_idx = cf_string(c->cf, key);
  free(key);

  op_emit(c->m, op_dup);
  op_emit2(c->m, op_ldc_w, key_idx);
  parse_expr(p, c);
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
  uint16_t ref_idx = cf_methodref(c->cf, "V6Value", "ref", "()Ljava/lang/Object;");
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
  uint16_t direct_idx = cf_methodref(c->cf, "Main", lambda_name,
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
  op_emit(c->m, op_dup);
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

  uint16_t obj_ctor_idx = cf_methodref(c->cf, "V6Object", "<init>", "()V");
  op_emit2(c->m, op_new, object_class(c->cf));
  op_emit(c->m, op_dup);
  op_emit2(c->m, op_invokespecial, obj_ctor_idx);
  uint16_t inst_slot = c->next_local_slot++;
  emit_astore(c->m, inst_slot);

  uint16_t proto_str = cf_string(c->cf, "prototype");
  uint16_t get_idx = cf_methodref(c->cf, "V6Object", "get",
                                  "(Ljava/lang/String;)LV6Value;");
  uint16_t setprotoval_idx =
      cf_methodref(c->cf, "V6Object", "setProtoFromValue", "(LV6Value;)V");
  emit_aload(c->m, inst_slot);
  emit_aload(c->m, cls_obj_slot);
  op_emit2(c->m, op_ldc_w, proto_str);
  op_emit2(c->m, op_invokevirtual, get_idx);
  op_emit2(c->m, op_invokevirtual, setprotoval_idx);

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
  uint16_t direct_idx = cf_methodref(c->cf, "Main", lambda_name,
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
  for (;;) {
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
        return;
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
    } else if (check(p, tok_lparen)) {
      advance(p);
      emit_insert_undefined_this(c);
      emit_call_args_and_invoke(p, c);
    } else {
      break;
    }
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

static void parse_new(parser* p, compiler* c) {
  if (!expect(p, tok_ident))
    return;
  tok name = p->prev;
  var_ref vr = resolve_var(c, name.start, name.len);
  if (vr.kind == var_not_found) {
    error_at(p, "undeclared variable");
    return;
  }

  const char* lambda_name = find_direct_fn(c, name.start, name.len);
  if (lambda_name) {
    compile_direct_new(p, c, vr, lambda_name);
    return;
  }

  emit_var_read_ref(c, vr);

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

static void parse_primary(parser* p, compiler* c) {
  if (match(p, tok_kw_new)) {
    parse_new(p, c);
    return;
  }

  if (match(p, tok_kw_super)) {
    parse_super(p, c);
    return;
  }

  if (match(p, tok_num)) {
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

  if (match(p, tok_kw_function)) {
    if (check(p, tok_ident))
      advance(p);
    compile_closure_value(p, c, 0, 1, NULL);
    return;
  }

  if (check(p, tok_ident) && peek_is_arrow(p)) {
    compile_closure_value(p, c, 1, 0, NULL);
    return;
  }

  if (match(p, tok_ident)) {
    tok name = p->prev;

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

  error_at(p, "expected expression");
  advance(p);
}

static void parse_unary(parser* p, compiler* c) {
  if (match(p, tok_minus)) {
    parse_unary(p, c);
    emit_to_number(c);
    op_emit(c->m, op_dneg);
    emit_box_tag(c, op_iconst_0);
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

static void parse_mul(parser* p, compiler* c) {
  parse_unary(p, c);
  while (check(p, tok_star) || check(p, tok_slash) || check(p, tok_percent)) {
    tok_kind k = p->cur.kind;
    advance(p);
    emit_to_number(c);
    parse_unary(p, c);
    emit_to_number(c);
    uint8_t op = k == tok_star ? op_dmul : (k == tok_slash ? op_ddiv : op_drem);
    op_emit(c->m, op);
    emit_box_tag(c, op_iconst_0);
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
      emit_to_number(c);
      parse_mul(p, c);
      emit_to_number(c);
      op_emit(c->m, op_dsub);
      emit_box_tag(c, op_iconst_0);
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
  while (check(p, tok_lt) || check(p, tok_gt) || check(p, tok_le) ||
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

static void parse_ternary(parser* p, compiler* c) {
  parse_or(p, c);
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
    tok target_name = key_name;
    if (match(p, tok_colon)) {
      if (!expect(p, tok_ident))
        return;
      target_name = p->prev;
    }

    char* keystr = dup_tok(key_name);
    uint16_t key_idx = cf_string(c->cf, keystr);
    free(keystr);

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
  c->continue_depth++;
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
    if (rl->accums[i].len == len &&
        memcmp(rl->accums[i].name, name, len) == 0)
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

static int compile_raw_expr(parser* p, compiler* c, raw_loop_ctx* rl,
                            int emit, int mode);

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

static int compile_raw_unary(parser* p, compiler* c, raw_loop_ctx* rl,
                             int emit, int mode) {
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

static int compile_raw_expr(parser* p, compiler* c, raw_loop_ctx* rl,
                            int emit, int mode) {
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
    uint16_t push_idx =
        cf_methodref(c->cf, "V6Object", "push", "(LV6Value;)V");
    op_emit2(c->m, op_invokevirtual, push_idx);
    size_t end_jump = op_pos(c->m);
    op_emit2(c->m, op_goto, 0);

    size_t slow_pos = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(slow_jump + 1),
              (uint16_t)(slow_pos - slow_jump));

    uint16_t push_str = cf_string(c->cf, "push");
    uint16_t getprop_idx = cf_methodref(c->cf, "V6Value", "getProp",
                                        "(Ljava/lang/String;)LV6Value;");
    uint16_t call_idx =
        cf_methodref(c->cf, "V6Value", "call", "(LV6Value;[LV6Value;)LV6Value;");
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
    op_patch2(c->m, (uint16_t)(body_jump + 1), (uint16_t)(body_pos - body_jump));
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
  expect(p, tok_lparen);

  if (try_compile_raw_for(p, c)) {
    return;
  }

  int saved_count = c->local_count;

  if (match(p, tok_kw_var) || match(p, tok_kw_let) || match(p, tok_kw_const)) {
    tok_kind kind = p->prev.kind;
    if (!expect(p, tok_ident))
      return;
    tok name = p->prev;
    if (match(p, tok_kw_in) || match(p, tok_kw_of)) {
      int is_of = p->prev.kind == tok_kw_of;
      if (is_of)
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

static void bind_param(compiler* fc, tok name, int idx) {
  uint16_t slot = fc->next_local_slot++;
  emit_aload(fc->m, 2);
  emit_iconst(fc->m, idx);
  uint16_t argat_idx =
      cf_methodref(fc->cf, "V6Value", "argAt", "([LV6Value;I)LV6Value;");
  op_emit2(fc->m, op_invokestatic, argat_idx);
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
      if (!expect(p, tok_ident))
        break;
      bind_param(fc, p->prev, idx);
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
  fc.box_locals = compile_body_has_closures(p, parens_params, is_arrow);

  if (parens_params) {
    parse_function_params(p, &fc);
  } else if (expect(p, tok_ident)) {
    bind_param(&fc, p->prev, 0);
  }

  if (!is_arrow)
    bind_this(&fc);

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
  uint16_t main_cls_idx = cf_class(c->cf, "Main");
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

  if (!expect(p, tok_ident))
    return;
  tok name = p->prev;
  uint16_t slot = c->next_local_slot++;
  emit_undef(c->cf, c->m);
  emit_var_declare(c, slot);
  add_local(c, name, slot, 0, 0);
  compile_closure_value(p, c, 0, 1, NULL);
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

  while (!check(p, tok_rbrace) && !check(p, tok_eof)) {
    int is_static = match(p, tok_kw_static);
    if (!match_property_name(p))
      break;
    tok member_name = p->prev;
    int is_ctor = !is_static && member_name.len == 11 &&
                  memcmp(member_name.start, "constructor", 11) == 0;

    if (is_ctor) {
      emit_aload(c->m, cls_tmp);
      ctor_lambda_name = malloc(24);
      compile_closure_value(p, c, 0, 1, ctor_lambda_name);
      uint16_t ascall_idx =
          cf_methodref(c->cf, "V6Value", "asCallable", "()LV6Callable;");
      op_emit2(c->m, op_invokevirtual, ascall_idx);
      uint16_t ctor_field =
          cf_fieldref(c->cf, "V6Class", "ctor", "LV6Callable;");
      op_emit2(c->m, op_putfield, ctor_field);
    } else {
      uint16_t target_slot = is_static ? cls_tmp : proto_tmp;
      char* mkey = dup_tok(member_name);
      uint16_t mkey_idx = cf_string(c->cf, mkey);
      free(mkey);
      emit_aload(c->m, target_slot);
      op_emit2(c->m, op_ldc_w, mkey_idx);
      compile_closure_value(p, c, 0, 1, NULL);
      op_emit2(c->m, op_invokevirtual, set_idx);
    }
  }
  expect(p, tok_rbrace);

  c->super_name = saved_super_name;
  c->super_len = saved_super_len;

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

static void parse_stmt(parser* p, compiler* c) {
  if (match(p, tok_lbrace)) {
    parse_block(p, c);
    return;
  }

  if (match(p, tok_kw_class)) {
    parse_class_decl(p, c);
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
    if (c->break_depth == 0) {
      error_at(p, "'break' outside loop or switch");
    } else {
      break_ctx* bc = &c->breaks[c->break_depth - 1];
      bc->jumps[bc->count++] = op_pos(c->m);
      op_emit2(c->m, op_goto, 0);
    }
    expect(p, tok_semi);
    return;
  }

  if (match(p, tok_kw_continue)) {
    if (c->continue_depth == 0) {
      error_at(p, "'continue' outside loop");
    } else {
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
    op_emit(c->m, op_areturn);
    return;
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
    } else if (hoist_functions && depth == 0 && t.kind == tok_kw_function) {
      const char* body_start = lx.cur;
      tok name = lex_next(&lx);
      if (name.kind == tok_ident) {
        prescan_hoist_one(c, name);
        if (pending_count < v6_max_pending_fns)
          pending_fns[pending_count++] = body_start;
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
    if (!check(&fp, tok_ident))
      continue;
    tok name = fp.cur;
    advance(&fp);
    char* lambda_name = malloc(24);
    if (!name_reassigned_in_scope(src, name.start, name.len)) {
      local* le = find_local_entry(c, name.start, name.len);
      if (le) {
        le->direct_fn = 1;
        le->fn_method_name = lambda_name;
      }
    }
    compile_closure_value(&fp, c, 0, 1, lambda_name);
    var_ref vr = resolve_var(c, name.start, name.len);
    if (vr.kind != var_not_found) {
      emit_var_write_ref(c, vr);
      op_emit(c->m, op_pop);
    }
  }
}

static void parse_program(parser* p, compiler* c) {
  while (!check(p, tok_eof)) {
    if (match(p, tok_kw_function))
      parse_function_decl(p, c);
    else
      parse_stmt(p, c);
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

compile_result compile_program(const char* src, class_file* cf) {
  method* main_m =
      cf_method(cf, acc_public | acc_static, "main", "([Ljava/lang/String;)V");
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

  prescan_decls(&c, src, 1);

  parser p;
  parser_init(&p, src);
  parse_program(&p, &c);

  op_emit(main_m, op_return);
  main_m->max_locals = c.next_local_slot;

  compile_result r;
  r.ok = p.had_error ? 0 : 1;
  r.line = p.err_line;
  memcpy(r.message, p.err_msg, sizeof(r.message));
  return r;
}
