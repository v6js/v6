#include "v6/parser.h"

#include "v6/module.h"
#include "v6/internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v6/expr.h"
#include "v6/calls.h"
#include "v6/primary.h"
#include "v6/scope.h"
#include "v6/stmt.h"

int is_logical_assign_op(tok_kind k) {
  return k == tok_amp_amp_eq || k == tok_pipe_pipe_eq ||
         k == tok_question_question_eq;
}

void emit_logical_assign_ident(parser* p, compiler* c, var_ref vr,
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

void emit_logical_assign_member(parser* p, compiler* c, tok_kind op) {
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

void parse_unary(parser* p, compiler* c) {
  if (match(p, tok_kw_delete)) {
    parse_delete_target(p, c);
    return;
  }
  if (match(p, tok_plus)) {
    parse_unary(p, c);
    emit_to_number(c);
    emit_box_tag(c, op_iconst_0);
    return;
  }
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
  if (match(p, tok_kw_void)) {
    parse_unary(p, c);
    op_emit(c->m, op_pop);
    emit_undef(c->cf, c->m);
    return;
  }
  if (match(p, tok_kw_typeof)) {
    if (check(p, tok_ident)) {
      lexer save_lex = p->lex;
      tok after = lex_next(&p->lex);
      p->lex = save_lex;
      int is_bare = after.kind != tok_dot && after.kind != tok_lbracket &&
                    after.kind != tok_lparen && after.kind != tok_template &&
                    after.kind != tok_plus_plus &&
                    after.kind != tok_minus_minus;
      var_ref probe = resolve_var(c, p->cur.start, p->cur.len);
      if (is_bare && probe.kind == var_not_found) {
        advance(p);
        emit_undef(c->cf, c->m);
      } else {
        parse_unary(p, c);
      }
    } else {
      parse_unary(p, c);
    }
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

void parse_expr(parser* p, compiler* c) {
  parse_ternary(p, c);
}

void parse_seq_expr(parser* p, compiler* c) {
  parse_expr(p, c);
  while (check(p, tok_comma)) {
    advance(p);
    op_emit(c->m, op_pop);
    parse_expr(p, c);
  }
}

int compile_expr(parser* p, compiler* c) {
  parse_expr(p, c);
  return p->had_error ? -1 : 0;
}
