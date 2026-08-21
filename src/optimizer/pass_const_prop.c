#include "v6/optimizer_pass.h"

#include <string.h>

static int is_literal_kind(ast_node* n) {
  switch (n->kind) {
  case ast_num:
  case ast_str:
  case ast_bool:
  case ast_null:
  case ast_undef:
    return 1;
  default:
    return 0;
  }
}

static void decl_count_pattern(ast_node* pat, const char* name, size_t len,
                               int* count);
static void decl_count_expr(ast_node* n, const char* name, size_t len,
                            int* count);
static void decl_count_stmt(ast_node* n, const char* name, size_t len,
                            int* count);

static void decl_count_pattern(ast_node* pat, const char* name, size_t len,
                               int* count) {
  if (!pat)
    return;
  switch (pat->kind) {
  case ast_pat_ident:
    if (pat->str_len == len && memcmp(pat->str, name, len) == 0)
      (*count)++;
    break;
  case ast_pat_assign:
    decl_count_pattern(pat->a, name, len, count);
    break;
  case ast_pat_rest:
    decl_count_pattern(pat->a, name, len, count);
    break;
  case ast_pat_array:
    for (int i = 0; i < pat->list.len; i++)
      decl_count_pattern(pat->list.items[i], name, len, count);
    break;
  case ast_pat_object:
    for (int i = 0; i < pat->props.len; i++)
      decl_count_pattern(pat->props.items[i].value, name, len, count);
    break;
  default:
    break;
  }
}

static void decl_count_params(ast_param_list* params, const char* name,
                              size_t len, int* count) {
  for (int i = 0; i < params->len; i++)
    decl_count_pattern(params->items[i].pattern, name, len, count);
}

static void decl_count_expr(ast_node* n, const char* name, size_t len,
                            int* count) {
  if (!n)
    return;
  switch (n->kind) {
  case ast_unary:
  case ast_spread:
  case ast_await:
  case ast_yield:
  case ast_update:
    decl_count_expr(n->a, name, len, count);
    break;
  case ast_binary:
  case ast_logical:
  case ast_assign:
    decl_count_expr(n->a, name, len, count);
    decl_count_expr(n->b, name, len, count);
    break;
  case ast_cond:
    decl_count_expr(n->a, name, len, count);
    decl_count_expr(n->b, name, len, count);
    decl_count_expr(n->c, name, len, count);
    break;
  case ast_seq:
  case ast_array_lit:
  case ast_template:
    for (int i = 0; i < n->list.len; i++)
      decl_count_expr(n->list.items[i], name, len, count);
    break;
  case ast_object_lit:
    for (int i = 0; i < n->props.len; i++) {
      ast_prop* p = &n->props.items[i];
      if (p->computed)
        decl_count_expr(p->key, name, len, count);
      decl_count_expr(p->value, name, len, count);
    }
    break;
  case ast_member:
    decl_count_expr(n->a, name, len, count);
    if (n->flag_a)
      decl_count_expr(n->b, name, len, count);
    break;
  case ast_call:
  case ast_new:
    decl_count_expr(n->a, name, len, count);
    for (int i = 0; i < n->list.len; i++)
      decl_count_expr(n->list.items[i], name, len, count);
    break;
  case ast_tagged_template:
    decl_count_expr(n->a, name, len, count);
    for (int i = 0; i < n->list.len; i++)
      decl_count_expr(n->list.items[i], name, len, count);
    break;
  case ast_func_expr:
    decl_count_params(&n->params, name, len, count);
    if (n->str_len == len && memcmp(n->str, name, len) == 0)
      (*count)++;
    if (n->flag_d)
      decl_count_stmt(n->a, name, len, count);
    else
      decl_count_expr(n->a, name, len, count);
    break;
  case ast_class_expr:
    if (n->flag_a)
      decl_count_expr(n->a, name, len, count);
    for (int i = 0; i < n->members.len; i++) {
      ast_class_member* m = &n->members.items[i];
      if (m->computed)
        decl_count_expr(m->key, name, len, count);
      if (m->value)
        decl_count_expr(m->value, name, len, count);
    }
    break;
  case ast_paren_pattern_assign:
    decl_count_pattern(n->a, name, len, count);
    decl_count_expr(n->b, name, len, count);
    break;
  default:
    break;
  }
}

static void decl_count_stmt(ast_node* n, const char* name, size_t len,
                            int* count) {
  if (!n)
    return;
  switch (n->kind) {
  case ast_program:
  case ast_block:
    for (int i = 0; i < n->list.len; i++)
      decl_count_stmt(n->list.items[i], name, len, count);
    break;
  case ast_expr_stmt:
    decl_count_expr(n->a, name, len, count);
    break;
  case ast_var_decl:
    for (int i = 0; i < n->list.len; i++) {
      ast_node* d = n->list.items[i];
      ast_node* target = d->kind == ast_pat_assign ? d->a : d;
      decl_count_pattern(target, name, len, count);
      if (d->kind == ast_pat_assign)
        decl_count_expr(d->b, name, len, count);
    }
    break;
  case ast_func_decl:
    if (n->str_len == len && memcmp(n->str, name, len) == 0)
      (*count)++;
    decl_count_params(&n->params, name, len, count);
    decl_count_stmt(n->a, name, len, count);
    break;
  case ast_class_decl:
    if (n->str_len == len && memcmp(n->str, name, len) == 0)
      (*count)++;
    decl_count_expr(n, name, len, count);
    break;
  case ast_if:
    decl_count_expr(n->a, name, len, count);
    decl_count_stmt(n->b, name, len, count);
    decl_count_stmt(n->c, name, len, count);
    break;
  case ast_while:
    decl_count_expr(n->a, name, len, count);
    decl_count_stmt(n->b, name, len, count);
    break;
  case ast_do_while:
    decl_count_stmt(n->a, name, len, count);
    decl_count_expr(n->b, name, len, count);
    break;
  case ast_for:
    if (n->a) {
      if (n->a->kind == ast_var_decl)
        decl_count_stmt(n->a, name, len, count);
      else
        decl_count_expr(n->a, name, len, count);
    }
    decl_count_expr(n->b, name, len, count);
    decl_count_expr(n->c, name, len, count);
    decl_count_stmt(n->d, name, len, count);
    break;
  case ast_for_in:
  case ast_for_of:
    decl_count_pattern(n->a, name, len, count);
    decl_count_expr(n->b, name, len, count);
    decl_count_stmt(n->c, name, len, count);
    break;
  case ast_switch:
    decl_count_expr(n->a, name, len, count);
    for (int i = 0; i < n->cases.len; i++) {
      if (n->cases.items[i].test)
        decl_count_expr(n->cases.items[i].test, name, len, count);
      for (int j = 0; j < n->cases.items[i].body.len; j++)
        decl_count_stmt(n->cases.items[i].body.items[j], name, len, count);
    }
    break;
  case ast_try:
    decl_count_stmt(n->a, name, len, count);
    if (n->flag_a && n->b)
      decl_count_pattern(n->b, name, len, count);
    decl_count_stmt(n->c, name, len, count);
    decl_count_stmt(n->d, name, len, count);
    break;
  case ast_throw:
  case ast_return:
    decl_count_expr(n->a, name, len, count);
    break;
  case ast_labeled:
    decl_count_stmt(n->a, name, len, count);
    break;
  default:
    break;
  }
}

static void replace_expr(ast_node* n, const char* name, size_t len,
                         ast_node* literal, int* did_replace);
static void replace_stmt(ast_node* n, const char* name, size_t len,
                         ast_node* literal, int* did_replace);

static void copy_literal_into(ast_node* dst, ast_node* src) {
  int line = dst->line;
  *dst = *src;
  dst->line = line;
}

static void replace_expr(ast_node* n, const char* name, size_t len,
                         ast_node* literal, int* did_replace) {
  if (!n)
    return;
  if (n->kind == ast_ident) {
    if (n->str_len == len && memcmp(n->str, name, len) == 0) {
      copy_literal_into(n, literal);
      *did_replace = 1;
    }
    return;
  }
  switch (n->kind) {
  case ast_unary:
  case ast_spread:
  case ast_await:
  case ast_yield:
  case ast_update:
    replace_expr(n->a, name, len, literal, did_replace);
    break;
  case ast_binary:
  case ast_logical:
    replace_expr(n->a, name, len, literal, did_replace);
    replace_expr(n->b, name, len, literal, did_replace);
    break;
  case ast_assign:
    replace_expr(n->b, name, len, literal, did_replace);
    break;
  case ast_cond:
    replace_expr(n->a, name, len, literal, did_replace);
    replace_expr(n->b, name, len, literal, did_replace);
    replace_expr(n->c, name, len, literal, did_replace);
    break;
  case ast_seq:
  case ast_array_lit:
  case ast_template:
    for (int i = 0; i < n->list.len; i++)
      replace_expr(n->list.items[i], name, len, literal, did_replace);
    break;
  case ast_object_lit:
    for (int i = 0; i < n->props.len; i++) {
      ast_prop* p = &n->props.items[i];
      if (p->computed)
        replace_expr(p->key, name, len, literal, did_replace);
      replace_expr(p->value, name, len, literal, did_replace);
    }
    break;
  case ast_member:
    replace_expr(n->a, name, len, literal, did_replace);
    if (n->flag_a)
      replace_expr(n->b, name, len, literal, did_replace);
    break;
  case ast_call:
  case ast_new:
    replace_expr(n->a, name, len, literal, did_replace);
    for (int i = 0; i < n->list.len; i++)
      replace_expr(n->list.items[i], name, len, literal, did_replace);
    break;
  case ast_tagged_template:
    replace_expr(n->a, name, len, literal, did_replace);
    for (int i = 0; i < n->list.len; i++)
      replace_expr(n->list.items[i], name, len, literal, did_replace);
    break;
  case ast_func_expr:
    if (n->flag_d)
      replace_stmt(n->a, name, len, literal, did_replace);
    else
      replace_expr(n->a, name, len, literal, did_replace);
    break;
  case ast_class_expr:
    if (n->flag_a)
      replace_expr(n->a, name, len, literal, did_replace);
    for (int i = 0; i < n->members.len; i++) {
      ast_class_member* m = &n->members.items[i];
      if (m->computed)
        replace_expr(m->key, name, len, literal, did_replace);
      if (m->value)
        replace_expr(m->value, name, len, literal, did_replace);
    }
    break;
  case ast_paren_pattern_assign:
    replace_expr(n->b, name, len, literal, did_replace);
    break;
  default:
    break;
  }
}

static void replace_stmt(ast_node* n, const char* name, size_t len,
                         ast_node* literal, int* did_replace) {
  if (!n)
    return;
  switch (n->kind) {
  case ast_program:
  case ast_block:
    for (int i = 0; i < n->list.len; i++)
      replace_stmt(n->list.items[i], name, len, literal, did_replace);
    break;
  case ast_expr_stmt:
    replace_expr(n->a, name, len, literal, did_replace);
    break;
  case ast_var_decl:
    for (int i = 0; i < n->list.len; i++) {
      ast_node* d = n->list.items[i];
      if (d->kind == ast_pat_assign)
        replace_expr(d->b, name, len, literal, did_replace);
    }
    break;
  case ast_func_decl:
    replace_stmt(n->a, name, len, literal, did_replace);
    break;
  case ast_class_decl:
    replace_expr(n, name, len, literal, did_replace);
    break;
  case ast_if:
    replace_expr(n->a, name, len, literal, did_replace);
    replace_stmt(n->b, name, len, literal, did_replace);
    replace_stmt(n->c, name, len, literal, did_replace);
    break;
  case ast_while:
    replace_expr(n->a, name, len, literal, did_replace);
    replace_stmt(n->b, name, len, literal, did_replace);
    break;
  case ast_do_while:
    replace_stmt(n->a, name, len, literal, did_replace);
    replace_expr(n->b, name, len, literal, did_replace);
    break;
  case ast_for:
    if (n->a) {
      if (n->a->kind == ast_var_decl)
        replace_stmt(n->a, name, len, literal, did_replace);
      else
        replace_expr(n->a, name, len, literal, did_replace);
    }
    replace_expr(n->b, name, len, literal, did_replace);
    replace_expr(n->c, name, len, literal, did_replace);
    replace_stmt(n->d, name, len, literal, did_replace);
    break;
  case ast_for_in:
  case ast_for_of:
    replace_expr(n->b, name, len, literal, did_replace);
    replace_stmt(n->c, name, len, literal, did_replace);
    break;
  case ast_switch:
    replace_expr(n->a, name, len, literal, did_replace);
    for (int i = 0; i < n->cases.len; i++) {
      if (n->cases.items[i].test)
        replace_expr(n->cases.items[i].test, name, len, literal, did_replace);
      for (int j = 0; j < n->cases.items[i].body.len; j++)
        replace_stmt(n->cases.items[i].body.items[j], name, len, literal,
                     did_replace);
    }
    break;
  case ast_try:
    replace_stmt(n->a, name, len, literal, did_replace);
    replace_stmt(n->c, name, len, literal, did_replace);
    replace_stmt(n->d, name, len, literal, did_replace);
    break;
  case ast_throw:
  case ast_return:
    replace_expr(n->a, name, len, literal, did_replace);
    break;
  case ast_labeled:
    replace_stmt(n->a, name, len, literal, did_replace);
    break;
  default:
    break;
  }
}

static void collect_and_propagate_stmt(ast_node* program, ast_node* n,
                                       int* changed);
static void collect_and_propagate_expr(ast_node* program, ast_node* n,
                                       int* changed);

static void collect_and_propagate_list(ast_node* program, ast_list* list,
                                       int* changed) {
  for (int i = 0; i < list->len; i++)
    collect_and_propagate_stmt(program, list->items[i], changed);
}

static void try_propagate_var_decl(ast_node* program, ast_node* n,
                                   int* changed) {
  if (n->flag_a != tok_kw_const)
    return;
  for (int i = 0; i < n->list.len; i++) {
    ast_node* d = n->list.items[i];
    if (d->kind != ast_pat_assign)
      continue;
    ast_node* target = d->a;
    ast_node* init = d->b;
    if (target->kind != ast_pat_ident || !is_literal_kind(init))
      continue;
    int count = 0;
    decl_count_stmt(program, target->str, target->str_len, &count);
    if (count != 1)
      continue;
    int did_replace = 0;
    replace_stmt(program, target->str, target->str_len, init, &did_replace);
    if (did_replace)
      *changed = 1;
  }
}

static void collect_var_decl_inits(ast_node* program, ast_node* n,
                                   int* changed) {
  for (int i = 0; i < n->list.len; i++) {
    ast_node* d = n->list.items[i];
    if (d->kind == ast_pat_assign)
      collect_and_propagate_expr(program, d->b, changed);
  }
}

static void collect_and_propagate_expr(ast_node* program, ast_node* n,
                                       int* changed) {
  if (!n)
    return;
  switch (n->kind) {
  case ast_unary:
  case ast_spread:
  case ast_await:
  case ast_yield:
  case ast_update:
    collect_and_propagate_expr(program, n->a, changed);
    break;
  case ast_binary:
  case ast_logical:
  case ast_assign:
    collect_and_propagate_expr(program, n->a, changed);
    collect_and_propagate_expr(program, n->b, changed);
    break;
  case ast_cond:
    collect_and_propagate_expr(program, n->a, changed);
    collect_and_propagate_expr(program, n->b, changed);
    collect_and_propagate_expr(program, n->c, changed);
    break;
  case ast_seq:
  case ast_array_lit:
  case ast_template:
    for (int i = 0; i < n->list.len; i++)
      collect_and_propagate_expr(program, n->list.items[i], changed);
    break;
  case ast_object_lit:
    for (int i = 0; i < n->props.len; i++) {
      ast_prop* p = &n->props.items[i];
      if (p->computed)
        collect_and_propagate_expr(program, p->key, changed);
      collect_and_propagate_expr(program, p->value, changed);
    }
    break;
  case ast_member:
    collect_and_propagate_expr(program, n->a, changed);
    if (n->flag_a)
      collect_and_propagate_expr(program, n->b, changed);
    break;
  case ast_call:
  case ast_new:
    collect_and_propagate_expr(program, n->a, changed);
    for (int i = 0; i < n->list.len; i++)
      collect_and_propagate_expr(program, n->list.items[i], changed);
    break;
  case ast_tagged_template:
    collect_and_propagate_expr(program, n->a, changed);
    for (int i = 0; i < n->list.len; i++)
      collect_and_propagate_expr(program, n->list.items[i], changed);
    break;
  case ast_func_expr:
    if (n->flag_d)
      collect_and_propagate_stmt(program, n->a, changed);
    else
      collect_and_propagate_expr(program, n->a, changed);
    break;
  case ast_class_expr:
    for (int i = 0; i < n->members.len; i++) {
      ast_class_member* m = &n->members.items[i];
      if (m->value && !m->is_field && m->value->flag_d)
        collect_and_propagate_stmt(program, m->value->a, changed);
    }
    break;
  case ast_paren_pattern_assign:
    collect_and_propagate_expr(program, n->b, changed);
    break;
  default:
    break;
  }
}

static void collect_and_propagate_stmt(ast_node* program, ast_node* n,
                                       int* changed) {
  if (!n)
    return;
  switch (n->kind) {
  case ast_program:
  case ast_block:
    collect_and_propagate_list(program, &n->list, changed);
    break;
  case ast_expr_stmt:
    collect_and_propagate_expr(program, n->a, changed);
    break;
  case ast_var_decl:
    try_propagate_var_decl(program, n, changed);
    collect_var_decl_inits(program, n, changed);
    break;
  case ast_if:
    collect_and_propagate_expr(program, n->a, changed);
    collect_and_propagate_stmt(program, n->b, changed);
    collect_and_propagate_stmt(program, n->c, changed);
    break;
  case ast_while:
    collect_and_propagate_expr(program, n->a, changed);
    collect_and_propagate_stmt(program, n->b, changed);
    break;
  case ast_do_while:
    collect_and_propagate_stmt(program, n->a, changed);
    collect_and_propagate_expr(program, n->b, changed);
    break;
  case ast_for:
    if (n->a) {
      if (n->a->kind == ast_var_decl) {
        try_propagate_var_decl(program, n->a, changed);
        collect_var_decl_inits(program, n->a, changed);
      } else {
        collect_and_propagate_expr(program, n->a, changed);
      }
    }
    collect_and_propagate_expr(program, n->b, changed);
    collect_and_propagate_expr(program, n->c, changed);
    collect_and_propagate_stmt(program, n->d, changed);
    break;
  case ast_for_in:
  case ast_for_of:
    collect_and_propagate_expr(program, n->b, changed);
    collect_and_propagate_stmt(program, n->c, changed);
    break;
  case ast_switch:
    collect_and_propagate_expr(program, n->a, changed);
    for (int i = 0; i < n->cases.len; i++) {
      if (n->cases.items[i].test)
        collect_and_propagate_expr(program, n->cases.items[i].test, changed);
      collect_and_propagate_list(program, &n->cases.items[i].body, changed);
    }
    break;
  case ast_try:
    collect_and_propagate_stmt(program, n->a, changed);
    collect_and_propagate_stmt(program, n->c, changed);
    collect_and_propagate_stmt(program, n->d, changed);
    break;
  case ast_labeled:
    collect_and_propagate_stmt(program, n->a, changed);
    break;
  case ast_func_decl:
    collect_and_propagate_stmt(program, n->a, changed);
    break;
  case ast_throw:
  case ast_return:
    collect_and_propagate_expr(program, n->a, changed);
    break;
  default:
    break;
  }
}

int v6_opt_pass_const_prop(ast_node* program, ast_arena* arena) {
  (void)arena;
  int changed = 0;
  collect_and_propagate_stmt(program, program, &changed);
  return changed;
}
