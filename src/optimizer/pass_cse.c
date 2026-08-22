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

static int is_cse_candidate(ast_node* n) {
  if (!is_safe_pure_expr(n))
    return 0;
  switch (n->kind) {
  case ast_unary:
  case ast_binary:
  case ast_logical:
  case ast_cond:
    return 1;
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

static int expr_equal(ast_node* a, ast_node* b) {
  if (a->kind != b->kind)
    return 0;
  switch (a->kind) {
  case ast_num:
    return a->num == b->num;
  case ast_str:
    return a->str_len == b->str_len && memcmp(a->str, b->str, a->str_len) == 0;
  case ast_bool:
    return a->flag_a == b->flag_a;
  case ast_null:
  case ast_undef:
    return 1;
  case ast_ident:
    return a->str_len == b->str_len && memcmp(a->str, b->str, a->str_len) == 0;
  case ast_unary:
    return a->op == b->op && expr_equal(a->a, b->a);
  case ast_binary:
  case ast_logical:
    return a->op == b->op && expr_equal(a->a, b->a) && expr_equal(a->b, b->b);
  case ast_cond:
    return expr_equal(a->a, b->a) && expr_equal(a->b, b->b) &&
           expr_equal(a->c, b->c);
  default:
    return 0;
  }
}

static void become_ident(ast_node* n, const char* name, size_t len) {
  n->kind = ast_ident;
  n->str = name;
  n->str_len = len;
  n->a = n->b = n->c = n->d = NULL;
}

static void cse_stmt(ast_arena* arena, ast_node* n, int* changed);

static void cse_list(ast_arena* arena, ast_list* list, int* changed) {
  for (int i = 0; i < list->len; i++) {
    ast_node* si = list->items[i];
    if (si->kind != ast_var_decl || si->flag_a != tok_kw_const ||
        si->list.len != 1)
      continue;
    ast_node* di = si->list.items[0];
    if (di->kind != ast_pat_assign || di->a->kind != ast_pat_ident)
      continue;
    if (!is_cse_candidate(di->b))
      continue;

    name_set refs;
    memset(&refs, 0, sizeof(refs));
    collect_ref_names(di->b, &refs);

    for (int j = i + 1; j < list->len; j++) {
      ast_node* sj = list->items[j];
      if (sj->kind == ast_var_decl && sj->flag_a == tok_kw_const &&
          sj->list.len == 1) {
        ast_node* dj = sj->list.items[0];
        if (dj->kind == ast_pat_assign && dj->a->kind == ast_pat_ident &&
            expr_equal(di->b, dj->b)) {
          become_ident(dj->b, di->a->str, di->a->str_len);
          *changed = 1;
        }
      }

      name_set touched;
      memset(&touched, 0, sizeof(touched));
      mut_scan_stmt(sj, &touched);
      int stop = touched.wildcard;
      for (int r = 0; r < refs.count && !stop; r++)
        if (name_set_has(&touched, refs.names[r], refs.lens[r]))
          stop = 1;
      if (stop)
        break;
    }
  }

  for (int i = 0; i < list->len; i++)
    cse_stmt(arena, list->items[i], changed);
}

static void cse_stmt(ast_arena* arena, ast_node* n, int* changed) {
  if (!n)
    return;
  switch (n->kind) {
  case ast_program:
  case ast_block:
    cse_list(arena, &n->list, changed);
    break;
  case ast_if:
    cse_stmt(arena, n->b, changed);
    cse_stmt(arena, n->c, changed);
    break;
  case ast_while:
    cse_stmt(arena, n->b, changed);
    break;
  case ast_do_while:
    cse_stmt(arena, n->a, changed);
    break;
  case ast_for:
    cse_stmt(arena, n->d, changed);
    break;
  case ast_for_in:
  case ast_for_of:
    cse_stmt(arena, n->c, changed);
    break;
  case ast_switch:
    for (int i = 0; i < n->cases.len; i++) {
      ast_list* body = &n->cases.items[i].body;
      for (int j = 0; j < body->len; j++)
        cse_stmt(arena, body->items[j], changed);
    }
    break;
  case ast_try:
    cse_stmt(arena, n->a, changed);
    cse_stmt(arena, n->c, changed);
    cse_stmt(arena, n->d, changed);
    break;
  case ast_labeled:
    cse_stmt(arena, n->a, changed);
    break;
  case ast_func_decl:
    cse_stmt(arena, n->a, changed);
    break;
  case ast_class_decl:
  case ast_class_expr:
    for (int i = 0; i < n->members.len; i++) {
      ast_class_member* m = &n->members.items[i];
      if (m->value && !m->is_field && m->value->flag_d)
        cse_stmt(arena, m->value->a, changed);
    }
    break;
  case ast_expr_stmt:
    if (n->a->kind == ast_func_expr && n->a->flag_d)
      cse_stmt(arena, n->a->a, changed);
    break;
  default:
    break;
  }
}

int v6_opt_pass_common_subexpr(ast_node* program, ast_arena* arena) {
  int changed = 0;
  cse_stmt(arena, program, &changed);
  return changed;
}
