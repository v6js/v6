#include "v6/internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int num_ident_is_param(num_fn_ctx* nf, tok name, int* out_idx) {
  for (int i = 0; i < nf->param_count; i++) {
    if (nf->param_lens[i] == name.len &&
        memcmp(nf->param_names[i], name.start, name.len) == 0) {
      *out_idx = i;
      return 1;
    }
  }
  return 0;
}

const char* find_num_shadow_fn(compiler* c, const char* name,
                                      size_t len, int* out_arity) {
  while (c) {
    for (int i = 0; i < c->param_count; i++) {
      if (c->params[i].len == len && memcmp(c->params[i].name, name, len) == 0)
        return NULL;
    }
    local* le = find_local_entry(c, name, len);
    if (le) {
      if (le->num_shadow_name)
        *out_arity = le->num_arity;
      return le->num_shadow_name;
    }
    c = c->parent;
  }
  return NULL;
}

void build_num_sig(char* out, int arity) {
  int pos = 0;
  out[pos++] = '(';
  for (int i = 0; i < arity; i++)
    out[pos++] = 'D';
  out[pos++] = ')';
  out[pos++] = 'D';
  out[pos] = '\0';
}

static int compile_num_expr(parser* p, class_file* cf, method* m,
                            num_fn_ctx* nf, int emit);

int compile_num_call_args(parser* p, class_file* cf, method* m,
                                 num_fn_ctx* nf, int emit,
                                 int expected_arity) {
  if (!check(p, tok_lparen))
    return 0;
  advance(p);
  int n = 0;
  if (!check(p, tok_rparen)) {
    for (;;) {
      if (!compile_num_expr(p, cf, m, nf, emit))
        return 0;
      n++;
      if (check(p, tok_comma)) {
        advance(p);
        continue;
      }
      break;
    }
  }
  if (!check(p, tok_rparen))
    return 0;
  advance(p);
  return n == expected_arity;
}

static int compile_num_primary(parser* p, class_file* cf, method* m,
                               num_fn_ctx* nf, int emit) {
  if (check(p, tok_num) && !p->cur.is_bigint) {
    double v = p->cur.num;
    advance(p);
    if (emit)
      emit_dconst_val(cf, m, v);
    return 1;
  }
  if (check(p, tok_lparen)) {
    advance(p);
    if (!compile_num_expr(p, cf, m, nf, emit))
      return 0;
    if (!check(p, tok_rparen))
      return 0;
    advance(p);
    return 1;
  }
  if (check(p, tok_ident)) {
    tok name = p->cur;
    int pidx;
    if (num_ident_is_param(nf, name, &pidx)) {
      advance(p);
      if (emit)
        emit_dload(m, nf->param_slots[pidx]);
      return 1;
    }
    int is_self = name.len == nf->self_len &&
                  memcmp(name.start, nf->self_name, name.len) == 0;
    const char* target_method;
    int target_arity;
    if (is_self) {
      target_method = nf->self_shadow_method;
      target_arity = nf->param_count;
    } else {
      target_arity = 0;
      target_method =
          find_num_shadow_fn(nf->sibling_scope, name.start, name.len,
                             &target_arity);
      if (!target_method)
        return 0;
    }
    advance(p);
    if (!compile_num_call_args(p, cf, m, nf, emit, target_arity))
      return 0;
    if (emit) {
      char sig[16];
      build_num_sig(sig, target_arity);
      uint16_t midx = cf_methodref(cf, nf->class_name, target_method, sig);
      op_emit2(m, op_invokestatic, midx);
    }
    return 1;
  }
  return 0;
}

static int compile_num_unary(parser* p, class_file* cf, method* m,
                             num_fn_ctx* nf, int emit) {
  if (check(p, tok_minus)) {
    advance(p);
    if (!compile_num_unary(p, cf, m, nf, emit))
      return 0;
    if (emit)
      op_emit(m, op_dneg);
    return 1;
  }
  return compile_num_primary(p, cf, m, nf, emit);
}

static int compile_num_mul(parser* p, class_file* cf, method* m,
                           num_fn_ctx* nf, int emit) {
  if (!compile_num_unary(p, cf, m, nf, emit))
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
    if (!compile_num_unary(p, cf, m, nf, emit))
      return 0;
    if (emit)
      op_emit(m, bop);
  }
  return 1;
}

static int compile_num_expr(parser* p, class_file* cf, method* m,
                            num_fn_ctx* nf, int emit) {
  if (!compile_num_mul(p, cf, m, nf, emit))
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
    if (!compile_num_mul(p, cf, m, nf, emit))
      return 0;
    if (emit)
      op_emit(m, bop);
  }
  return 1;
}

static int compile_num_cond(parser* p, class_file* cf, method* m,
                            num_fn_ctx* nf, int emit, size_t* out_false_jump) {
  if (!compile_num_expr(p, cf, m, nf, emit))
    return 0;
  uint8_t jump_op;
  if (check(p, tok_lt))
    jump_op = op_ifge;
  else if (check(p, tok_le))
    jump_op = op_ifgt;
  else if (check(p, tok_gt))
    jump_op = op_ifle;
  else if (check(p, tok_ge))
    jump_op = op_iflt;
  else if (check(p, tok_eq) || check(p, tok_eq_strict))
    jump_op = op_ifne;
  else if (check(p, tok_neq) || check(p, tok_neq_strict))
    jump_op = op_ifeq;
  else
    return 0;
  advance(p);
  if (!compile_num_expr(p, cf, m, nf, emit))
    return 0;
  if (emit) {
    op_emit(m, op_dcmpg);
    *out_false_jump = op_pos(m);
    op_emit2(m, jump_op, 0);
  }
  return 1;
}

static int compile_num_body(parser* p, class_file* cf, method* m,
                            num_fn_ctx* nf, int emit) {
  for (;;) {
    if (check(p, tok_kw_if)) {
      advance(p);
      if (!check(p, tok_lparen))
        return 0;
      advance(p);
      size_t false_jump = 0;
      if (!compile_num_cond(p, cf, m, nf, emit, &false_jump))
        return 0;
      if (!check(p, tok_rparen))
        return 0;
      advance(p);
      int had_brace = check(p, tok_lbrace);
      if (had_brace)
        advance(p);
      if (!check(p, tok_kw_return))
        return 0;
      advance(p);
      if (!compile_num_expr(p, cf, m, nf, emit))
        return 0;
      if (!check(p, tok_semi))
        return 0;
      advance(p);
      if (emit)
        op_emit(m, op_dreturn);
      if (had_brace) {
        if (!check(p, tok_rbrace))
          return 0;
        advance(p);
      }
      if (check(p, tok_kw_else))
        return 0;
      if (emit) {
        size_t after_pos = op_pos(m);
        op_patch2(m, (uint16_t)(false_jump + 1),
                  (uint16_t)(after_pos - false_jump));
      }
      continue;
    }
    if (check(p, tok_kw_return)) {
      advance(p);
      if (!compile_num_expr(p, cf, m, nf, emit))
        return 0;
      if (!check(p, tok_semi))
        return 0;
      advance(p);
      if (emit)
        op_emit(m, op_dreturn);
      return check(p, tok_rbrace);
    }
    return 0;
  }
}

int try_compile_num_shadow(compiler* c, tok fn_name,
                                  const char* params_start,
                                  char* out_shadow_name, int* out_arity) {
  num_fn_ctx nf;
  nf.self_name = fn_name.start;
  nf.self_len = fn_name.len;
  nf.sibling_scope = c;
  nf.class_name = c->this_class_name;
  nf.param_count = 0;

  parser probe;
  parser_init(&probe, params_start);

  if (!check(&probe, tok_lparen))
    return 0;
  advance(&probe);
  if (!check(&probe, tok_rparen)) {
    for (;;) {
      if (!check(&probe, tok_ident))
        return 0;
      if (nf.param_count >= v6_max_num_params)
        return 0;
      nf.param_names[nf.param_count] = probe.cur.start;
      nf.param_lens[nf.param_count] = probe.cur.len;
      nf.param_count++;
      advance(&probe);
      if (check(&probe, tok_comma)) {
        advance(&probe);
        continue;
      }
      break;
    }
  }
  if (!check(&probe, tok_rparen))
    return 0;
  advance(&probe);
  if (!check(&probe, tok_lbrace))
    return 0;
  advance(&probe);

  int id = (*c->lambda_counter)++;
  snprintf(nf.self_shadow_method, sizeof(nf.self_shadow_method), "numfn%d",
           id);

  if (!compile_num_body(&probe, NULL, NULL, &nf, 0))
    return 0;

  parser real;
  parser_init(&real, params_start);
  advance(&real);
  for (int i = 0; i < nf.param_count; i++) {
    advance(&real);
    if (check(&real, tok_comma))
      advance(&real);
  }
  advance(&real);
  advance(&real);

  char sig[16];
  build_num_sig(sig, nf.param_count);
  method* m = cf_method(c->cf, acc_static, nf.self_shadow_method, sig);
  m->max_stack = 64;
  m->max_locals = 0;
  for (int i = 0; i < nf.param_count; i++) {
    nf.param_slots[i] = (uint16_t)(m->max_locals);
    m->max_locals += 2;
  }

  if (!compile_num_body(&real, c->cf, m, &nf, 1))
    return 0;

  strcpy(out_shadow_name, nf.self_shadow_method);
  *out_arity = nf.param_count;
  return 1;
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
  if (check(p, tok_tilde)) {
    advance(p);
    if (!compile_raw_unary(p, c, rl, emit, mode))
      return 0;
    if (emit) {
      emit_to_int32_raw(c->cf, c->m);
      op_emit(c->m, op_iconst_m1);
      op_emit(c->m, op_ixor);
      op_emit(c->m, op_i2d);
    }
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

static int compile_raw_add(parser* p, compiler* c, raw_loop_ctx* rl, int emit,
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

static int compile_raw_shift(parser* p, compiler* c, raw_loop_ctx* rl,
                             int emit, int mode) {
  if (!compile_raw_add(p, c, rl, emit, mode))
    return 0;
  int any = 0;
  for (;;) {
    uint8_t bop;
    if (check(p, tok_shl))
      bop = op_ishl;
    else if (check(p, tok_shr))
      bop = op_ishr;
    else if (check(p, tok_ushr))
      bop = op_iushr;
    else
      break;
    advance(p);
    if (!any && emit) {
      emit_to_int32_raw(c->cf, c->m);
      any = 1;
    }
    if (!compile_raw_add(p, c, rl, emit, mode))
      return 0;
    if (emit) {
      emit_to_int32_raw(c->cf, c->m);
      emit_iconst(c->m, 31);
      op_emit(c->m, op_iand);
      op_emit(c->m, bop);
    }
  }
  if (any && emit)
    op_emit(c->m, op_i2d);
  return 1;
}

static int compile_raw_bitand(parser* p, compiler* c, raw_loop_ctx* rl,
                              int emit, int mode) {
  if (!compile_raw_shift(p, c, rl, emit, mode))
    return 0;
  int any = 0;
  while (check(p, tok_amp)) {
    advance(p);
    if (!any && emit) {
      emit_to_int32_raw(c->cf, c->m);
      any = 1;
    }
    if (!compile_raw_shift(p, c, rl, emit, mode))
      return 0;
    if (emit) {
      emit_to_int32_raw(c->cf, c->m);
      op_emit(c->m, op_iand);
    }
  }
  if (any && emit)
    op_emit(c->m, op_i2d);
  return 1;
}

static int compile_raw_bitxor(parser* p, compiler* c, raw_loop_ctx* rl,
                              int emit, int mode) {
  if (!compile_raw_bitand(p, c, rl, emit, mode))
    return 0;
  int any = 0;
  while (check(p, tok_caret)) {
    advance(p);
    if (!any && emit) {
      emit_to_int32_raw(c->cf, c->m);
      any = 1;
    }
    if (!compile_raw_bitand(p, c, rl, emit, mode))
      return 0;
    if (emit) {
      emit_to_int32_raw(c->cf, c->m);
      op_emit(c->m, op_ixor);
    }
  }
  if (any && emit)
    op_emit(c->m, op_i2d);
  return 1;
}

static int compile_raw_bitor(parser* p, compiler* c, raw_loop_ctx* rl,
                             int emit, int mode) {
  if (!compile_raw_bitxor(p, c, rl, emit, mode))
    return 0;
  int any = 0;
  while (check(p, tok_pipe)) {
    advance(p);
    if (!any && emit) {
      emit_to_int32_raw(c->cf, c->m);
      any = 1;
    }
    if (!compile_raw_bitxor(p, c, rl, emit, mode))
      return 0;
    if (emit) {
      emit_to_int32_raw(c->cf, c->m);
      op_emit(c->m, op_ior);
    }
  }
  if (any && emit)
    op_emit(c->m, op_i2d);
  return 1;
}

static int compile_raw_expr(parser* p, compiler* c, raw_loop_ctx* rl, int emit,
                            int mode) {
  return compile_raw_bitor(p, c, rl, emit, mode);
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

int try_compile_raw_for(parser* p, compiler* c) {
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
