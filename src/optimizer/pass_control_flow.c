#include "v6/optimizer_pass.h"

#include <string.h>

static int is_terminator(ast_node* s) {
  if (!s)
    return 0;
  switch (s->kind) {
  case ast_return:
  case ast_throw:
  case ast_break:
  case ast_continue:
    return 1;
  case ast_block:
    return s->list.len > 0 && is_terminator(s->list.items[s->list.len - 1]);
  default:
    return 0;
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

static void list_insert_range(ast_arena* arena, ast_list* list, int at,
                              ast_node** src, int count) {
  if (count == 0)
    return;
  list_ensure_cap(arena, list, list->len + count);
  memmove(list->items + at + count, list->items + at,
          sizeof(ast_node*) * (size_t)(list->len - at));
  memcpy(list->items + at, src, sizeof(ast_node*) * (size_t)count);
  list->len += count;
}

static void cf_list(ast_arena* arena, ast_list* list, int* changed);
static void cf_stmt(ast_arena* arena, ast_node* n, int* changed);
static void cf_expr_shallow(ast_arena* arena, ast_node* n, int* changed);

static void cf_stmt(ast_arena* arena, ast_node* n, int* changed) {
  if (!n)
    return;
  switch (n->kind) {
  case ast_program:
  case ast_block:
    cf_list(arena, &n->list, changed);
    break;
  case ast_if:
    cf_stmt(arena, n->b, changed);
    cf_stmt(arena, n->c, changed);
    break;
  case ast_while:
    cf_stmt(arena, n->b, changed);
    break;
  case ast_do_while:
    cf_stmt(arena, n->a, changed);
    break;
  case ast_for:
    cf_stmt(arena, n->d, changed);
    break;
  case ast_for_in:
  case ast_for_of:
    cf_stmt(arena, n->c, changed);
    break;
  case ast_switch:
    for (int i = 0; i < n->cases.len; i++)
      cf_list(arena, &n->cases.items[i].body, changed);
    break;
  case ast_try:
    cf_stmt(arena, n->a, changed);
    cf_stmt(arena, n->c, changed);
    cf_stmt(arena, n->d, changed);
    break;
  case ast_labeled:
    cf_stmt(arena, n->a, changed);
    break;
  case ast_func_decl:
    cf_stmt(arena, n->a, changed);
    break;
  case ast_class_decl:
  case ast_class_expr:
    for (int i = 0; i < n->members.len; i++) {
      ast_class_member* m = &n->members.items[i];
      if (m->value && !m->is_field && m->value->flag_d)
        cf_stmt(arena, m->value->a, changed);
    }
    break;
  case ast_expr_stmt:
    cf_expr_shallow(arena, n->a, changed);
    break;
  default:
    break;
  }
}

static void cf_expr_shallow(ast_arena* arena, ast_node* n, int* changed) {
  if (!n)
    return;
  if (n->kind == ast_func_expr && n->flag_d)
    cf_stmt(arena, n->a, changed);
}

static void cf_list(ast_arena* arena, ast_list* list, int* changed) {
  for (int i = 0; i < list->len; i++) {
    ast_node* s = list->items[i];
    if (s->kind == ast_if && s->c && is_terminator(s->b)) {
      ast_node* else_branch = s->c;
      s->c = NULL;
      ast_node* tail[1];
      int tail_count;
      ast_node** tail_items;
      if (else_branch->kind == ast_block) {
        tail_items = else_branch->list.items;
        tail_count = else_branch->list.len;
      } else {
        tail[0] = else_branch;
        tail_items = tail;
        tail_count = 1;
      }
      list_insert_range(arena, list, i + 1, tail_items, tail_count);
      *changed = 1;
    }
    cf_stmt(arena, s, changed);
  }
}

int v6_opt_pass_control_flow_simplify(ast_node* program, ast_arena* arena) {
  int changed = 0;
  cf_stmt(arena, program, &changed);
  return changed;
}
