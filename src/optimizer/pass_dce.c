#include "v6/optimizer_pass.h"

#include <string.h>

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

static void list_replace_one(ast_arena* arena, ast_list* list, int at,
                             ast_node** src, int count) {
  if (count == 1) {
    list->items[at] = src[0];
    return;
  }
  if (count == 0) {
    memmove(list->items + at, list->items + at + 1,
            sizeof(ast_node*) * (size_t)(list->len - at - 1));
    list->len -= 1;
    return;
  }
  list_ensure_cap(arena, list, list->len + count - 1);
  memmove(list->items + at + count, list->items + at + 1,
          sizeof(ast_node*) * (size_t)(list->len - at - 1));
  memcpy(list->items + at, src, sizeof(ast_node*) * (size_t)count);
  list->len += count - 1;
}

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

static int is_switch_literal(ast_node* n) {
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

static int literal_strict_eq(ast_node* a, ast_node* b) {
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
  default:
    return 0;
  }
}

static int get_const_bool(ast_node* n, int* out) {
  switch (n->kind) {
  case ast_bool:
    *out = n->flag_a;
    return 1;
  case ast_num:
    *out = n->num != 0.0 && n->num == n->num;
    return 1;
  case ast_str:
    *out = n->str_len > 0;
    return 1;
  case ast_null:
  case ast_undef:
    *out = 0;
    return 1;
  default:
    return 0;
  }
}

static void become_empty(ast_node* n) {
  n->kind = ast_empty;
  n->a = n->b = n->c = n->d = NULL;
}

static void become_block_of(ast_arena* arena, ast_node* n, ast_node* body) {
  if (!body) {
    become_empty(n);
    return;
  }
  if (body->kind == ast_block) {
    int line = n->line;
    *n = *body;
    n->line = line;
    return;
  }
  ast_node** items = ast_arena_alloc(arena, sizeof(ast_node*));
  items[0] = body;
  n->kind = ast_block;
  n->a = n->b = n->c = n->d = NULL;
  n->list.items = items;
  n->list.len = 1;
  n->list.cap = 1;
}

static void dce_list(ast_arena* arena, ast_list* list, int* changed);
static void dce_expr_shallow(ast_arena* arena, ast_node* n, int* changed);
static void dce_stmt(ast_arena* arena, ast_node* n, int* changed);

static void dce_stmt(ast_arena* arena, ast_node* n, int* changed) {
  if (!n)
    return;
  switch (n->kind) {
  case ast_program:
  case ast_block:
    dce_list(arena, &n->list, changed);
    break;
  case ast_if: {
    int cond;
    if (get_const_bool(n->a, &cond)) {
      if (cond)
        become_block_of(arena, n, n->b);
      else
        become_block_of(arena, n, n->c);
      *changed = 1;
      dce_stmt(arena, n, changed);
      return;
    }
    dce_stmt(arena, n->b, changed);
    dce_stmt(arena, n->c, changed);
    break;
  }
  case ast_while: {
    int cond;
    if (get_const_bool(n->a, &cond) && !cond) {
      become_empty(n);
      *changed = 1;
      return;
    }
    dce_stmt(arena, n->b, changed);
    break;
  }
  case ast_for: {
    if (n->b) {
      int cond;
      if (get_const_bool(n->b, &cond) && !cond && !n->a) {
        become_empty(n);
        *changed = 1;
        return;
      }
    }
    dce_stmt(arena, n->d, changed);
    break;
  }
  case ast_do_while:
    dce_stmt(arena, n->a, changed);
    break;
  case ast_for_in:
  case ast_for_of:
    dce_stmt(arena, n->c, changed);
    break;
  case ast_switch: {
    if (is_switch_literal(n->a)) {
      int matched_idx = -1, default_idx = -1, give_up = 0;
      for (int i = 0; i < n->cases.len; i++) {
        ast_switch_case* c = &n->cases.items[i];
        if (!c->test) {
          default_idx = i;
          continue;
        }
        if (!is_switch_literal(c->test)) {
          give_up = 1;
          break;
        }
        if (literal_strict_eq(n->a, c->test)) {
          matched_idx = i;
          break;
        }
      }
      if (!give_up) {
        int start = matched_idx >= 0 ? matched_idx : default_idx;
        if (start < 0) {
          become_empty(n);
          *changed = 1;
          break;
        }
        ast_list* body = &n->cases.items[start].body;
        int body_terminates =
            body->len > 0 && is_terminator(body->items[body->len - 1]);
        int is_last_case = start == n->cases.len - 1;
        if (body_terminates || is_last_case) {
          int drop_last = body->len > 0 &&
                          body->items[body->len - 1]->kind == ast_break &&
                          body->items[body->len - 1]->str_len == 0;
          int count = body->len - (drop_last ? 1 : 0);
          ast_node** items =
              count > 0
                  ? ast_arena_alloc(arena, sizeof(ast_node*) * (size_t)count)
                  : NULL;
          for (int k = 0; k < count; k++)
            items[k] = body->items[k];
          n->kind = ast_block;
          n->a = n->b = n->c = n->d = NULL;
          n->list.items = items;
          n->list.len = count;
          n->list.cap = count;
          *changed = 1;
          dce_stmt(arena, n, changed);
          break;
        }
      }
    }
    for (int i = 0; i < n->cases.len; i++)
      dce_list(arena, &n->cases.items[i].body, changed);
    break;
  }
  case ast_try:
    dce_stmt(arena, n->a, changed);
    dce_stmt(arena, n->c, changed);
    dce_stmt(arena, n->d, changed);
    break;
  case ast_labeled:
    dce_stmt(arena, n->a, changed);
    break;
  case ast_func_decl:
    dce_stmt(arena, n->a, changed);
    break;
  case ast_class_decl:
  case ast_class_expr:
    for (int i = 0; i < n->members.len; i++) {
      ast_class_member* m = &n->members.items[i];
      if (m->value && !m->is_field && m->value->flag_d)
        dce_stmt(arena, m->value->a, changed);
    }
    break;
  case ast_expr_stmt:
    dce_expr_shallow(arena, n->a, changed);
    break;
  default:
    break;
  }
}

static void dce_expr_shallow(ast_arena* arena, ast_node* n, int* changed) {
  if (!n)
    return;
  if (n->kind == ast_func_expr && n->flag_d)
    dce_stmt(arena, n->a, changed);
}

static int has_hoisted_decl(ast_node* s) {
  if (!s)
    return 0;
  if (s->kind == ast_func_decl || s->kind == ast_class_decl)
    return 1;
  if (s->kind == ast_var_decl && s->flag_a == tok_kw_var)
    return 1;
  return 0;
}

static int block_is_unwrappable(ast_node* block) {
  for (int i = 0; i < block->list.len; i++) {
    ast_node* s = block->list.items[i];
    if (s->kind == ast_func_decl || s->kind == ast_class_decl)
      return 0;
    if (s->kind == ast_var_decl && s->flag_a != tok_kw_var)
      return 0;
  }
  return 1;
}

static void unwrap_blocks(ast_arena* arena, ast_list* list, int* changed) {
  for (int i = 0; i < list->len; i++) {
    ast_node* s = list->items[i];
    if (s->kind == ast_block && block_is_unwrappable(s)) {
      list_replace_one(arena, list, i, s->list.items, s->list.len);
      *changed = 1;
    }
  }
}

static void dce_list(ast_arena* arena, ast_list* list, int* changed) {
  int cut_at = -1;
  for (int i = 0; i < list->len; i++) {
    if (is_terminator(list->items[i]) && i + 1 < list->len) {
      cut_at = i + 1;
      break;
    }
  }
  if (cut_at >= 0) {
    for (int i = cut_at; i < list->len; i++) {
      if (has_hoisted_decl(list->items[i])) {
        cut_at = -1;
        break;
      }
    }
  }
  int limit = cut_at >= 0 ? cut_at : list->len;
  if (cut_at >= 0) {
    list->len = cut_at;
    *changed = 1;
  }

  int out = 0;
  for (int i = 0; i < limit; i++) {
    ast_node* s = list->items[i];
    dce_stmt(arena, s, changed);
    if (s->kind == ast_empty) {
      *changed = 1;
      continue;
    }
    list->items[out++] = s;
  }
  if (out != list->len) {
    *changed = 1;
    list->len = out;
  }

  unwrap_blocks(arena, list, changed);
}

int v6_opt_pass_dead_code(ast_node* program, ast_arena* arena) {
  int changed = 0;
  dce_stmt(arena, program, &changed);
  return changed;
}
