#include "v6/optimizer_pass.h"

#include <string.h>

static void count_uses_expr(ast_node* n, const char* name, size_t len,
                            int* count);
static void count_uses_stmt(ast_node* n, const char* name, size_t len,
                            int* count);

static void count_uses_list(ast_list* list, const char* name, size_t len,
                            int* count) {
  for (int i = 0; i < list->len; i++)
    count_uses_expr(list->items[i], name, len, count);
}

static void count_uses_stmt_list(ast_list* list, const char* name, size_t len,
                                 int* count) {
  for (int i = 0; i < list->len; i++)
    count_uses_stmt(list->items[i], name, len, count);
}

static void count_uses_expr(ast_node* n, const char* name, size_t len,
                            int* count) {
  if (!n)
    return;
  if (n->kind == ast_ident) {
    if (n->str_len == len && memcmp(n->str, name, len) == 0)
      (*count)++;
    return;
  }
  switch (n->kind) {
  case ast_unary:
  case ast_spread:
  case ast_await:
  case ast_yield:
    count_uses_expr(n->a, name, len, count);
    break;
  case ast_update:
    count_uses_expr(n->a, name, len, count);
    break;
  case ast_binary:
  case ast_logical:
  case ast_assign:
    count_uses_expr(n->a, name, len, count);
    count_uses_expr(n->b, name, len, count);
    break;
  case ast_cond:
    count_uses_expr(n->a, name, len, count);
    count_uses_expr(n->b, name, len, count);
    count_uses_expr(n->c, name, len, count);
    break;
  case ast_seq:
  case ast_array_lit:
  case ast_template:
    count_uses_list(&n->list, name, len, count);
    break;
  case ast_object_lit:
    for (int i = 0; i < n->props.len; i++) {
      ast_prop* p = &n->props.items[i];
      if (p->computed)
        count_uses_expr(p->key, name, len, count);
      count_uses_expr(p->value, name, len, count);
    }
    break;
  case ast_member:
    count_uses_expr(n->a, name, len, count);
    if (n->flag_a)
      count_uses_expr(n->b, name, len, count);
    break;
  case ast_call:
  case ast_new:
    count_uses_expr(n->a, name, len, count);
    count_uses_list(&n->list, name, len, count);
    break;
  case ast_tagged_template:
    count_uses_expr(n->a, name, len, count);
    count_uses_list(&n->list, name, len, count);
    break;
  case ast_func_expr:
    for (int i = 0; i < n->params.len; i++) {
      if (n->params.items[i].pattern->kind == ast_pat_assign)
        count_uses_expr(n->params.items[i].pattern->b, name, len, count);
    }
    if (n->flag_d)
      count_uses_stmt(n->a, name, len, count);
    else
      count_uses_expr(n->a, name, len, count);
    break;
  case ast_class_expr:
    if (n->flag_a)
      count_uses_expr(n->a, name, len, count);
    for (int i = 0; i < n->members.len; i++) {
      ast_class_member* m = &n->members.items[i];
      if (m->computed)
        count_uses_expr(m->key, name, len, count);
      if (m->value)
        count_uses_expr(m->value, name, len, count);
    }
    break;
  case ast_paren_pattern_assign:
    count_uses_expr(n->b, name, len, count);
    break;
  default:
    break;
  }
}

static void count_uses_var_decl(ast_node* n, const char* name, size_t len,
                                int* count) {
  for (int i = 0; i < n->list.len; i++) {
    ast_node* d = n->list.items[i];
    if (d->kind == ast_pat_assign)
      count_uses_expr(d->b, name, len, count);
  }
}

static void count_uses_stmt(ast_node* n, const char* name, size_t len,
                            int* count) {
  if (!n)
    return;
  switch (n->kind) {
  case ast_program:
  case ast_block:
    count_uses_stmt_list(&n->list, name, len, count);
    break;
  case ast_expr_stmt:
    count_uses_expr(n->a, name, len, count);
    break;
  case ast_var_decl:
    count_uses_var_decl(n, name, len, count);
    break;
  case ast_func_decl:
    count_uses_stmt(n->a, name, len, count);
    break;
  case ast_class_decl:
    count_uses_expr(n, name, len, count);
    break;
  case ast_if:
    count_uses_expr(n->a, name, len, count);
    count_uses_stmt(n->b, name, len, count);
    count_uses_stmt(n->c, name, len, count);
    break;
  case ast_while:
    count_uses_expr(n->a, name, len, count);
    count_uses_stmt(n->b, name, len, count);
    break;
  case ast_do_while:
    count_uses_stmt(n->a, name, len, count);
    count_uses_expr(n->b, name, len, count);
    break;
  case ast_for:
    if (n->a) {
      if (n->a->kind == ast_var_decl)
        count_uses_var_decl(n->a, name, len, count);
      else
        count_uses_expr(n->a, name, len, count);
    }
    count_uses_expr(n->b, name, len, count);
    count_uses_expr(n->c, name, len, count);
    count_uses_stmt(n->d, name, len, count);
    break;
  case ast_for_in:
  case ast_for_of:
    count_uses_expr(n->b, name, len, count);
    count_uses_stmt(n->c, name, len, count);
    break;
  case ast_switch:
    count_uses_expr(n->a, name, len, count);
    for (int i = 0; i < n->cases.len; i++) {
      if (n->cases.items[i].test)
        count_uses_expr(n->cases.items[i].test, name, len, count);
      for (int j = 0; j < n->cases.items[i].body.len; j++)
        count_uses_stmt(n->cases.items[i].body.items[j], name, len, count);
    }
    break;
  case ast_try:
    count_uses_stmt(n->a, name, len, count);
    count_uses_stmt(n->c, name, len, count);
    count_uses_stmt(n->d, name, len, count);
    break;
  case ast_throw:
  case ast_return:
    count_uses_expr(n->a, name, len, count);
    break;
  case ast_labeled:
    count_uses_stmt(n->a, name, len, count);
    break;
  default:
    break;
  }
}

static int is_safe_dead_initializer(ast_node* n) {
  if (!n)
    return 1;
  switch (n->kind) {
  case ast_num:
  case ast_bigint:
  case ast_str:
  case ast_bool:
  case ast_null:
  case ast_undef:
    return 1;
  default:
    return 0;
  }
}

static int try_remove_unused(ast_node* scope_root, ast_node* target,
                             ast_node* init) {
  if (target->kind != ast_pat_ident)
    return 0;
  if (!is_safe_dead_initializer(init))
    return 0;
  int count = 0;
  count_uses_stmt(scope_root, target->str, target->str_len, &count);
  return count == 0;
}

static void ds_var_decl_list(ast_node* scope_root, ast_list* list,
                             int* changed) {
  int out = 0;
  for (int i = 0; i < list->len; i++) {
    ast_node* d = list->items[i];
    ast_node* target = d->kind == ast_pat_assign ? d->a : d;
    ast_node* init = d->kind == ast_pat_assign ? d->b : NULL;
    if (try_remove_unused(scope_root, target, init)) {
      *changed = 1;
      continue;
    }
    list->items[out++] = d;
  }
  if (out != list->len) {
    *changed = 1;
    list->len = out;
  }
}

static void ds_list(ast_node* scope_root, ast_list* list, int* changed);
static void ds_stmt(ast_node* scope_root, ast_node* n, int* changed);
static void ds_expr_shallow(ast_node* scope_root, ast_node* n, int* changed);

static void ds_stmt(ast_node* scope_root, ast_node* n, int* changed) {
  if (!n)
    return;
  switch (n->kind) {
  case ast_program:
  case ast_block:
    ds_list(scope_root, &n->list, changed);
    break;
  case ast_var_decl:
    if (n->flag_a != tok_kw_var) {
      ds_var_decl_list(scope_root, &n->list, changed);
      if (n->list.len == 0) {
        n->kind = ast_empty;
        n->a = n->b = n->c = n->d = NULL;
      }
    }
    break;
  case ast_if:
    ds_stmt(scope_root, n->b, changed);
    ds_stmt(scope_root, n->c, changed);
    break;
  case ast_while:
    ds_stmt(scope_root, n->b, changed);
    break;
  case ast_do_while:
    ds_stmt(scope_root, n->a, changed);
    break;
  case ast_for:
    ds_stmt(scope_root, n->d, changed);
    break;
  case ast_for_in:
  case ast_for_of:
    ds_stmt(scope_root, n->c, changed);
    break;
  case ast_switch:
    for (int i = 0; i < n->cases.len; i++)
      ds_list(scope_root, &n->cases.items[i].body, changed);
    break;
  case ast_try:
    ds_stmt(scope_root, n->a, changed);
    ds_stmt(scope_root, n->c, changed);
    ds_stmt(scope_root, n->d, changed);
    break;
  case ast_labeled:
    ds_stmt(scope_root, n->a, changed);
    break;
  case ast_func_decl:
    if (n->flag_d)
      ds_stmt(n->a, n->a, changed);
    break;
  case ast_class_decl:
  case ast_class_expr:
    for (int i = 0; i < n->members.len; i++) {
      ast_class_member* m = &n->members.items[i];
      if (m->value && !m->is_field && m->value->flag_d)
        ds_stmt(m->value->a, m->value->a, changed);
    }
    break;
  case ast_expr_stmt:
    ds_expr_shallow(scope_root, n->a, changed);
    break;
  default:
    break;
  }
}

static void ds_expr_shallow(ast_node* scope_root, ast_node* n, int* changed) {
  if (!n)
    return;
  (void)scope_root;
  if (n->kind == ast_func_expr && n->flag_d)
    ds_stmt(n->a, n->a, changed);
}

static void ds_list(ast_node* scope_root, ast_list* list, int* changed) {
  for (int i = 0; i < list->len; i++)
    ds_stmt(scope_root, list->items[i], changed);
}

int v6_opt_pass_dead_store(ast_node* program, ast_arena* arena) {
  (void)arena;
  int changed = 0;
  ds_stmt(program, program, &changed);
  return changed;
}
