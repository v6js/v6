#include "v6/optimizer_pass.h"

#include <string.h>

typedef struct name_set {
  const char* names[32];
  size_t lens[32];
  int count;
  int wildcard;
} name_set;

static void name_set_add(name_set* s, const char* name, size_t len) {
  if (s->count >= 32) {
    s->wildcard = 1;
    return;
  }
  s->names[s->count] = name;
  s->lens[s->count] = len;
  s->count++;
}

static int name_set_has(name_set* s, const char* name, size_t len) {
  if (s->wildcard)
    return 1;
  for (int i = 0; i < s->count; i++)
    if (s->lens[i] == len && memcmp(s->names[i], name, len) == 0)
      return 1;
  return 0;
}

static void collect_pattern_names(ast_node* pat, name_set* s) {
  if (!pat)
    return;
  switch (pat->kind) {
  case ast_ident:
  case ast_pat_ident:
    name_set_add(s, pat->str, pat->str_len);
    break;
  case ast_pat_assign:
    collect_pattern_names(pat->a, s);
    break;
  case ast_pat_rest:
    collect_pattern_names(pat->a, s);
    break;
  case ast_pat_array:
    for (int i = 0; i < pat->list.len; i++)
      collect_pattern_names(pat->list.items[i], s);
    break;
  case ast_pat_object:
    for (int i = 0; i < pat->props.len; i++)
      collect_pattern_names(pat->props.items[i].value, s);
    break;
  default:
    s->wildcard = 1;
    break;
  }
}

static void mut_scan_expr(ast_node* n, name_set* s);
static void mut_scan_stmt(ast_node* n, name_set* s);

static void mut_scan_expr(ast_node* n, name_set* s) {
  if (!n)
    return;
  switch (n->kind) {
  case ast_assign:
    collect_pattern_names(n->a, s);
    mut_scan_expr(n->b, s);
    break;
  case ast_update:
    collect_pattern_names(n->a, s);
    break;
  case ast_unary:
  case ast_spread:
  case ast_await:
  case ast_yield:
    mut_scan_expr(n->a, s);
    break;
  case ast_binary:
  case ast_logical:
    mut_scan_expr(n->a, s);
    mut_scan_expr(n->b, s);
    break;
  case ast_cond:
    mut_scan_expr(n->a, s);
    mut_scan_expr(n->b, s);
    mut_scan_expr(n->c, s);
    break;
  case ast_seq:
  case ast_array_lit:
  case ast_template:
    for (int i = 0; i < n->list.len; i++)
      mut_scan_expr(n->list.items[i], s);
    break;
  case ast_object_lit:
    for (int i = 0; i < n->props.len; i++) {
      ast_prop* p = &n->props.items[i];
      if (p->computed)
        mut_scan_expr(p->key, s);
      mut_scan_expr(p->value, s);
    }
    break;
  case ast_member:
    mut_scan_expr(n->a, s);
    if (n->flag_a)
      mut_scan_expr(n->b, s);
    break;
  case ast_call:
  case ast_new:
    mut_scan_expr(n->a, s);
    for (int i = 0; i < n->list.len; i++)
      mut_scan_expr(n->list.items[i], s);
    break;
  case ast_tagged_template:
    mut_scan_expr(n->a, s);
    for (int i = 0; i < n->list.len; i++)
      mut_scan_expr(n->list.items[i], s);
    break;
  case ast_func_expr:
    if (n->flag_d)
      mut_scan_stmt(n->a, s);
    else
      mut_scan_expr(n->a, s);
    break;
  case ast_class_expr:
    if (n->flag_a)
      mut_scan_expr(n->a, s);
    for (int i = 0; i < n->members.len; i++) {
      ast_class_member* m = &n->members.items[i];
      if (m->computed)
        mut_scan_expr(m->key, s);
      if (m->value)
        mut_scan_expr(m->value, s);
    }
    break;
  case ast_paren_pattern_assign:
    collect_pattern_names(n->a, s);
    mut_scan_expr(n->b, s);
    break;
  default:
    break;
  }
}

static void mut_scan_var_decl(ast_node* n, name_set* s) {
  for (int i = 0; i < n->list.len; i++) {
    ast_node* d = n->list.items[i];
    if (d->kind == ast_pat_assign)
      mut_scan_expr(d->b, s);
  }
}

static void mut_scan_stmt(ast_node* n, name_set* s) {
  if (!n)
    return;
  switch (n->kind) {
  case ast_program:
  case ast_block:
    for (int i = 0; i < n->list.len; i++)
      mut_scan_stmt(n->list.items[i], s);
    break;
  case ast_expr_stmt:
    mut_scan_expr(n->a, s);
    break;
  case ast_var_decl:
    mut_scan_var_decl(n, s);
    break;
  case ast_func_decl:
    mut_scan_stmt(n->a, s);
    break;
  case ast_class_decl:
    mut_scan_expr(n, s);
    break;
  case ast_if:
    mut_scan_expr(n->a, s);
    mut_scan_stmt(n->b, s);
    mut_scan_stmt(n->c, s);
    break;
  case ast_while:
    mut_scan_expr(n->a, s);
    mut_scan_stmt(n->b, s);
    break;
  case ast_do_while:
    mut_scan_stmt(n->a, s);
    mut_scan_expr(n->b, s);
    break;
  case ast_for:
    if (n->a) {
      if (n->a->kind == ast_var_decl)
        mut_scan_var_decl(n->a, s);
      else
        mut_scan_expr(n->a, s);
    }
    mut_scan_expr(n->b, s);
    mut_scan_expr(n->c, s);
    mut_scan_stmt(n->d, s);
    break;
  case ast_for_in:
  case ast_for_of:
    collect_pattern_names(n->a, s);
    mut_scan_expr(n->b, s);
    mut_scan_stmt(n->c, s);
    break;
  case ast_switch:
    mut_scan_expr(n->a, s);
    for (int i = 0; i < n->cases.len; i++) {
      if (n->cases.items[i].test)
        mut_scan_expr(n->cases.items[i].test, s);
      for (int j = 0; j < n->cases.items[i].body.len; j++)
        mut_scan_stmt(n->cases.items[i].body.items[j], s);
    }
    break;
  case ast_try:
    mut_scan_stmt(n->a, s);
    mut_scan_stmt(n->c, s);
    mut_scan_stmt(n->d, s);
    break;
  case ast_throw:
  case ast_return:
    mut_scan_expr(n->a, s);
    break;
  case ast_labeled:
    mut_scan_stmt(n->a, s);
    break;
  default:
    break;
  }
}

static int is_safe_invariant_expr(ast_node* n) {
  switch (n->kind) {
  case ast_num:
  case ast_str:
  case ast_bool:
  case ast_null:
  case ast_undef:
  case ast_ident:
    return 1;
  case ast_unary:
    return is_safe_invariant_expr(n->a);
  case ast_binary:
  case ast_logical:
    return is_safe_invariant_expr(n->a) && is_safe_invariant_expr(n->b);
  case ast_cond:
    return is_safe_invariant_expr(n->a) && is_safe_invariant_expr(n->b) &&
           is_safe_invariant_expr(n->c);
  default:
    return 0;
  }
}

static void collect_ref_names(ast_node* n, name_set* s) {
  if (!n)
    return;
  switch (n->kind) {
  case ast_ident:
    name_set_add(s, n->str, n->str_len);
    break;
  case ast_unary:
    collect_ref_names(n->a, s);
    break;
  case ast_binary:
  case ast_logical:
    collect_ref_names(n->a, s);
    collect_ref_names(n->b, s);
    break;
  case ast_cond:
    collect_ref_names(n->a, s);
    collect_ref_names(n->b, s);
    collect_ref_names(n->c, s);
    break;
  default:
    break;
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
    if (n->flag_d)
      decl_count_expr(n->a, name, len, count);
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

static void list_ensure_cap(ast_arena* arena, ast_list* list, int min_cap) {
  if (list->cap >= min_cap)
    return;
  int new_cap = list->cap == 0 ? 4 : list->cap * 2;
  while (new_cap < min_cap)
    new_cap *= 2;
  ast_node** items =
      ast_arena_alloc(arena, sizeof(ast_node*) * (size_t)new_cap);
  if (list->items)
    memcpy(items, list->items, sizeof(ast_node*) * (size_t)list->len);
  list->items = items;
  list->cap = new_cap;
}

static void list_insert_one(ast_arena* arena, ast_list* list, int at,
                            ast_node* item) {
  list_ensure_cap(arena, list, list->len + 1);
  memmove(list->items + at + 1, list->items + at,
          sizeof(ast_node*) * (size_t)(list->len - at));
  list->items[at] = item;
  list->len += 1;
}

static int try_hoist_from_loop_body(ast_arena* arena, ast_node* program,
                                    ast_list* outer_list, int loop_index,
                                    ast_node* loop_test, ast_node* loop_update,
                                    ast_node* body) {
  if (!body || body->kind != ast_block)
    return 0;

  name_set mutated;
  memset(&mutated, 0, sizeof(mutated));
  mut_scan_expr(loop_test, &mutated);
  mut_scan_expr(loop_update, &mutated);
  mut_scan_stmt(body, &mutated);

  int changed = 0;
  int out = 0;
  int insert_at = loop_index;

  for (int i = 0; i < body->list.len; i++) {
    ast_node* s = body->list.items[i];
    body->list.items[out++] = s;

    if (s->kind != ast_var_decl || s->flag_a == tok_kw_var || s->list.len != 1)
      continue;
    ast_node* d = s->list.items[0];
    if (d->kind != ast_pat_assign || d->a->kind != ast_pat_ident)
      continue;
    if (!is_safe_invariant_expr(d->b))
      continue;

    name_set refs;
    memset(&refs, 0, sizeof(refs));
    collect_ref_names(d->b, &refs);

    int touched = 0;
    for (int r = 0; r < refs.count && !touched; r++)
      if (name_set_has(&mutated, refs.names[r], refs.lens[r]))
        touched = 1;
    if (touched || refs.wildcard)
      continue;

    int decl_count = 0;
    decl_count_stmt(program, d->a->str, d->a->str_len, &decl_count);
    if (decl_count != 1)
      continue;

    out--;
    list_insert_one(arena, outer_list, insert_at, s);
    insert_at++;
    changed = 1;
  }
  body->list.len = out;

  return changed;
}

static void licm_list(ast_arena* arena, ast_node* program, ast_list* list,
                      int* changed);
static void licm_stmt(ast_arena* arena, ast_node* program, ast_node* n,
                      int* changed);

static void licm_stmt(ast_arena* arena, ast_node* program, ast_node* n,
                      int* changed) {
  if (!n)
    return;
  switch (n->kind) {
  case ast_program:
  case ast_block:
    licm_list(arena, program, &n->list, changed);
    break;
  case ast_if:
    licm_stmt(arena, program, n->b, changed);
    licm_stmt(arena, program, n->c, changed);
    break;
  case ast_while:
    licm_stmt(arena, program, n->b, changed);
    break;
  case ast_do_while:
    licm_stmt(arena, program, n->a, changed);
    break;
  case ast_for:
    licm_stmt(arena, program, n->d, changed);
    break;
  case ast_for_in:
  case ast_for_of:
    licm_stmt(arena, program, n->c, changed);
    break;
  case ast_switch:
    for (int i = 0; i < n->cases.len; i++)
      licm_list(arena, program, &n->cases.items[i].body, changed);
    break;
  case ast_try:
    licm_stmt(arena, program, n->a, changed);
    licm_stmt(arena, program, n->c, changed);
    licm_stmt(arena, program, n->d, changed);
    break;
  case ast_labeled:
    licm_stmt(arena, program, n->a, changed);
    break;
  case ast_func_decl:
    licm_stmt(arena, program, n->a, changed);
    break;
  case ast_class_decl:
  case ast_class_expr:
    for (int i = 0; i < n->members.len; i++) {
      ast_class_member* m = &n->members.items[i];
      if (m->value && !m->is_field && m->value->flag_d)
        licm_stmt(arena, program, m->value->a, changed);
    }
    break;
  case ast_expr_stmt:
    if (n->a->kind == ast_func_expr && n->a->flag_d)
      licm_stmt(arena, program, n->a->a, changed);
    break;
  default:
    break;
  }
}

static void licm_list(ast_arena* arena, ast_node* program, ast_list* list,
                      int* changed) {
  for (int i = 0; i < list->len; i++) {
    ast_node* s = list->items[i];
    if (s->kind == ast_while) {
      if (try_hoist_from_loop_body(arena, program, list, i, s->a, NULL, s->b))
        *changed = 1;
    } else if (s->kind == ast_for) {
      if (try_hoist_from_loop_body(arena, program, list, i, s->b, s->c, s->d))
        *changed = 1;
    } else if (s->kind == ast_do_while) {
      if (try_hoist_from_loop_body(arena, program, list, i, s->b, NULL, s->a))
        *changed = 1;
    }
    licm_stmt(arena, program, s, changed);
  }
}

int v6_opt_pass_loop_invariant(ast_node* program, ast_arena* arena) {
  int changed = 0;
  licm_stmt(arena, program, program, &changed);
  return changed;
}
