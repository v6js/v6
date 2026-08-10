#include "v6/parser.h"

#include "v6/module.h"
#include "v6/internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v6/closures.h"
#include "v6/expr.h"
#include "v6/pattern.h"
#include "v6/scope.h"
#include "v6/stmt.h"

void emit_wrap_generator(compiler* c) {
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

void emit_wrap_async(compiler* c) {
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

void emit_wrap_async_generator(compiler* c) {
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
  if (fc->param_count < v6_max_params) {
    fc->params[fc->param_count].name = name.start;
    fc->params[fc->param_count].len = name.len;
    fc->params[fc->param_count].slot = slot;
    fc->param_count++;
  }
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
  if (fc->param_count < v6_max_params) {
    fc->params[fc->param_count].name = name.start;
    fc->params[fc->param_count].len = name.len;
    fc->params[fc->param_count].slot = slot;
    fc->param_count++;
  }
}

static void parse_function_params(parser* p, compiler* fc) {
  expect(p, tok_lparen);
  int idx = 0;
  if (!check(p, tok_rparen)) {
    for (;;) {
      if (match(p, tok_ellipsis)) {
        if (!is_contextual_ident(p->cur.kind)) {
          error_at(p, "expected identifier");
          break;
        }
        advance(p);
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

      if (!is_contextual_ident(p->cur.kind)) {
        error_at(p, "expected identifier");
        break;
      }
      advance(p);
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

static void bind_arguments(compiler* fc) {
  uint16_t slot = fc->next_local_slot++;
  emit_aload(fc->m, 2);
  emit_iconst(fc->m, 0);
  uint16_t restargs_idx = cf_methodref(fc->cf, "V6Object", "restFromArgs",
                                       "([LV6Value;I)LV6Array;");
  op_emit2(fc->m, op_invokestatic, restargs_idx);
  emit_box_object_ref(fc);
  emit_var_declare(fc, slot);
  tok at;
  at.kind = tok_ident;
  at.start = "arguments";
  at.len = 9;
  at.line = 0;
  at.num = 0;
  add_local(fc, at, slot, 0, 0);
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

void compile_closure_value(parser* p, compiler* c, int is_arrow,
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
  fc.locals = malloc(sizeof(local) * v6_initial_locals);
  fc.local_count = 0;
  fc.local_cap = v6_initial_locals;
  fc.scratch_slot = 3;
  fc.next_local_slot = 5;
  fc.upvalues = malloc(sizeof(upvalue) * v6_initial_upvalues);
  fc.upvalue_count = 0;
  fc.upvalue_cap = v6_initial_upvalues;
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

  if (!is_arrow)
    bind_arguments(&fc);

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
  if (!is_contextual_ident(p->cur.kind)) {
    error_at(p, "expected identifier");
    return;
  }
  advance(p);
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

void parse_function_decl(parser* p, compiler* c) {
  if (c->brace_depth == 0) {
    skip_function_tokens(p);
    return;
  }

  int is_gen = match(p, tok_star);
  if (!is_contextual_ident(p->cur.kind)) {
    error_at(p, "expected identifier");
    return;
  }
  advance(p);
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
