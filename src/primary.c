#include "v6/parser.h"

#include "v6/module.h"
#include "v6/internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v6/primary.h"
#include "v6/calls.h"
#include "v6/class.h"
#include "v6/closures.h"
#include "v6/expr.h"
#include "v6/import.h"
#include "v6/literal.h"
#include "v6/pattern.h"
#include "v6/scope.h"

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

static void parse_new(parser* p, compiler* c) {
  if (match(p, tok_kw_this)) {
    var_ref this_vr = resolve_var(c, "this", 4);
    if (this_vr.kind == var_not_found) {
      error_at(p, "'this' outside function");
      return;
    }
    emit_var_read_ref(c, this_vr);
  } else {
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
  }

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

static void parse_ident_primary(parser* p, compiler* c, tok name) {
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

  if (check(p, tok_assign) || check(p, tok_plus_eq) || check(p, tok_minus_eq) ||
      check(p, tok_star_eq) || check(p, tok_slash_eq) ||
      check(p, tok_percent_eq) || check(p, tok_amp_eq) ||
      check(p, tok_pipe_eq) || check(p, tok_caret_eq) || check(p, tok_shl_eq) ||
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
      uint8_t bop =
          op == tok_minus_eq
              ? op_dsub
              : (op == tok_star_eq ? op_dmul
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
    emit_throw_reference_error(c, name.start, name.len);
    return;
  }
  if (check(p, tok_lparen)) {
    int shadow_arity = 0;
    const char* shadow_name =
        find_num_shadow_fn(c, name.start, name.len, &shadow_arity);
    if (shadow_name) {
      lexer saved_lex = p->lex;
      tok saved_cur = p->cur;
      tok saved_prev = p->prev;
      num_fn_ctx nf;
      nf.param_count = 0;
      nf.self_name = "";
      nf.self_len = 0;
      nf.sibling_scope = c;
      nf.class_name = c->this_class_name;
      if (compile_num_call_args(p, c->cf, c->m, &nf, 0, shadow_arity)) {
        p->lex = saved_lex;
        p->cur = saved_cur;
        p->prev = saved_prev;
        if (compile_num_call_args(p, c->cf, c->m, &nf, 1, shadow_arity)) {
          char sig[16];
          build_num_sig(sig, shadow_arity);
          uint16_t midx =
              cf_methodref(c->cf, c->this_class_name, shadow_name, sig);
          op_emit2(c->m, op_invokestatic, midx);
          op_emit2(c->m, op_invokestatic, value_num_method(c->cf));
          return;
        }
      }
      p->lex = saved_lex;
      p->cur = saved_cur;
      p->prev = saved_prev;
    }
    const char* lambda_name = find_direct_fn(c, name.start, name.len);
    if (lambda_name) {
      compile_direct_call(p, c, vr, lambda_name);
      return;
    }
  }
  emit_var_read_ref(c, vr);
}

void parse_primary(parser* p, compiler* c) {
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
    op_emit2(c->m, op_ldc2_w, idx);
    op_emit2(c->m, op_invokestatic, value_num_method(c->cf));
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
    var_ref undef_vr = resolve_var(c, "undefined", 9);
    if (undef_vr.kind != var_not_found) {
      emit_var_read_ref(c, undef_vr);
      return;
    }
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
      if (is_contextual_ident(p->cur.kind))
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
    advance(p);
    parse_ident_primary(p, c, p->prev);
    return;
  }

  if (match(p, tok_kw_function)) {
    int is_gen = match(p, tok_star);
    if (is_contextual_ident(p->cur.kind))
      advance(p);
    compile_closure_value(p, c, 0, 1, NULL);
    if (is_gen)
      emit_wrap_generator(c);
    return;
  }

  if (match(p, tok_kw_class)) {
    parse_class_decl(p, c, 1);
    return;
  }

  if (check(p, tok_ident) && peek_is_arrow(p)) {
    compile_closure_value(p, c, 1, 0, NULL);
    return;
  }

  if (is_contextual_ident(p->cur.kind)) {
    advance(p);
    parse_ident_primary(p, c, p->prev);
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
    parse_seq_expr(p, c);
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
    lexer regex_lex = p->lex;
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
  emit_undef(c->cf, c->m);
}

void parse_delete_target(parser* p, compiler* c) {
  parse_primary(p, c);
  for (;;) {
    if (match(p, tok_dot)) {
      if (!match_property_name(p)) {
        error_at(p, "expected property name");
        return;
      }
      char* key = dup_tok(p->prev);
      uint16_t key_idx = cf_string(c->cf, key);
      free(key);
      op_emit2(c->m, op_ldc_w, key_idx);
      if (check(p, tok_dot) || check(p, tok_lbracket)) {
        uint16_t get_idx = cf_methodref(c->cf, "V6Value", "getProp",
                                        "(Ljava/lang/String;)LV6Value;");
        op_emit2(c->m, op_invokevirtual, get_idx);
        continue;
      }
      uint16_t del_idx =
          cf_methodref(c->cf, "V6Value", "deleteProp", "(Ljava/lang/String;)Z");
      op_emit2(c->m, op_invokevirtual, del_idx);
      op_emit(c->m, op_i2d);
      emit_box_bool(c);
      return;
    }
    if (match(p, tok_lbracket)) {
      parse_expr(p, c);
      uint16_t tostring_idx =
          cf_methodref(c->cf, "V6Value", "toString", "()Ljava/lang/String;");
      op_emit2(c->m, op_invokevirtual, tostring_idx);
      if (!expect(p, tok_rbracket))
        return;
      if (check(p, tok_dot) || check(p, tok_lbracket)) {
        uint16_t get_idx = cf_methodref(c->cf, "V6Value", "getProp",
                                        "(Ljava/lang/String;)LV6Value;");
        op_emit2(c->m, op_invokevirtual, get_idx);
        continue;
      }
      uint16_t del_idx =
          cf_methodref(c->cf, "V6Value", "deleteProp", "(Ljava/lang/String;)Z");
      op_emit2(c->m, op_invokevirtual, del_idx);
      op_emit(c->m, op_i2d);
      emit_box_bool(c);
      return;
    }
    op_emit(c->m, op_pop);
    emit_const_singleton(c->cf, c->m, "TRUE");
    return;
  }
}
