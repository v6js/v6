#include "v6/parser.h"

#include "v6/module.h"
#include "v6/internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v6/calls.h"
#include "v6/expr.h"
#include "v6/literal.h"
#include "v6/primary.h"

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

void emit_args_array(parser* p, compiler* c) {
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

void emit_call_args_and_invoke(parser* p, compiler* c) {
  op_emit(c->m, op_swap);
  emit_args_array(p, c);
  uint16_t call_idx =
      cf_methodref(c->cf, "V6Value", "call", "(LV6Value;[LV6Value;)LV6Value;");
  op_emit2(c->m, op_invokevirtual, call_idx);
}

void compile_direct_call(parser* p, compiler* c, var_ref vr,
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

void compile_direct_new(parser* p, compiler* c, var_ref vr,
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

void parse_postfix(parser* p, compiler* c) {
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
      int is_bracket = check(p, tok_lbracket);
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
        expect(p, tok_rbracket);
      }

      if (is_bracket && check(p, tok_assign)) {
        advance(p);
        parse_expr(p, c);
        op_emit(c->m, op_dup_x2);
        uint16_t set_idx = cf_methodref(c->cf, "V6Value", "setIndexedOrProp",
                                        "(LV6Value;LV6Value;)V");
        op_emit2(c->m, op_invokevirtual, set_idx);
        goto patch_and_return;
      }

      if (is_bracket && !is_logical_assign_op(p->cur.kind) &&
          !check(p, tok_plus_eq) && !check(p, tok_minus_eq) &&
          !check(p, tok_star_eq) && !check(p, tok_slash_eq) &&
          !check(p, tok_percent_eq) && !check(p, tok_amp_eq) &&
          !check(p, tok_pipe_eq) && !check(p, tok_caret_eq) &&
          !check(p, tok_shl_eq) && !check(p, tok_shr_eq) &&
          !check(p, tok_ushr_eq) && !check(p, tok_plus_plus) &&
          !check(p, tok_minus_minus) && !check(p, tok_lparen) &&
          !check(p, tok_template)) {
        uint16_t get_idx = cf_methodref(c->cf, "V6Value", "getIndexedOrProp",
                                        "(LV6Value;)LV6Value;");
        op_emit2(c->m, op_invokevirtual, get_idx);
        continue;
      }

      if (is_bracket) {
        uint16_t tostring_idx =
            cf_methodref(c->cf, "V6Value", "toString", "()Ljava/lang/String;");
        op_emit2(c->m, op_invokevirtual, tostring_idx);
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
