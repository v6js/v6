#include "v6/optimizer_buf.h"
#include "v6/optimizer_pass.h"
#include "v6/optimizer_print.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
  cv_none,
  cv_num,
  cv_str,
  cv_bool,
  cv_null,
  cv_undef,
} cv_kind;

typedef struct const_val {
  cv_kind kind;
  double num;
  const char* str;
  size_t str_len;
} const_val;

static int get_const(ast_node* n, const_val* out) {
  switch (n->kind) {
  case ast_num:
    out->kind = cv_num;
    out->num = n->num;
    return 1;
  case ast_str:
    out->kind = cv_str;
    out->str = n->str;
    out->str_len = n->str_len;
    return 1;
  case ast_bool:
    out->kind = cv_bool;
    out->num = n->flag_a;
    return 1;
  case ast_null:
    out->kind = cv_null;
    return 1;
  case ast_undef:
    out->kind = cv_undef;
    return 1;
  default:
    return 0;
  }
}

static double str_to_number(const char* s, size_t len) {
  size_t start = 0, end = len;
  while (start < end &&
         (s[start] == ' ' || s[start] == '\t' || s[start] == '\n' ||
          s[start] == '\r' || s[start] == '\f' || s[start] == '\v'))
    start++;
  while (end > start &&
         (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\n' ||
          s[end - 1] == '\r' || s[end - 1] == '\f' || s[end - 1] == '\v'))
    end--;
  if (start == end)
    return 0.0;
  if (end - start == 8 && memcmp(s + start, "Infinity", 8) == 0)
    return HUGE_VAL;
  if (end - start == 9 && memcmp(s + start, "-Infinity", 9) == 0)
    return -HUGE_VAL;
  char buf[64];
  size_t n = end - start;
  if (n >= sizeof(buf))
    return NAN;
  memcpy(buf, s + start, n);
  buf[n] = '\0';
  char* endp;
  double v = strtod(buf, &endp);
  if (endp != buf + n)
    return NAN;
  return v;
}

static double to_number(const_val v) {
  switch (v.kind) {
  case cv_num:
    return v.num;
  case cv_bool:
    return v.num != 0.0 ? 1.0 : 0.0;
  case cv_null:
    return 0.0;
  case cv_undef:
    return NAN;
  case cv_str:
    return str_to_number(v.str, v.str_len);
  default:
    return NAN;
  }
}

static int to_boolean(const_val v) {
  switch (v.kind) {
  case cv_num:
    return v.num != 0.0 && !isnan(v.num);
  case cv_bool:
    return v.num != 0.0;
  case cv_null:
  case cv_undef:
    return 0;
  case cv_str:
    return v.str_len > 0;
  default:
    return 0;
  }
}

static char* number_to_string(ast_arena* arena, double num, size_t* out_len) {
  v6_opt_buf tmp;
  v6_opt_buf_init(&tmp);
  v6_opt_print_number(&tmp, num);
  char* s = ast_arena_strdup(arena, tmp.data, tmp.len);
  *out_len = tmp.len;
  v6_opt_buf_free(&tmp);
  return s;
}

static const char* to_string_val(ast_arena* arena, const_val v,
                                 size_t* out_len) {
  switch (v.kind) {
  case cv_str:
    *out_len = v.str_len;
    return v.str;
  case cv_num:
    return number_to_string(arena, v.num, out_len);
  case cv_bool:
    if (v.num != 0.0) {
      *out_len = 4;
      return "true";
    }
    *out_len = 5;
    return "false";
  case cv_null:
    *out_len = 4;
    return "null";
  case cv_undef:
    *out_len = 9;
    return "undefined";
  default:
    *out_len = 0;
    return "";
  }
}

static void set_num(ast_node* n, double v) {
  n->kind = ast_num;
  n->num = v;
  n->a = n->b = n->c = n->d = NULL;
}

static void set_str(ast_node* n, const char* s, size_t len) {
  n->kind = ast_str;
  n->str = s;
  n->str_len = len;
  n->a = n->b = n->c = n->d = NULL;
}

static void set_bool(ast_node* n, int v) {
  n->kind = ast_bool;
  n->flag_a = v ? 1 : 0;
  n->a = n->b = n->c = n->d = NULL;
}

static void set_undef(ast_node* n) {
  n->kind = ast_undef;
  n->a = n->b = n->c = n->d = NULL;
}

static void copy_node(ast_node* dst, ast_node* src) {
  int line = dst->line;
  *dst = *src;
  dst->line = line;
}

static int32_t to_int32(double d) {
  if (isnan(d) || isinf(d))
    return 0;
  double m = fmod(trunc(d), 4294967296.0);
  if (m < 0)
    m += 4294967296.0;
  uint32_t u = (uint32_t)m;
  return (int32_t)u;
}

static uint32_t to_uint32(double d) {
  if (isnan(d) || isinf(d))
    return 0;
  double m = fmod(trunc(d), 4294967296.0);
  if (m < 0)
    m += 4294967296.0;
  return (uint32_t)m;
}

static int is_null_or_undef(cv_kind k) {
  return k == cv_null || k == cv_undef;
}

static int loose_eq(const_val a, const_val b) {
  if (is_null_or_undef(a.kind) || is_null_or_undef(b.kind))
    return is_null_or_undef(a.kind) && is_null_or_undef(b.kind);
  if (a.kind == cv_str && b.kind == cv_str)
    return a.str_len == b.str_len && memcmp(a.str, b.str, a.str_len) == 0;
  double na = to_number(a), nb = to_number(b);
  if (isnan(na) || isnan(nb))
    return 0;
  return na == nb;
}

static int strict_eq(const_val a, const_val b) {
  if (a.kind != b.kind)
    return 0;
  switch (a.kind) {
  case cv_num:
    return a.num == b.num;
  case cv_str:
    return a.str_len == b.str_len && memcmp(a.str, b.str, a.str_len) == 0;
  case cv_bool:
    return a.num == b.num;
  case cv_null:
  case cv_undef:
    return 1;
  default:
    return 0;
  }
}

static ast_arena* g_fold_arena;

static int fold_binary(ast_node* n) {
  const_val a, b;
  if (!get_const(n->a, &a) || !get_const(n->b, &b))
    return 0;

  switch (n->op) {
  case tok_plus: {
    if (a.kind == cv_str || b.kind == cv_str) {
      size_t al, bl;
      const char* as = to_string_val(g_fold_arena, a, &al);
      const char* bs = to_string_val(g_fold_arena, b, &bl);
      char* combined = ast_arena_alloc(g_fold_arena, al + bl + 1);
      memcpy(combined, as, al);
      memcpy(combined + al, bs, bl);
      combined[al + bl] = '\0';
      set_str(n, combined, al + bl);
    } else {
      set_num(n, to_number(a) + to_number(b));
    }
    return 1;
  }
  case tok_minus:
    set_num(n, to_number(a) - to_number(b));
    return 1;
  case tok_star:
    set_num(n, to_number(a) * to_number(b));
    return 1;
  case tok_slash:
    set_num(n, to_number(a) / to_number(b));
    return 1;
  case tok_percent:
    set_num(n, fmod(to_number(a), to_number(b)));
    return 1;
  case tok_star_star:
    set_num(n, pow(to_number(a), to_number(b)));
    return 1;
  case tok_amp:
    set_num(n, (double)(to_int32(to_number(a)) & to_int32(to_number(b))));
    return 1;
  case tok_pipe:
    set_num(n, (double)(to_int32(to_number(a)) | to_int32(to_number(b))));
    return 1;
  case tok_caret:
    set_num(n, (double)(to_int32(to_number(a)) ^ to_int32(to_number(b))));
    return 1;
  case tok_shl:
    set_num(n,
            (double)(to_int32(to_number(a)) << (to_uint32(to_number(b)) & 31)));
    return 1;
  case tok_shr:
    set_num(n,
            (double)(to_int32(to_number(a)) >> (to_uint32(to_number(b)) & 31)));
    return 1;
  case tok_ushr:
    set_num(
        n, (double)(to_uint32(to_number(a)) >> (to_uint32(to_number(b)) & 31)));
    return 1;
  case tok_lt:
  case tok_gt:
  case tok_le:
  case tok_ge: {
    int result;
    if (a.kind == cv_str && b.kind == cv_str) {
      size_t ml = a.str_len < b.str_len ? a.str_len : b.str_len;
      int c = ml ? memcmp(a.str, b.str, ml) : 0;
      if (c == 0)
        c = (a.str_len > b.str_len) - (a.str_len < b.str_len);
      result = n->op == tok_lt   ? c < 0
               : n->op == tok_gt ? c > 0
               : n->op == tok_le ? c <= 0
                                 : c >= 0;
    } else {
      double na = to_number(a), nb = to_number(b);
      if (isnan(na) || isnan(nb))
        result = 0;
      else
        result = n->op == tok_lt   ? na < nb
                 : n->op == tok_gt ? na > nb
                 : n->op == tok_le ? na <= nb
                                   : na >= nb;
    }
    set_bool(n, result);
    return 1;
  }
  case tok_eq_strict:
    set_bool(n, strict_eq(a, b));
    return 1;
  case tok_neq_strict:
    set_bool(n, !strict_eq(a, b));
    return 1;
  case tok_eq:
    set_bool(n, loose_eq(a, b));
    return 1;
  case tok_neq:
    set_bool(n, !loose_eq(a, b));
    return 1;
  default:
    return 0;
  }
}

static int fold_logical(ast_node* n) {
  const_val a;
  int have_a = get_const(n->a, &a);

  if (n->op == tok_question_question) {
    if (have_a) {
      if (!is_null_or_undef(a.kind)) {
        copy_node(n, n->a);
        return 1;
      }
      copy_node(n, n->b);
      return 1;
    }
    return 0;
  }

  if (have_a) {
    int truthy = to_boolean(a);
    if (n->op == tok_amp_amp) {
      if (!truthy) {
        copy_node(n, n->a);
        return 1;
      }
      copy_node(n, n->b);
      return 1;
    }
    if (n->op == tok_pipe_pipe) {
      if (truthy) {
        copy_node(n, n->a);
        return 1;
      }
      copy_node(n, n->b);
      return 1;
    }
  }
  return 0;
}

static int fold_unary(ast_node* n) {
  const_val a;
  if (!get_const(n->a, &a))
    return 0;

  switch (n->op) {
  case tok_bang:
    set_bool(n, !to_boolean(a));
    return 1;
  case tok_minus:
    set_num(n, -to_number(a));
    return 1;
  case tok_plus:
    set_num(n, to_number(a));
    return 1;
  case tok_tilde:
    set_num(n, (double)(~to_int32(to_number(a))));
    return 1;
  case tok_kw_void:
    set_undef(n);
    return 1;
  case tok_kw_typeof: {
    const char* t;
    switch (a.kind) {
    case cv_num:
      t = "number";
      break;
    case cv_str:
      t = "string";
      break;
    case cv_bool:
      t = "boolean";
      break;
    case cv_undef:
      t = "undefined";
      break;
    default:
      t = "object";
      break;
    }
    set_str(n, t, strlen(t));
    return 1;
  }
  default:
    return 0;
  }
}

static int fold_cond(ast_node* n) {
  const_val a;
  if (!get_const(n->a, &a))
    return 0;
  if (to_boolean(a))
    copy_node(n, n->b);
  else
    copy_node(n, n->c);
  return 1;
}

static void fold_list(ast_list* list, int* changed);
static void fold_expr(ast_node* n, int* changed);
static void fold_stmt(ast_node* n, int* changed);

static void fold_expr(ast_node* n, int* changed) {
  if (!n)
    return;
  switch (n->kind) {
  case ast_unary:
    fold_expr(n->a, changed);
    if (fold_unary(n))
      *changed = 1;
    break;
  case ast_binary:
    fold_expr(n->a, changed);
    fold_expr(n->b, changed);
    if (fold_binary(n))
      *changed = 1;
    break;
  case ast_logical:
    fold_expr(n->a, changed);
    fold_expr(n->b, changed);
    if (fold_logical(n))
      *changed = 1;
    break;
  case ast_cond:
    fold_expr(n->a, changed);
    fold_expr(n->b, changed);
    fold_expr(n->c, changed);
    if (fold_cond(n))
      *changed = 1;
    break;
  case ast_assign:
    fold_expr(n->b, changed);
    break;
  case ast_update:
    break;
  case ast_seq:
    fold_list(&n->list, changed);
    break;
  case ast_spread:
  case ast_await:
    fold_expr(n->a, changed);
    break;
  case ast_yield:
    fold_expr(n->a, changed);
    break;
  case ast_array_lit:
    fold_list(&n->list, changed);
    break;
  case ast_object_lit:
    for (int i = 0; i < n->props.len; i++) {
      ast_prop* p = &n->props.items[i];
      if (p->computed)
        fold_expr(p->key, changed);
      fold_expr(p->value, changed);
    }
    break;
  case ast_member:
    fold_expr(n->a, changed);
    if (n->flag_a)
      fold_expr(n->b, changed);
    break;
  case ast_call:
  case ast_new:
    fold_expr(n->a, changed);
    fold_list(&n->list, changed);
    break;
  case ast_template:
    fold_list(&n->list, changed);
    break;
  case ast_tagged_template:
    fold_expr(n->a, changed);
    fold_list(&n->list, changed);
    break;
  case ast_func_expr:
    if (!n->flag_d)
      fold_expr(n->a, changed);
    else
      fold_stmt(n->a, changed);
    break;
  case ast_paren_pattern_assign:
    fold_expr(n->b, changed);
    break;
  default:
    break;
  }
}

static void fold_list(ast_list* list, int* changed) {
  for (int i = 0; i < list->len; i++)
    fold_expr(list->items[i], changed);
}

static void fold_var_decl_list(ast_list* list, int* changed) {
  for (int i = 0; i < list->len; i++) {
    ast_node* d = list->items[i];
    if (d->kind == ast_pat_assign)
      fold_expr(d->b, changed);
  }
}

static void fold_stmt(ast_node* n, int* changed) {
  if (!n)
    return;
  switch (n->kind) {
  case ast_program:
  case ast_block:
    for (int i = 0; i < n->list.len; i++)
      fold_stmt(n->list.items[i], changed);
    break;
  case ast_expr_stmt:
    fold_expr(n->a, changed);
    break;
  case ast_var_decl:
    fold_var_decl_list(&n->list, changed);
    break;
  case ast_func_decl:
    fold_stmt(n->a, changed);
    break;
  case ast_class_decl:
  case ast_class_expr:
    if (n->flag_a)
      fold_expr(n->a, changed);
    for (int i = 0; i < n->members.len; i++) {
      ast_class_member* m = &n->members.items[i];
      if (m->computed)
        fold_expr(m->key, changed);
      if (m->value)
        fold_expr(m->value, changed);
    }
    break;
  case ast_if:
    fold_expr(n->a, changed);
    fold_stmt(n->b, changed);
    fold_stmt(n->c, changed);
    break;
  case ast_while:
    fold_expr(n->a, changed);
    fold_stmt(n->b, changed);
    break;
  case ast_do_while:
    fold_stmt(n->a, changed);
    fold_expr(n->b, changed);
    break;
  case ast_for:
    if (n->a) {
      if (n->a->kind == ast_var_decl)
        fold_stmt(n->a, changed);
      else
        fold_expr(n->a, changed);
    }
    fold_expr(n->b, changed);
    fold_expr(n->c, changed);
    fold_stmt(n->d, changed);
    break;
  case ast_for_in:
  case ast_for_of:
    fold_expr(n->b, changed);
    fold_stmt(n->c, changed);
    break;
  case ast_switch:
    fold_expr(n->a, changed);
    for (int i = 0; i < n->cases.len; i++) {
      if (n->cases.items[i].test)
        fold_expr(n->cases.items[i].test, changed);
      for (int j = 0; j < n->cases.items[i].body.len; j++)
        fold_stmt(n->cases.items[i].body.items[j], changed);
    }
    break;
  case ast_try:
    fold_stmt(n->a, changed);
    fold_stmt(n->c, changed);
    fold_stmt(n->d, changed);
    break;
  case ast_throw:
    fold_expr(n->a, changed);
    break;
  case ast_return:
    fold_expr(n->a, changed);
    break;
  case ast_labeled:
    fold_stmt(n->a, changed);
    break;
  default:
    break;
  }
}

int v6_opt_pass_const_fold(ast_node* program, ast_arena* arena) {
  g_fold_arena = arena;
  int changed = 0;
  fold_stmt(program, &changed);
  return changed;
}
