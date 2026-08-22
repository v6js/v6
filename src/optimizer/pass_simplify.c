#include "v6/optimizer_pass.h"
#include "v6/optimizer_print.h"

#include <string.h>

static int is_num_literal(ast_node* n, double v) {
  return n->kind == ast_num && n->num == v;
}

static void copy_node(ast_node* dst, ast_node* src) {
  int line = dst->line;
  *dst = *src;
  dst->line = line;
}

static void set_num(ast_node* n, double v) {
  n->kind = ast_num;
  n->num = v;
  n->a = n->b = n->c = n->d = NULL;
}

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

static int simplify_seq(ast_node* n) {
  int out = 0;
  int removed = 0;
  for (int i = 0; i < n->list.len; i++) {
    ast_node* item = n->list.items[i];
    int is_last = i == n->list.len - 1;
    if (!is_last && is_pure_expr(item)) {
      removed = 1;
      continue;
    }
    n->list.items[out++] = item;
  }
  if (out != n->list.len)
    n->list.len = out;
  if (n->list.len == 1) {
    copy_node(n, n->list.items[0]);
    return 1;
  }
  return removed;
}

static int simplify_member(ast_node* n) {
  if (!n->flag_a || n->b->kind != ast_str)
    return 0;
  if (!v6_opt_is_valid_ident_name(n->b->str, n->b->str_len))
    return 0;
  n->flag_a = 0;
  n->str = n->b->str;
  n->str_len = n->b->str_len;
  n->b = NULL;
  return 1;
}

static void become_unary(ast_node* n, tok_kind op, ast_node* operand) {
  n->kind = ast_unary;
  n->op = op;
  n->a = operand;
  n->b = n->c = n->d = NULL;
}

static int simplify_binary(ast_node* n) {
  if (n->op == tok_minus && is_num_literal(n->b, 0.0)) {
    ast_node* keep = n->a;
    become_unary(n, tok_plus, keep);
    return 1;
  }
  if (n->op == tok_minus && is_num_literal(n->a, 0.0)) {
    ast_node* keep = n->b;
    become_unary(n, tok_minus, keep);
    return 1;
  }
  if (n->op == tok_star && is_num_literal(n->b, 1.0)) {
    ast_node* keep = n->a;
    become_unary(n, tok_plus, keep);
    return 1;
  }
  if (n->op == tok_star && is_num_literal(n->a, 1.0)) {
    ast_node* keep = n->b;
    become_unary(n, tok_plus, keep);
    return 1;
  }
  if (n->op == tok_slash && is_num_literal(n->b, 1.0)) {
    ast_node* keep = n->a;
    become_unary(n, tok_plus, keep);
    return 1;
  }
  if (n->op == tok_star_star && is_num_literal(n->b, 1.0)) {
    copy_node(n, n->a);
    return 1;
  }
  if (n->op == tok_star_star && is_num_literal(n->b, 0.0) &&
      is_pure_expr(n->a)) {
    set_num(n, 1.0);
    return 1;
  }
  return 0;
}

static int same_ident(ast_node* a, ast_node* b) {
  return a->kind == ast_ident && b->kind == ast_ident &&
         a->str_len == b->str_len && memcmp(a->str, b->str, a->str_len) == 0;
}

static int simplify_logical(ast_node* n) {
  if ((n->op == tok_pipe_pipe || n->op == tok_amp_amp ||
       n->op == tok_question_question) &&
      same_ident(n->a, n->b)) {
    copy_node(n, n->a);
    return 1;
  }
  return 0;
}

static int collapse_not_chain(ast_node* n) {
  if (n->kind != ast_unary || n->op != tok_bang)
    return 0;
  ast_node* a = n->a;
  if (a->kind != ast_unary || a->op != tok_bang)
    return 0;
  ast_node* b = a->a;
  if (b->kind != ast_unary || b->op != tok_bang)
    return 0;
  n->a = b->a;
  return 1;
}

static int drop_double_negation(ast_node** cond) {
  ast_node* n = *cond;
  if (n->kind != ast_unary || n->op != tok_bang)
    return 0;
  if (n->a->kind != ast_unary || n->a->op != tok_bang)
    return 0;
  *cond = n->a->a;
  return 1;
}

static void simplify_list(ast_list* list, int* changed);
static void simplify_expr(ast_node* n, int* changed);
static void simplify_stmt(ast_node* n, int* changed);

static void simplify_bool_context(ast_node** slot, int* changed) {
  if (!*slot)
    return;
  simplify_expr(*slot, changed);
  if (drop_double_negation(slot))
    *changed = 1;
}

static void simplify_expr(ast_node* n, int* changed) {
  if (!n)
    return;
  switch (n->kind) {
  case ast_unary:
    simplify_expr(n->a, changed);
    if (collapse_not_chain(n))
      *changed = 1;
    break;
  case ast_binary:
    simplify_expr(n->a, changed);
    simplify_expr(n->b, changed);
    if (simplify_binary(n))
      *changed = 1;
    break;
  case ast_logical:
    simplify_expr(n->a, changed);
    simplify_expr(n->b, changed);
    if (simplify_logical(n))
      *changed = 1;
    break;
  case ast_cond:
    simplify_bool_context(&n->a, changed);
    simplify_expr(n->b, changed);
    simplify_expr(n->c, changed);
    break;
  case ast_assign:
    simplify_expr(n->b, changed);
    break;
  case ast_seq:
    simplify_list(&n->list, changed);
    if (simplify_seq(n))
      *changed = 1;
    break;
  case ast_spread:
  case ast_await:
  case ast_yield:
    simplify_expr(n->a, changed);
    break;
  case ast_array_lit:
    simplify_list(&n->list, changed);
    break;
  case ast_object_lit:
    for (int i = 0; i < n->props.len; i++) {
      ast_prop* p = &n->props.items[i];
      if (p->computed) {
        simplify_expr(p->key, changed);
        if (p->key->kind == ast_str &&
            v6_opt_is_valid_ident_name(p->key->str, p->key->str_len)) {
          p->computed = 0;
          *changed = 1;
        }
      }
      simplify_expr(p->value, changed);
    }
    break;
  case ast_member:
    simplify_expr(n->a, changed);
    if (n->flag_a)
      simplify_expr(n->b, changed);
    if (simplify_member(n))
      *changed = 1;
    break;
  case ast_call:
  case ast_new:
    simplify_expr(n->a, changed);
    simplify_list(&n->list, changed);
    break;
  case ast_template:
    simplify_list(&n->list, changed);
    break;
  case ast_tagged_template:
    simplify_expr(n->a, changed);
    simplify_list(&n->list, changed);
    break;
  case ast_func_expr:
    if (!n->flag_d)
      simplify_expr(n->a, changed);
    else
      simplify_stmt(n->a, changed);
    break;
  case ast_paren_pattern_assign:
    simplify_expr(n->b, changed);
    break;
  default:
    break;
  }
}

static void simplify_list(ast_list* list, int* changed) {
  for (int i = 0; i < list->len; i++)
    simplify_expr(list->items[i], changed);
}

static void simplify_var_decl_list(ast_list* list, int* changed) {
  for (int i = 0; i < list->len; i++) {
    ast_node* d = list->items[i];
    if (d->kind == ast_pat_assign)
      simplify_expr(d->b, changed);
  }
}

static void simplify_stmt(ast_node* n, int* changed) {
  if (!n)
    return;
  switch (n->kind) {
  case ast_program:
  case ast_block:
    for (int i = 0; i < n->list.len; i++)
      simplify_stmt(n->list.items[i], changed);
    break;
  case ast_expr_stmt:
    simplify_expr(n->a, changed);
    break;
  case ast_var_decl:
    simplify_var_decl_list(&n->list, changed);
    break;
  case ast_func_decl:
    simplify_stmt(n->a, changed);
    break;
  case ast_class_decl:
  case ast_class_expr:
    if (n->flag_a)
      simplify_expr(n->a, changed);
    for (int i = 0; i < n->members.len; i++) {
      ast_class_member* m = &n->members.items[i];
      if (m->computed) {
        simplify_expr(m->key, changed);
        if (m->key->kind == ast_str &&
            v6_opt_is_valid_ident_name(m->key->str, m->key->str_len)) {
          m->computed = 0;
          *changed = 1;
        }
      }
      if (m->value)
        simplify_expr(m->value, changed);
    }
    break;
  case ast_if:
    simplify_bool_context(&n->a, changed);
    simplify_stmt(n->b, changed);
    simplify_stmt(n->c, changed);
    break;
  case ast_while:
    simplify_bool_context(&n->a, changed);
    simplify_stmt(n->b, changed);
    break;
  case ast_do_while:
    simplify_stmt(n->a, changed);
    simplify_bool_context(&n->b, changed);
    break;
  case ast_for:
    if (n->a) {
      if (n->a->kind == ast_var_decl)
        simplify_stmt(n->a, changed);
      else
        simplify_expr(n->a, changed);
    }
    if (n->b)
      simplify_bool_context(&n->b, changed);
    simplify_expr(n->c, changed);
    simplify_stmt(n->d, changed);
    break;
  case ast_for_in:
  case ast_for_of:
    simplify_expr(n->b, changed);
    simplify_stmt(n->c, changed);
    break;
  case ast_switch:
    simplify_expr(n->a, changed);
    for (int i = 0; i < n->cases.len; i++) {
      if (n->cases.items[i].test)
        simplify_expr(n->cases.items[i].test, changed);
      for (int j = 0; j < n->cases.items[i].body.len; j++)
        simplify_stmt(n->cases.items[i].body.items[j], changed);
    }
    break;
  case ast_try:
    simplify_stmt(n->a, changed);
    simplify_stmt(n->c, changed);
    simplify_stmt(n->d, changed);
    break;
  case ast_throw:
    simplify_expr(n->a, changed);
    break;
  case ast_return:
    simplify_expr(n->a, changed);
    break;
  case ast_labeled:
    simplify_stmt(n->a, changed);
    break;
  default:
    break;
  }
}

int v6_opt_pass_algebraic_simplify(ast_node* program, ast_arena* arena) {
  (void)arena;
  int changed = 0;
  simplify_stmt(program, &changed);
  return changed;
}
