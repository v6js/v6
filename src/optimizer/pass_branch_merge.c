#include "v6/optimizer_pass.h"

#include <string.h>

static int is_pure_expr(ast_node* n) {
  switch (n->kind) {
  case ast_num:
  case ast_bigint:
  case ast_str:
  case ast_bool:
  case ast_null:
  case ast_undef:
  case ast_ident:
  case ast_this:
    return 1;
  default:
    return 0;
  }
}

static int expr_equal(ast_node* a, ast_node* b) {
  if (a->kind != b->kind)
    return 0;
  switch (a->kind) {
  case ast_num:
    return a->num == b->num;
  case ast_bigint:
  case ast_str:
    return a->str_len == b->str_len && memcmp(a->str, b->str, a->str_len) == 0;
  case ast_bool:
    return a->flag_a == b->flag_a;
  case ast_null:
  case ast_undef:
  case ast_this:
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
  case ast_assign:
    return a->op == b->op && expr_equal(a->a, b->a) && expr_equal(a->b, b->b);
  case ast_member:
    if (a->flag_a != b->flag_a)
      return 0;
    if (!expr_equal(a->a, b->a))
      return 0;
    if (a->flag_a)
      return expr_equal(a->b, b->b);
    return a->str_len == b->str_len && memcmp(a->str, b->str, a->str_len) == 0;
  case ast_call:
  case ast_new:
    if (!expr_equal(a->a, b->a))
      return 0;
    if (a->list.len != b->list.len)
      return 0;
    for (int i = 0; i < a->list.len; i++) {
      if (a->list.items[i]->kind == ast_spread ||
          b->list.items[i]->kind == ast_spread)
        return 0;
      if (!expr_equal(a->list.items[i], b->list.items[i]))
        return 0;
    }
    return 1;
  default:
    return 0;
  }
}

static ast_node* unwrap_single(ast_node* s) {
  while (s && s->kind == ast_block && s->list.len == 1)
    s = s->list.items[0];
  return s;
}

static int stmt_equal_mergeable(ast_node* a, ast_node* b) {
  a = unwrap_single(a);
  b = unwrap_single(b);
  if (!a || !b || a->kind != b->kind)
    return 0;
  if (a->kind == ast_return)
    return (!a->a && !b->a) || (a->a && b->a && expr_equal(a->a, b->a));
  if (a->kind == ast_expr_stmt)
    return expr_equal(a->a, b->a);
  return 0;
}

static void become_stmt_seq(ast_arena* arena, ast_node* n, ast_node* cond,
                            ast_node* body) {
  ast_node* cond_stmt = ast_arena_alloc(arena, sizeof(ast_node));
  memset(cond_stmt, 0, sizeof(ast_node));
  cond_stmt->kind = ast_expr_stmt;
  cond_stmt->line = cond->line;
  cond_stmt->a = cond;

  ast_node** items = ast_arena_alloc(arena, sizeof(ast_node*) * 2);
  items[0] = cond_stmt;
  items[1] = unwrap_single(body);

  int line = n->line;
  n->kind = ast_block;
  n->a = n->b = n->c = n->d = NULL;
  n->list.items = items;
  n->list.len = 2;
  n->list.cap = 2;
  n->line = line;
}

static void branch_merge_stmt(ast_arena* arena, ast_node* n, int* changed);

static void branch_merge_list(ast_arena* arena, ast_list* list, int* changed) {
  for (int i = 0; i < list->len; i++)
    branch_merge_stmt(arena, list->items[i], changed);
}

static void branch_merge_stmt(ast_arena* arena, ast_node* n, int* changed) {
  if (!n)
    return;
  switch (n->kind) {
  case ast_program:
  case ast_block:
    branch_merge_list(arena, &n->list, changed);
    break;
  case ast_if:
    branch_merge_stmt(arena, n->b, changed);
    branch_merge_stmt(arena, n->c, changed);
    if (n->c && stmt_equal_mergeable(n->b, n->c)) {
      ast_node* cond = n->a;
      ast_node* body = n->b;
      if (is_pure_expr(cond)) {
        int line = n->line;
        ast_node* keep = unwrap_single(body);
        *n = *keep;
        n->line = line;
      } else {
        become_stmt_seq(arena, n, cond, body);
      }
      *changed = 1;
    }
    break;
  case ast_while:
    branch_merge_stmt(arena, n->b, changed);
    break;
  case ast_do_while:
    branch_merge_stmt(arena, n->a, changed);
    break;
  case ast_for:
    branch_merge_stmt(arena, n->d, changed);
    break;
  case ast_for_in:
  case ast_for_of:
    branch_merge_stmt(arena, n->c, changed);
    break;
  case ast_switch:
    for (int i = 0; i < n->cases.len; i++)
      branch_merge_list(arena, &n->cases.items[i].body, changed);
    break;
  case ast_try:
    branch_merge_stmt(arena, n->a, changed);
    branch_merge_stmt(arena, n->c, changed);
    branch_merge_stmt(arena, n->d, changed);
    break;
  case ast_labeled:
    branch_merge_stmt(arena, n->a, changed);
    break;
  case ast_func_decl:
    branch_merge_stmt(arena, n->a, changed);
    break;
  case ast_class_decl:
  case ast_class_expr:
    for (int i = 0; i < n->members.len; i++) {
      ast_class_member* m = &n->members.items[i];
      if (m->value && !m->is_field && m->value->flag_d)
        branch_merge_stmt(arena, m->value->a, changed);
    }
    break;
  case ast_expr_stmt:
    if (n->a->kind == ast_func_expr && n->a->flag_d)
      branch_merge_stmt(arena, n->a->a, changed);
    break;
  default:
    break;
  }
}

int v6_opt_pass_branch_merge(ast_node* program, ast_arena* arena) {
  int changed = 0;
  branch_merge_stmt(arena, program, &changed);
  return changed;
}
