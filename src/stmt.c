#include "v6/parser.h"

#include "v6/module.h"
#include "v6/internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v6/stmt.h"
#include "v6/class.h"
#include "v6/closures.h"
#include "v6/expr.h"
#include "v6/import.h"
#include "v6/pattern.h"
#include "v6/scope.h"

void parse_block(parser* p, compiler* c) {
  int saved_count = c->local_count;
  c->brace_depth++;
  while (!check(p, tok_rbrace) && !check(p, tok_eof) && !p->had_error)
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
  parse_seq_expr(p, c);
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
  parse_seq_expr(p, c);
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

static void skip_do_while_body(parser* p) {
  if (check(p, tok_lbrace)) {
    advance(p);
    skip_balanced(p, tok_lbrace, tok_rbrace);
    return;
  }
  int save_auto_regex = p->lex.auto_regex;
  p->lex.auto_regex = 1;
  int depth = 0;
  for (;;) {
    if (check(p, tok_eof)) {
      p->lex.auto_regex = save_auto_regex;
      return;
    }
    if (check(p, tok_lparen) || check(p, tok_lbracket) ||
        check(p, tok_lbrace)) {
      depth++;
      advance(p);
    } else if (check(p, tok_rparen) || check(p, tok_rbracket)) {
      if (depth == 0) {
        p->lex.auto_regex = save_auto_regex;
        return;
      }
      depth--;
      advance(p);
    } else if (check(p, tok_rbrace)) {
      if (depth == 0) {
        p->lex.auto_regex = save_auto_regex;
        return;
      }
      depth--;
      advance(p);
      if (depth == 0 && !check(p, tok_kw_else) && !check(p, tok_kw_catch) &&
          !check(p, tok_kw_finally)) {
        p->lex.auto_regex = save_auto_regex;
        return;
      }
    } else if (depth == 0 && check(p, tok_semi)) {
      advance(p);
      p->lex.auto_regex = save_auto_regex;
      return;
    } else {
      advance(p);
    }
  }
}

static void parse_do_while(parser* p, compiler* c) {
  const char* body_src = p->cur.start;
  skip_do_while_body(p);

  if (!expect(p, tok_kw_while))
    return;
  if (!expect(p, tok_lparen))
    return;
  const char* cond_src = p->cur.start;
  skip_balanced(p, tok_lparen, tok_rparen);
  expect_semi(p);

  size_t entry_jump = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);

  size_t cond_pos = op_pos(c->m);
  {
    parser cp;
    parser_init(&cp, cond_src);
    parse_expr(&cp, c);
  }
  emit_truthy(c);
  size_t exit_jump = op_pos(c->m);
  op_emit2(c->m, op_ifeq, 0);
  size_t loop_back_jump = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);

  size_t body_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(entry_jump + 1),
            (uint16_t)(body_pos - entry_jump));
  op_patch2(c->m, (uint16_t)(loop_back_jump + 1),
            (uint16_t)(body_pos - loop_back_jump));

  push_loop(c, cond_pos);
  {
    parser bp;
    parser_init(&bp, body_src);
    parse_stmt(&bp, c);
  }
  size_t body_to_cond = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);
  op_patch2(c->m, (uint16_t)(body_to_cond + 1),
            (uint16_t)(cond_pos - body_to_cond));

  size_t end_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(exit_jump + 1), (uint16_t)(end_pos - exit_jump));
  pop_loop(c, end_pos);
}

static void parse_for_in(parser* p, compiler* c, tok_kind kind, tok name) {
  parse_seq_expr(p, c);
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
    var_vr.index = next_declared_slot(c);
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
    var_vr.index = next_declared_slot(c);
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
    var_vr.index = next_declared_slot(c);
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
  } else if (is_contextual_ident(p->cur.kind)) {
    lexer save_lex2 = p->lex;
    tok save_cur2 = p->cur;
    tok save_prev2 = p->prev;
    tok name = p->cur;
    advance(p);
    if (match(p, tok_kw_in) || match(p, tok_kw_of)) {
      int is_of = p->prev.kind == tok_kw_of;
      if (is_of && is_await)
        parse_for_await_of(p, c, tok_kw_var, name);
      else if (is_of)
        parse_for_of(p, c, tok_kw_var, name);
      else
        parse_for_in(p, c, tok_kw_var, name);
      for (int i = saved_count; i < c->local_count; i++) {
        if (!c->locals[i].is_var)
          c->locals[i].dead = 1;
      }
      return;
    }
    p->lex = save_lex2;
    p->cur = save_cur2;
    p->prev = save_prev2;
    parse_seq_expr(p, c);
    op_emit(c->m, op_pop);
  } else if (!check(p, tok_semi)) {
    parse_seq_expr(p, c);
    op_emit(c->m, op_pop);
  }
  expect(p, tok_semi);

  size_t cond_pos = op_pos(c->m);
  int has_cond = !check(p, tok_semi);
  size_t exit_jump = 0;
  if (has_cond) {
    parse_seq_expr(p, c);
    emit_truthy(c);
    exit_jump = op_pos(c->m);
    op_emit2(c->m, op_ifeq, 0);
  }
  expect(p, tok_semi);

  size_t body_jump = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);

  size_t inc_pos = op_pos(c->m);
  if (!check(p, tok_rparen)) {
    parse_seq_expr(p, c);
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
  parse_seq_expr(p, c);
  expect(p, tok_rparen);
  expect(p, tok_lbrace);

  emit_astore(c->m, c->scratch_slot);

  c->breaks[c->break_depth].count = 0;
  c->break_depth++;

  size_t prev_case_jump = 0;
  int have_prev_case_jump = 0;
  int default_seen = 0;

  while (!check(p, tok_rbrace) && !check(p, tok_eof) && !p->had_error) {
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
      uint16_t err_slot = next_declared_slot(c);
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

void parse_stmt(parser* p, compiler* c) {
  if (match(p, tok_semi))
    return;

  if (match(p, tok_lbrace)) {
    parse_block(p, c);
    return;
  }

  if (match(p, tok_kw_function)) {
    parse_function_decl(p, c);
    return;
  }

  if (check(p, tok_kw_async)) {
    lexer save_lex_fn = p->lex;
    tok save_cur_fn = p->cur;
    tok save_prev_fn = p->prev;
    advance(p);
    if (match(p, tok_kw_function)) {
      parse_function_decl(p, c);
      return;
    }
    p->lex = save_lex_fn;
    p->cur = save_cur_fn;
    p->prev = save_prev_fn;
  }

  if (match(p, tok_kw_class)) {
    parse_class_decl(p, c, 0);
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

  if (match(p, tok_kw_do)) {
    parse_do_while(p, c);
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
    if (check(p, tok_ident) && p->cur.line == p->prev.line) {
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
    expect_semi(p);
    return;
  }

  if (match(p, tok_kw_continue)) {
    if (check(p, tok_ident) && p->cur.line == p->prev.line) {
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
    expect_semi(p);
    return;
  }

  if (match(p, tok_kw_throw)) {
    parse_expr(p, c);
    expect_semi(p);
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
    expect_semi(p);
    return;
  }

  if (match(p, tok_kw_return)) {
    if (check(p, tok_semi) || check(p, tok_rbrace) || check(p, tok_eof) ||
        p->cur.line > p->prev.line)
      emit_undef(c->cf, c->m);
    else
      parse_seq_expr(p, c);
    expect_semi(p);
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
      expect_semi(p);
      return;
    }
    p->lex = save_lex;
    p->cur = save_cur;
    p->prev = save_prev;
  }

  parse_seq_expr(p, c);
  op_emit(c->m, op_pop);
  expect_semi(p);
}

void parse_program(parser* p, compiler* c) {
  while (!check(p, tok_eof) && !p->had_error) {
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
