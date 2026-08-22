#include "v6/optimizer_pass.h"

#include <string.h>

static int is_safe_pure_expr(ast_node* n) {
  switch (n->kind) {
  case ast_num:
  case ast_str:
  case ast_bool:
  case ast_null:
  case ast_undef:
  case ast_ident:
    return 1;
  case ast_unary:
    return is_safe_pure_expr(n->a);
  case ast_binary:
  case ast_logical:
    return is_safe_pure_expr(n->a) && is_safe_pure_expr(n->b);
  case ast_cond:
    return is_safe_pure_expr(n->a) && is_safe_pure_expr(n->b) &&
           is_safe_pure_expr(n->c);
  default:
    return 0;
  }
}

static int name_matches(const char* s, size_t len, const char* name,
                        size_t nlen) {
  return len == nlen && memcmp(s, name, len) == 0;
}

static void decl_count_pattern(ast_node* pat, const char* name, size_t len,
                               int* count) {
  if (!pat)
    return;
  switch (pat->kind) {
  case ast_pat_ident:
    if (name_matches(pat->str, pat->str_len, name, len))
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
                            int* count);
static void decl_count_stmt(ast_node* n, const char* name, size_t len,
                            int* count);

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
    if (name_matches(n->str, n->str_len, name, len))
      (*count)++;
    decl_count_expr(n->a, name, len, count);
    if (n->flag_d)
      decl_count_stmt(n->a, name, len, count);
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
    if (name_matches(n->str, n->str_len, name, len))
      (*count)++;
    decl_count_params(&n->params, name, len, count);
    decl_count_stmt(n->a, name, len, count);
    break;
  case ast_class_decl:
    if (name_matches(n->str, n->str_len, name, len))
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

static void ref_count_expr(ast_node* n, const char* name, size_t len,
                           int* count);
static void ref_count_stmt(ast_node* n, const char* name, size_t len,
                           int* count);

static void ref_count_expr(ast_node* n, const char* name, size_t len,
                           int* count) {
  if (!n)
    return;
  if (n->kind == ast_ident) {
    if (name_matches(n->str, n->str_len, name, len))
      (*count)++;
    return;
  }
  switch (n->kind) {
  case ast_unary:
  case ast_spread:
  case ast_await:
  case ast_yield:
  case ast_update:
    ref_count_expr(n->a, name, len, count);
    break;
  case ast_binary:
  case ast_logical:
  case ast_assign:
    ref_count_expr(n->a, name, len, count);
    ref_count_expr(n->b, name, len, count);
    break;
  case ast_cond:
    ref_count_expr(n->a, name, len, count);
    ref_count_expr(n->b, name, len, count);
    ref_count_expr(n->c, name, len, count);
    break;
  case ast_seq:
  case ast_array_lit:
  case ast_template:
    for (int i = 0; i < n->list.len; i++)
      ref_count_expr(n->list.items[i], name, len, count);
    break;
  case ast_object_lit:
    for (int i = 0; i < n->props.len; i++) {
      ast_prop* p = &n->props.items[i];
      if (p->computed)
        ref_count_expr(p->key, name, len, count);
      ref_count_expr(p->value, name, len, count);
    }
    break;
  case ast_member:
    ref_count_expr(n->a, name, len, count);
    if (n->flag_a)
      ref_count_expr(n->b, name, len, count);
    break;
  case ast_call:
  case ast_new:
    ref_count_expr(n->a, name, len, count);
    for (int i = 0; i < n->list.len; i++)
      ref_count_expr(n->list.items[i], name, len, count);
    break;
  case ast_tagged_template:
    ref_count_expr(n->a, name, len, count);
    for (int i = 0; i < n->list.len; i++)
      ref_count_expr(n->list.items[i], name, len, count);
    break;
  case ast_func_expr:
    if (n->flag_d)
      ref_count_stmt(n->a, name, len, count);
    else
      ref_count_expr(n->a, name, len, count);
    break;
  case ast_class_expr:
    if (n->flag_a)
      ref_count_expr(n->a, name, len, count);
    for (int i = 0; i < n->members.len; i++) {
      ast_class_member* m = &n->members.items[i];
      if (m->computed)
        ref_count_expr(m->key, name, len, count);
      if (m->value)
        ref_count_expr(m->value, name, len, count);
    }
    break;
  case ast_paren_pattern_assign:
    ref_count_expr(n->b, name, len, count);
    break;
  default:
    break;
  }
}

static void ref_count_var_decl(ast_node* n, const char* name, size_t len,
                               int* count) {
  for (int i = 0; i < n->list.len; i++) {
    ast_node* d = n->list.items[i];
    if (d->kind == ast_pat_assign)
      ref_count_expr(d->b, name, len, count);
  }
}

static void ref_count_stmt(ast_node* n, const char* name, size_t len,
                           int* count) {
  if (!n)
    return;
  switch (n->kind) {
  case ast_program:
  case ast_block:
    for (int i = 0; i < n->list.len; i++)
      ref_count_stmt(n->list.items[i], name, len, count);
    break;
  case ast_expr_stmt:
    ref_count_expr(n->a, name, len, count);
    break;
  case ast_var_decl:
    ref_count_var_decl(n, name, len, count);
    break;
  case ast_func_decl:
    ref_count_stmt(n->a, name, len, count);
    break;
  case ast_class_decl:
    ref_count_expr(n, name, len, count);
    break;
  case ast_if:
    ref_count_expr(n->a, name, len, count);
    ref_count_stmt(n->b, name, len, count);
    ref_count_stmt(n->c, name, len, count);
    break;
  case ast_while:
    ref_count_expr(n->a, name, len, count);
    ref_count_stmt(n->b, name, len, count);
    break;
  case ast_do_while:
    ref_count_stmt(n->a, name, len, count);
    ref_count_expr(n->b, name, len, count);
    break;
  case ast_for:
    if (n->a) {
      if (n->a->kind == ast_var_decl)
        ref_count_var_decl(n->a, name, len, count);
      else
        ref_count_expr(n->a, name, len, count);
    }
    ref_count_expr(n->b, name, len, count);
    ref_count_expr(n->c, name, len, count);
    ref_count_stmt(n->d, name, len, count);
    break;
  case ast_for_in:
  case ast_for_of:
    ref_count_expr(n->b, name, len, count);
    ref_count_stmt(n->c, name, len, count);
    break;
  case ast_switch:
    ref_count_expr(n->a, name, len, count);
    for (int i = 0; i < n->cases.len; i++) {
      if (n->cases.items[i].test)
        ref_count_expr(n->cases.items[i].test, name, len, count);
      for (int j = 0; j < n->cases.items[i].body.len; j++)
        ref_count_stmt(n->cases.items[i].body.items[j], name, len, count);
    }
    break;
  case ast_try:
    ref_count_stmt(n->a, name, len, count);
    ref_count_stmt(n->c, name, len, count);
    ref_count_stmt(n->d, name, len, count);
    break;
  case ast_throw:
  case ast_return:
    ref_count_expr(n->a, name, len, count);
    break;
  case ast_labeled:
    ref_count_stmt(n->a, name, len, count);
    break;
  default:
    break;
  }
}

static ast_node* find_call_site_expr(ast_node* n, const char* name, size_t len);
static ast_node* find_call_site_stmt(ast_node* n, const char* name, size_t len);

static ast_node* find_call_site_expr(ast_node* n, const char* name,
                                     size_t len) {
  if (!n)
    return NULL;
  if (n->kind == ast_call && n->a->kind == ast_ident &&
      name_matches(n->a->str, n->a->str_len, name, len))
    return n;
  ast_node* r = NULL;
  switch (n->kind) {
  case ast_unary:
  case ast_spread:
  case ast_await:
  case ast_yield:
  case ast_update:
    r = find_call_site_expr(n->a, name, len);
    break;
  case ast_binary:
  case ast_logical:
  case ast_assign:
    r = find_call_site_expr(n->a, name, len);
    if (!r)
      r = find_call_site_expr(n->b, name, len);
    break;
  case ast_cond:
    r = find_call_site_expr(n->a, name, len);
    if (!r)
      r = find_call_site_expr(n->b, name, len);
    if (!r)
      r = find_call_site_expr(n->c, name, len);
    break;
  case ast_seq:
  case ast_array_lit:
  case ast_template:
    for (int i = 0; i < n->list.len && !r; i++)
      r = find_call_site_expr(n->list.items[i], name, len);
    break;
  case ast_object_lit:
    for (int i = 0; i < n->props.len && !r; i++) {
      ast_prop* p = &n->props.items[i];
      if (p->computed)
        r = find_call_site_expr(p->key, name, len);
      if (!r)
        r = find_call_site_expr(p->value, name, len);
    }
    break;
  case ast_member:
    r = find_call_site_expr(n->a, name, len);
    if (!r && n->flag_a)
      r = find_call_site_expr(n->b, name, len);
    break;
  case ast_call:
  case ast_new:
    r = find_call_site_expr(n->a, name, len);
    for (int i = 0; i < n->list.len && !r; i++)
      r = find_call_site_expr(n->list.items[i], name, len);
    break;
  case ast_tagged_template:
    r = find_call_site_expr(n->a, name, len);
    for (int i = 0; i < n->list.len && !r; i++)
      r = find_call_site_expr(n->list.items[i], name, len);
    break;
  case ast_func_expr:
    if (n->flag_d)
      r = find_call_site_stmt(n->a, name, len);
    else
      r = find_call_site_expr(n->a, name, len);
    break;
  case ast_class_expr:
    if (n->flag_a)
      r = find_call_site_expr(n->a, name, len);
    for (int i = 0; i < n->members.len && !r; i++) {
      ast_class_member* m = &n->members.items[i];
      if (m->computed)
        r = find_call_site_expr(m->key, name, len);
      if (!r && m->value)
        r = find_call_site_expr(m->value, name, len);
    }
    break;
  case ast_paren_pattern_assign:
    r = find_call_site_expr(n->b, name, len);
    break;
  default:
    break;
  }
  return r;
}

static ast_node* find_call_site_var_decl(ast_node* n, const char* name,
                                         size_t len) {
  for (int i = 0; i < n->list.len; i++) {
    ast_node* d = n->list.items[i];
    if (d->kind == ast_pat_assign) {
      ast_node* r = find_call_site_expr(d->b, name, len);
      if (r)
        return r;
    }
  }
  return NULL;
}

static ast_node* find_call_site_stmt(ast_node* n, const char* name,
                                     size_t len) {
  if (!n)
    return NULL;
  ast_node* r = NULL;
  switch (n->kind) {
  case ast_program:
  case ast_block:
    for (int i = 0; i < n->list.len && !r; i++)
      r = find_call_site_stmt(n->list.items[i], name, len);
    break;
  case ast_expr_stmt:
    r = find_call_site_expr(n->a, name, len);
    break;
  case ast_var_decl:
    r = find_call_site_var_decl(n, name, len);
    break;
  case ast_func_decl:
    r = find_call_site_stmt(n->a, name, len);
    break;
  case ast_class_decl:
    r = find_call_site_expr(n, name, len);
    break;
  case ast_if:
    r = find_call_site_expr(n->a, name, len);
    if (!r)
      r = find_call_site_stmt(n->b, name, len);
    if (!r)
      r = find_call_site_stmt(n->c, name, len);
    break;
  case ast_while:
    r = find_call_site_expr(n->a, name, len);
    if (!r)
      r = find_call_site_stmt(n->b, name, len);
    break;
  case ast_do_while:
    r = find_call_site_stmt(n->a, name, len);
    if (!r)
      r = find_call_site_expr(n->b, name, len);
    break;
  case ast_for:
    if (n->a) {
      if (n->a->kind == ast_var_decl)
        r = find_call_site_var_decl(n->a, name, len);
      else
        r = find_call_site_expr(n->a, name, len);
    }
    if (!r)
      r = find_call_site_expr(n->b, name, len);
    if (!r)
      r = find_call_site_expr(n->c, name, len);
    if (!r)
      r = find_call_site_stmt(n->d, name, len);
    break;
  case ast_for_in:
  case ast_for_of:
    r = find_call_site_expr(n->b, name, len);
    if (!r)
      r = find_call_site_stmt(n->c, name, len);
    break;
  case ast_switch:
    r = find_call_site_expr(n->a, name, len);
    for (int i = 0; i < n->cases.len && !r; i++) {
      if (n->cases.items[i].test)
        r = find_call_site_expr(n->cases.items[i].test, name, len);
      for (int j = 0; j < n->cases.items[i].body.len && !r; j++)
        r = find_call_site_stmt(n->cases.items[i].body.items[j], name, len);
    }
    break;
  case ast_try:
    r = find_call_site_stmt(n->a, name, len);
    if (!r)
      r = find_call_site_stmt(n->c, name, len);
    if (!r)
      r = find_call_site_stmt(n->d, name, len);
    break;
  case ast_throw:
  case ast_return:
    r = find_call_site_expr(n->a, name, len);
    break;
  case ast_labeled:
    r = find_call_site_stmt(n->a, name, len);
    break;
  default:
    break;
  }
  return r;
}

static int all_idents_are_params(ast_node* n, const char** param_names,
                                 size_t* param_lens, int param_count) {
  switch (n->kind) {
  case ast_num:
  case ast_str:
  case ast_bool:
  case ast_null:
  case ast_undef:
    return 1;
  case ast_ident:
    for (int i = 0; i < param_count; i++)
      if (name_matches(n->str, n->str_len, param_names[i], param_lens[i]))
        return 1;
    return 0;
  case ast_unary:
    return all_idents_are_params(n->a, param_names, param_lens, param_count);
  case ast_binary:
  case ast_logical:
    return all_idents_are_params(n->a, param_names, param_lens, param_count) &&
           all_idents_are_params(n->b, param_names, param_lens, param_count);
  case ast_cond:
    return all_idents_are_params(n->a, param_names, param_lens, param_count) &&
           all_idents_are_params(n->b, param_names, param_lens, param_count) &&
           all_idents_are_params(n->c, param_names, param_lens, param_count);
  default:
    return 0;
  }
}

static ast_node* clone_pure_expr(ast_arena* arena, ast_node* n) {
  ast_node* c = ast_arena_alloc(arena, sizeof(ast_node));
  *c = *n;
  switch (n->kind) {
  case ast_unary:
    c->a = clone_pure_expr(arena, n->a);
    break;
  case ast_binary:
  case ast_logical:
    c->a = clone_pure_expr(arena, n->a);
    c->b = clone_pure_expr(arena, n->b);
    break;
  case ast_cond:
    c->a = clone_pure_expr(arena, n->a);
    c->b = clone_pure_expr(arena, n->b);
    c->c = clone_pure_expr(arena, n->c);
    break;
  default:
    break;
  }
  return c;
}

static ast_node* substitute_clone(ast_arena* arena, ast_node* n,
                                  const char** param_names, size_t* param_lens,
                                  ast_node** args, int param_count) {
  if (n->kind == ast_ident) {
    for (int i = 0; i < param_count; i++)
      if (name_matches(n->str, n->str_len, param_names[i], param_lens[i]))
        return clone_pure_expr(arena, args[i]);
    return clone_pure_expr(arena, n);
  }
  ast_node* c = ast_arena_alloc(arena, sizeof(ast_node));
  *c = *n;
  switch (n->kind) {
  case ast_unary:
    c->a = substitute_clone(arena, n->a, param_names, param_lens, args,
                            param_count);
    break;
  case ast_binary:
  case ast_logical:
    c->a = substitute_clone(arena, n->a, param_names, param_lens, args,
                            param_count);
    c->b = substitute_clone(arena, n->b, param_names, param_lens, args,
                            param_count);
    break;
  case ast_cond:
    c->a = substitute_clone(arena, n->a, param_names, param_lens, args,
                            param_count);
    c->b = substitute_clone(arena, n->b, param_names, param_lens, args,
                            param_count);
    c->c = substitute_clone(arena, n->c, param_names, param_lens, args,
                            param_count);
    break;
  default:
    break;
  }
  return c;
}

#define v6_opt_inline_max_params 8

static int try_inline_func_decl(ast_arena* arena, ast_node* program,
                                ast_node* fn, int* changed) {
  if (fn->flag_b || fn->flag_c)
    return 0;
  if (fn->params.len > v6_opt_inline_max_params)
    return 0;
  if (!fn->a || !fn->flag_d || fn->a->kind != ast_block || fn->a->list.len != 1)
    return 0;
  ast_node* only = fn->a->list.items[0];
  if (only->kind != ast_return || !only->a)
    return 0;

  const char* param_names[v6_opt_inline_max_params];
  size_t param_lens[v6_opt_inline_max_params];
  for (int i = 0; i < fn->params.len; i++) {
    ast_node* p = fn->params.items[i].pattern;
    if (fn->params.items[i].is_rest || p->kind != ast_pat_ident)
      return 0;
    param_names[i] = p->str;
    param_lens[i] = p->str_len;
  }
  int param_count = fn->params.len;

  if (!is_safe_pure_expr(only->a))
    return 0;
  if (!all_idents_are_params(only->a, param_names, param_lens, param_count))
    return 0;

  int decls = 0;
  decl_count_stmt(program, fn->str, fn->str_len, &decls);
  if (decls != 1)
    return 0;

  int refs = 0;
  ref_count_stmt(program, fn->str, fn->str_len, &refs);
  if (refs != 1)
    return 0;

  ast_node* call = find_call_site_stmt(program, fn->str, fn->str_len);
  if (!call)
    return 0;
  if (call->list.len != param_count)
    return 0;

  ast_node* args[v6_opt_inline_max_params];
  for (int i = 0; i < param_count; i++) {
    ast_node* a = call->list.items[i];
    if (a->kind == ast_spread || !is_safe_pure_expr(a))
      return 0;
    args[i] = a;
  }

  ast_node* result = substitute_clone(arena, only->a, param_names, param_lens,
                                      args, param_count);
  int line = call->line;
  *call = *result;
  call->line = line;
  *changed = 1;
  return 1;
}

static void inline_stmt(ast_arena* arena, ast_node* program, ast_node* n,
                        int* changed);

static void inline_list(ast_arena* arena, ast_node* program, ast_list* list,
                        int* changed) {
  for (int i = 0; i < list->len; i++) {
    ast_node* s = list->items[i];
    if (s->kind == ast_func_decl)
      try_inline_func_decl(arena, program, s, changed);
    inline_stmt(arena, program, s, changed);
  }
}

static void inline_stmt(ast_arena* arena, ast_node* program, ast_node* n,
                        int* changed) {
  if (!n)
    return;
  switch (n->kind) {
  case ast_program:
  case ast_block:
    inline_list(arena, program, &n->list, changed);
    break;
  case ast_if:
    inline_stmt(arena, program, n->b, changed);
    inline_stmt(arena, program, n->c, changed);
    break;
  case ast_while:
    inline_stmt(arena, program, n->b, changed);
    break;
  case ast_do_while:
    inline_stmt(arena, program, n->a, changed);
    break;
  case ast_for:
    inline_stmt(arena, program, n->d, changed);
    break;
  case ast_for_in:
  case ast_for_of:
    inline_stmt(arena, program, n->c, changed);
    break;
  case ast_switch:
    for (int i = 0; i < n->cases.len; i++) {
      ast_list* body = &n->cases.items[i].body;
      for (int j = 0; j < body->len; j++) {
        if (body->items[j]->kind == ast_func_decl)
          try_inline_func_decl(arena, program, body->items[j], changed);
        inline_stmt(arena, program, body->items[j], changed);
      }
    }
    break;
  case ast_try:
    inline_stmt(arena, program, n->a, changed);
    inline_stmt(arena, program, n->c, changed);
    inline_stmt(arena, program, n->d, changed);
    break;
  case ast_labeled:
    inline_stmt(arena, program, n->a, changed);
    break;
  case ast_func_decl:
    inline_stmt(arena, program, n->a, changed);
    break;
  case ast_class_decl:
  case ast_class_expr:
    for (int i = 0; i < n->members.len; i++) {
      ast_class_member* m = &n->members.items[i];
      if (m->value && !m->is_field && m->value->flag_d)
        inline_stmt(arena, program, m->value->a, changed);
    }
    break;
  case ast_expr_stmt:
    if (n->a->kind == ast_func_expr && n->a->flag_d)
      inline_stmt(arena, program, n->a->a, changed);
    break;
  default:
    break;
  }
}

int v6_opt_pass_inline_functions(ast_node* program, ast_arena* arena) {
  int changed = 0;
  inline_stmt(arena, program, program, &changed);
  return changed;
}
