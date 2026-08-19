#include "v6/ast_hoist.h"

#include "v6/ast_codegen.h"
#include "v6/closures.h"
#include "v6/scope.h"

#include <stdlib.h>
#include <string.h>

#define ast_hoist_max_pending_fns 4096

static void hoist_name(compiler* c, const char* name, size_t len, int is_var,
                       int is_const) {
  local* le = find_local_entry(c, name, len);
  if (le) {
    le->is_var = is_var;
    le->is_const = is_const;
    return;
  }
  uint16_t slot = next_declared_slot(c);
  emit_undef(c->cf, c->m);
  emit_var_declare(c, slot);
  tok t;
  memset(&t, 0, sizeof(t));
  t.kind = tok_ident;
  t.start = name;
  t.len = len;
  add_local(c, t, slot, is_var, is_const);
}

static void hoist_pattern_names(compiler* c, ast_node* pat, int is_var,
                                int is_const) {
  if (!pat)
    return;
  switch (pat->kind) {
  case ast_pat_ident:
    hoist_name(c, pat->str, pat->str_len, is_var, is_const);
    break;
  case ast_pat_assign:
    hoist_pattern_names(c, pat->a, is_var, is_const);
    break;
  case ast_pat_rest:
    hoist_pattern_names(c, pat->a, is_var, is_const);
    break;
  case ast_pat_array:
    for (int i = 0; i < pat->list.len; i++)
      hoist_pattern_names(c, pat->list.items[i], is_var, is_const);
    break;
  case ast_pat_object:
    for (int i = 0; i < pat->props.len; i++)
      hoist_pattern_names(c, pat->props.items[i].value, is_var, is_const);
    break;
  default:
    break;
  }
}

static void hoist_vars_in_stmt(compiler* c, ast_node* s);

static void hoist_vars_in_list(compiler* c, ast_list* list) {
  for (int i = 0; i < list->len; i++)
    hoist_vars_in_stmt(c, list->items[i]);
}

static void hoist_vars_in_stmt(compiler* c, ast_node* s) {
  if (!s)
    return;
  switch (s->kind) {
  case ast_var_decl:
    if (s->flag_a == tok_kw_var) {
      for (int i = 0; i < s->list.len; i++) {
        ast_node* d = s->list.items[i];
        ast_node* target = (d->kind == ast_pat_assign) ? d->a : d;
        hoist_pattern_names(c, target, 1, 0);
      }
    }
    break;
  case ast_block:
    hoist_vars_in_list(c, &s->list);
    break;
  case ast_if:
    hoist_vars_in_stmt(c, s->b);
    hoist_vars_in_stmt(c, s->c);
    break;
  case ast_while:
    hoist_vars_in_stmt(c, s->b);
    break;
  case ast_do_while:
    hoist_vars_in_stmt(c, s->a);
    break;
  case ast_for:
    hoist_vars_in_stmt(c, s->a);
    hoist_vars_in_stmt(c, s->d);
    break;
  case ast_for_in:
  case ast_for_of:
    if (s->flag_a == tok_kw_var)
      hoist_pattern_names(c, s->a, 1, 0);
    hoist_vars_in_stmt(c, s->c);
    break;
  case ast_switch:
    for (int i = 0; i < s->cases.len; i++)
      hoist_vars_in_list(c, &s->cases.items[i].body);
    break;
  case ast_try:
    hoist_vars_in_stmt(c, s->a);
    hoist_vars_in_stmt(c, s->c);
    hoist_vars_in_stmt(c, s->d);
    break;
  case ast_labeled:
    hoist_vars_in_stmt(c, s->a);
    break;
  default:
    break;
  }
}

void ast_hoist_scope(compiler* c, ast_list* body) {
  ast_node** pending_fns =
      malloc(sizeof(ast_node*) * ast_hoist_max_pending_fns);
  int pending_count = 0;

  for (int i = 0; i < body->len; i++) {
    ast_node* s = body->items[i];

    hoist_vars_in_stmt(c, s);

    if (s->kind == ast_var_decl && s->flag_a != tok_kw_var) {
      int is_const = s->flag_a == tok_kw_const;
      for (int j = 0; j < s->list.len; j++) {
        ast_node* d = s->list.items[j];
        ast_node* target = (d->kind == ast_pat_assign) ? d->a : d;
        hoist_pattern_names(c, target, 0, is_const);
      }
    } else if (s->kind == ast_func_decl) {
      hoist_name(c, s->str, s->str_len, 1, 0);
      if (pending_count < ast_hoist_max_pending_fns)
        pending_fns[pending_count++] = s;
    } else if (s->kind == ast_class_decl) {
      hoist_name(c, s->str, s->str_len, 1, 0);
    } else if (s->kind == ast_import && s->import_binding) {
      ast_import_binding* ib = s->import_binding;
      if (ib->has_default)
        hoist_name(c, ib->default_name, ib->default_len, 1, 0);
      if (ib->has_namespace)
        hoist_name(c, ib->namespace_name, ib->namespace_len, 1, 0);
      for (int j = 0; j < ib->named_count; j++)
        hoist_name(c, ib->named[j].local, ib->named[j].local_len, 1, 0);
    }
  }

  for (int i = 0; i < pending_count; i++) {
    ast_node* fn = pending_fns[i];
    char* lambda_name = malloc(24);
    ast_codegen_function_value(c, fn, lambda_name);
    if (fn->flag_c && fn->flag_b)
      emit_wrap_async_generator(c);
    else if (fn->flag_c)
      emit_wrap_generator(c);
    else if (fn->flag_b)
      emit_wrap_async(c);
    var_ref vr = resolve_var(c, fn->str, fn->str_len);
    if (vr.kind != var_not_found) {
      emit_var_write_ref(c, vr);
      op_emit(c->m, op_pop);
    }
  }

  free(pending_fns);
}
