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
static int g_math_shadowed;
static int g_number_shadowed;
static int g_string_shadowed;
static int g_boolean_shadowed;

static int is_ascii_str(const char* s, size_t len) {
  for (size_t i = 0; i < len; i++)
    if ((unsigned char)s[i] >= 0x80)
      return 0;
  return 1;
}

static int name_matches(const char* s, size_t len, const char* name) {
  size_t nlen = strlen(name);
  return len == nlen && memcmp(s, name, nlen) == 0;
}

static int bound_in_pattern(ast_node* pat, const char* name);
static int bound_in_expr(ast_node* n, const char* name);
static int bound_in_stmt(ast_node* n, const char* name);

static int bound_in_pattern(ast_node* pat, const char* name) {
  if (!pat)
    return 0;
  switch (pat->kind) {
  case ast_pat_ident:
    return name_matches(pat->str, pat->str_len, name);
  case ast_pat_assign:
    return bound_in_pattern(pat->a, name) || bound_in_expr(pat->b, name);
  case ast_pat_rest:
    return bound_in_pattern(pat->a, name);
  case ast_pat_array:
    for (int i = 0; i < pat->list.len; i++)
      if (bound_in_pattern(pat->list.items[i], name))
        return 1;
    return 0;
  case ast_pat_object:
    for (int i = 0; i < pat->props.len; i++)
      if (bound_in_pattern(pat->props.items[i].value, name))
        return 1;
    return 0;
  default:
    return 0;
  }
}

static int bound_in_params(ast_param_list* params, const char* name) {
  for (int i = 0; i < params->len; i++)
    if (bound_in_pattern(params->items[i].pattern, name))
      return 1;
  return 0;
}

static int bound_in_expr(ast_node* n, const char* name) {
  if (!n)
    return 0;
  switch (n->kind) {
  case ast_unary:
  case ast_spread:
  case ast_await:
  case ast_yield:
  case ast_update:
    return bound_in_expr(n->a, name);
  case ast_binary:
  case ast_logical:
  case ast_assign:
    return bound_in_expr(n->a, name) || bound_in_expr(n->b, name);
  case ast_cond:
    return bound_in_expr(n->a, name) || bound_in_expr(n->b, name) ||
           bound_in_expr(n->c, name);
  case ast_seq:
  case ast_array_lit:
  case ast_template:
    for (int i = 0; i < n->list.len; i++)
      if (bound_in_expr(n->list.items[i], name))
        return 1;
    return 0;
  case ast_object_lit:
    for (int i = 0; i < n->props.len; i++) {
      ast_prop* p = &n->props.items[i];
      if ((p->computed && bound_in_expr(p->key, name)) ||
          bound_in_expr(p->value, name))
        return 1;
    }
    return 0;
  case ast_member:
    return bound_in_expr(n->a, name) ||
           (n->flag_a && bound_in_expr(n->b, name));
  case ast_call:
  case ast_new:
    if (bound_in_expr(n->a, name))
      return 1;
    for (int i = 0; i < n->list.len; i++)
      if (bound_in_expr(n->list.items[i], name))
        return 1;
    return 0;
  case ast_tagged_template:
    if (bound_in_expr(n->a, name))
      return 1;
    for (int i = 0; i < n->list.len; i++)
      if (bound_in_expr(n->list.items[i], name))
        return 1;
    return 0;
  case ast_func_expr:
    if (name_matches(n->str, n->str_len, name))
      return 1;
    if (bound_in_params(&n->params, name))
      return 1;
    return n->flag_d ? bound_in_stmt(n->a, name) : bound_in_expr(n->a, name);
  case ast_class_expr:
    if ((n->flag_a && bound_in_expr(n->a, name)))
      return 1;
    for (int i = 0; i < n->members.len; i++) {
      ast_class_member* m = &n->members.items[i];
      if ((m->computed && bound_in_expr(m->key, name)) ||
          (m->value && bound_in_expr(m->value, name)))
        return 1;
    }
    return 0;
  case ast_paren_pattern_assign:
    return bound_in_pattern(n->a, name) || bound_in_expr(n->b, name);
  default:
    return 0;
  }
}

static int bound_in_var_decl(ast_node* n, const char* name) {
  for (int i = 0; i < n->list.len; i++) {
    ast_node* d = n->list.items[i];
    ast_node* target = d->kind == ast_pat_assign ? d->a : d;
    if (bound_in_pattern(target, name))
      return 1;
    if (d->kind == ast_pat_assign && bound_in_expr(d->b, name))
      return 1;
  }
  return 0;
}

static int bound_in_stmt(ast_node* n, const char* name) {
  if (!n)
    return 0;
  switch (n->kind) {
  case ast_program:
  case ast_block:
    for (int i = 0; i < n->list.len; i++)
      if (bound_in_stmt(n->list.items[i], name))
        return 1;
    return 0;
  case ast_expr_stmt:
    return bound_in_expr(n->a, name);
  case ast_var_decl:
    return bound_in_var_decl(n, name);
  case ast_func_decl:
    return name_matches(n->str, n->str_len, name) ||
           bound_in_params(&n->params, name) || bound_in_stmt(n->a, name);
  case ast_class_decl:
    return bound_in_expr(n, name);
  case ast_import: {
    ast_import_binding* b = n->import_binding;
    if (!b)
      return 0;
    if (b->has_default && name_matches(b->default_name, b->default_len, name))
      return 1;
    if (b->has_namespace &&
        name_matches(b->namespace_name, b->namespace_len, name))
      return 1;
    for (int i = 0; i < b->named_count; i++)
      if (name_matches(b->named[i].local, b->named[i].local_len, name))
        return 1;
    return 0;
  }
  case ast_if:
    return bound_in_expr(n->a, name) || bound_in_stmt(n->b, name) ||
           bound_in_stmt(n->c, name);
  case ast_while:
    return bound_in_expr(n->a, name) || bound_in_stmt(n->b, name);
  case ast_do_while:
    return bound_in_stmt(n->a, name) || bound_in_expr(n->b, name);
  case ast_for:
    if (n->a) {
      if (n->a->kind == ast_var_decl) {
        if (bound_in_var_decl(n->a, name))
          return 1;
      } else if (bound_in_expr(n->a, name)) {
        return 1;
      }
    }
    return bound_in_expr(n->b, name) || bound_in_expr(n->c, name) ||
           bound_in_stmt(n->d, name);
  case ast_for_in:
  case ast_for_of:
    return bound_in_pattern(n->a, name) || bound_in_expr(n->b, name) ||
           bound_in_stmt(n->c, name);
  case ast_switch:
    if (bound_in_expr(n->a, name))
      return 1;
    for (int i = 0; i < n->cases.len; i++) {
      if (n->cases.items[i].test && bound_in_expr(n->cases.items[i].test, name))
        return 1;
      for (int j = 0; j < n->cases.items[i].body.len; j++)
        if (bound_in_stmt(n->cases.items[i].body.items[j], name))
          return 1;
    }
    return 0;
  case ast_try:
    if (bound_in_stmt(n->a, name))
      return 1;
    if (n->flag_a && n->b && bound_in_pattern(n->b, name))
      return 1;
    return bound_in_stmt(n->c, name) || bound_in_stmt(n->d, name);
  case ast_throw:
  case ast_return:
    return bound_in_expr(n->a, name);
  case ast_labeled:
    return bound_in_stmt(n->a, name);
  default:
    return 0;
  }
}

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

static int get_member_key(ast_node* n, const char** key, size_t* key_len) {
  if (!n->flag_a) {
    *key = n->str;
    *key_len = n->str_len;
    return 1;
  }
  if (n->b->kind == ast_str) {
    *key = n->b->str;
    *key_len = n->b->str_len;
    return 1;
  }
  return 0;
}

static int array_lit_has_dynamic_len(ast_node* arr) {
  for (int i = 0; i < arr->list.len; i++) {
    if (arr->list.items[i]->kind == ast_spread ||
        arr->list.items[i]->kind == ast_pat_hole)
      return 1;
  }
  return 0;
}

typedef struct math_const_entry {
  const char* name;
  double value;
} math_const_entry;

static const math_const_entry math_consts[] = {
    {"PI", 3.141592653589793},     {"E", 2.718281828459045},
    {"LN2", 0.6931471805599453},   {"LN10", 2.302585092994046},
    {"LOG2E", 1.4426950408889634}, {"LOG10E", 0.4342944819032518},
    {"SQRT2", 1.4142135623730951}, {"SQRT1_2", 0.7071067811865476},
};

static int fold_member(ast_node* n) {
  ast_node* obj = n->a;
  if (obj->kind == ast_array_lit) {
    const char* key;
    size_t key_len;
    if (get_member_key(n, &key, &key_len) &&
        name_matches(key, key_len, "length") &&
        !array_lit_has_dynamic_len(obj)) {
      set_num(n, (double)obj->list.len);
      return 1;
    }
    if (!n->flag_a)
      return 0;
    const_val idx_v;
    if (!get_const(n->b, &idx_v) || idx_v.kind != cv_num)
      return 0;
    double idxd = idx_v.num;
    if (idxd != trunc(idxd) || idxd < 0)
      return 0;
    if (array_lit_has_dynamic_len(obj))
      return 0;
    int idx = (int)idxd;
    if (idx >= obj->list.len)
      return 0;
    copy_node(n, obj->list.items[idx]);
    return 1;
  }
  if (obj->kind == ast_str) {
    const char* key;
    size_t key_len;
    if (get_member_key(n, &key, &key_len) &&
        name_matches(key, key_len, "length") &&
        is_ascii_str(obj->str, obj->str_len)) {
      set_num(n, (double)obj->str_len);
      return 1;
    }
    return 0;
  }
  if (obj->kind == ast_ident && !g_math_shadowed &&
      name_matches(obj->str, obj->str_len, "Math")) {
    const char* key;
    size_t key_len;
    if (!get_member_key(n, &key, &key_len))
      return 0;
    for (size_t i = 0; i < sizeof(math_consts) / sizeof(math_consts[0]); i++) {
      if (name_matches(key, key_len, math_consts[i].name)) {
        set_num(n, math_consts[i].value);
        return 1;
      }
    }
    return 0;
  }
  if (obj->kind == ast_object_lit) {
    const char* key;
    size_t key_len;
    if (n->flag_a) {
      const_val kv;
      if (!get_const(n->b, &kv))
        return 0;
      key = to_string_val(g_fold_arena, kv, &key_len);
    } else {
      key = n->str;
      key_len = n->str_len;
    }
    for (int i = 0; i < obj->props.len; i++) {
      if (obj->props.items[i].is_spread || obj->props.items[i].computed)
        return 0;
    }
    ast_prop* found = NULL;
    for (int i = 0; i < obj->props.len; i++) {
      ast_prop* p = &obj->props.items[i];
      if (p->key->kind == ast_str && p->key->str_len == key_len &&
          memcmp(p->key->str, key, key_len) == 0)
        found = p;
    }
    if (!found || found->is_getter || found->is_setter || found->is_method)
      return 0;
    copy_node(n, found->value);
    return 1;
  }
  return 0;
}

static double math_sign(double x) {
  if (isnan(x))
    return NAN;
  if (x > 0)
    return 1.0;
  if (x < 0)
    return -1.0;
  return x;
}

static double math_round(double x) {
  if (isnan(x) || isinf(x) || x == 0)
    return x;
  if (x > 0 && x < 0.5)
    return 0.0;
  if (x < 0 && x >= -0.5)
    return -0.0;
  return floor(x + 0.5);
}

typedef double (*math_unary_fn)(double);

typedef struct math_unary_entry {
  const char* name;
  math_unary_fn fn;
} math_unary_entry;

static const math_unary_entry math_unary_fns[] = {
    {"abs", fabs},    {"floor", floor},      {"ceil", ceil},
    {"trunc", trunc}, {"round", math_round}, {"sign", math_sign},
    {"sqrt", sqrt},   {"cbrt", cbrt},        {"exp", exp},
    {"log", log},     {"log2", log2},        {"log10", log10},
    {"sin", sin},     {"cos", cos},          {"tan", tan},
    {"asin", asin},   {"acos", acos},        {"atan", atan},
};

#define v6_opt_math_call_max_args 16

static int fold_call(ast_node* n) {
  ast_node* callee = n->a;
  if (callee->kind != ast_member || g_math_shadowed)
    return 0;
  ast_node* obj = callee->a;
  if (obj->kind != ast_ident || !name_matches(obj->str, obj->str_len, "Math"))
    return 0;
  const char* key;
  size_t key_len;
  if (!get_member_key(callee, &key, &key_len))
    return 0;

  int argc = n->list.len;
  if (argc > v6_opt_math_call_max_args)
    return 0;

  double args[v6_opt_math_call_max_args];
  for (int i = 0; i < argc; i++) {
    const_val v;
    if (n->list.items[i]->kind == ast_spread ||
        !get_const(n->list.items[i], &v))
      return 0;
    args[i] = to_number(v);
  }

  for (size_t i = 0; i < sizeof(math_unary_fns) / sizeof(math_unary_fns[0]);
       i++) {
    if (name_matches(key, key_len, math_unary_fns[i].name)) {
      if (argc != 1)
        return 0;
      set_num(n, math_unary_fns[i].fn(args[0]));
      return 1;
    }
  }
  if (name_matches(key, key_len, "pow")) {
    if (argc != 2)
      return 0;
    set_num(n, pow(args[0], args[1]));
    return 1;
  }
  if (name_matches(key, key_len, "atan2")) {
    if (argc != 2)
      return 0;
    set_num(n, atan2(args[0], args[1]));
    return 1;
  }
  if (name_matches(key, key_len, "max")) {
    double r = -HUGE_VAL;
    for (int i = 0; i < argc; i++) {
      if (isnan(args[i])) {
        r = NAN;
        break;
      }
      if (args[i] > r)
        r = args[i];
    }
    set_num(n, r);
    return 1;
  }
  if (name_matches(key, key_len, "min")) {
    double r = HUGE_VAL;
    for (int i = 0; i < argc; i++) {
      if (isnan(args[i])) {
        r = NAN;
        break;
      }
      if (args[i] < r)
        r = args[i];
    }
    set_num(n, r);
    return 1;
  }
  if (name_matches(key, key_len, "hypot")) {
    double sum = 0;
    int has_nan = 0, has_inf = 0;
    for (int i = 0; i < argc; i++) {
      if (isinf(args[i]))
        has_inf = 1;
      if (isnan(args[i]))
        has_nan = 1;
      sum += args[i] * args[i];
    }
    if (has_inf)
      set_num(n, HUGE_VAL);
    else if (has_nan)
      set_num(n, NAN);
    else
      set_num(n, sqrt(sum));
    return 1;
  }
  return 0;
}

static int32_t to_java_int(double v) {
  if (isnan(v))
    return 0;
  if (v >= 2147483647.0)
    return 2147483647;
  if (v <= -2147483648.0)
    return (int32_t)(-2147483648LL);
  return (int32_t)v;
}

static int clampi(int v, int lo, int hi) {
  if (v < lo)
    return lo;
  if (v > hi)
    return hi;
  return v;
}

static int norm_index(int idx, int len) {
  if (idx < 0)
    idx = idx + len > 0 ? idx + len : 0;
  return idx > len ? len : idx;
}

static char* dup_bytes(const char* s, int len) {
  char* r = ast_arena_alloc(g_fold_arena, (size_t)len + 1);
  if (len > 0)
    memcpy(r, s, (size_t)len);
  r[len] = '\0';
  return r;
}

static int fold_double_wrapper(ast_node* n) {
  ast_node* callee = n->a;
  if (callee->kind != ast_ident)
    return 0;
  int is_number = !g_number_shadowed &&
                  name_matches(callee->str, callee->str_len, "Number");
  int is_string = !g_string_shadowed &&
                  name_matches(callee->str, callee->str_len, "String");
  int is_boolean = !g_boolean_shadowed &&
                   name_matches(callee->str, callee->str_len, "Boolean");
  if (!is_number && !is_string && !is_boolean)
    return 0;
  if (n->list.len != 1 || n->list.items[0]->kind == ast_spread)
    return 0;
  ast_node* arg = n->list.items[0];
  if (arg->kind != ast_call || arg->a->kind != ast_ident)
    return 0;
  if (arg->list.len != 1 || arg->list.items[0]->kind == ast_spread)
    return 0;
  if (arg->a->str_len != callee->str_len ||
      memcmp(arg->a->str, callee->str, callee->str_len) != 0)
    return 0;
  copy_node(n, arg);
  return 1;
}

static int fold_global_wrapper_call(ast_node* n) {
  ast_node* callee = n->a;
  if (callee->kind != ast_ident)
    return 0;

  int argc = n->list.len;
  if (argc > 0 && n->list.items[0]->kind == ast_spread)
    return 0;
  const_val arg;
  int have_arg = argc > 0 && get_const(n->list.items[0], &arg);
  if (argc > 0 && !have_arg)
    return 0;

  if (!g_number_shadowed &&
      name_matches(callee->str, callee->str_len, "Number")) {
    set_num(n, argc == 0 ? 0.0 : to_number(arg));
    return 1;
  }
  if (!g_boolean_shadowed &&
      name_matches(callee->str, callee->str_len, "Boolean")) {
    set_bool(n, argc == 0 ? 0 : to_boolean(arg));
    return 1;
  }
  if (!g_string_shadowed &&
      name_matches(callee->str, callee->str_len, "String")) {
    if (argc == 0) {
      set_str(n, "", 0);
      return 1;
    }
    size_t sl;
    const char* s = to_string_val(g_fold_arena, arg, &sl);
    set_str(n, s, sl);
    return 1;
  }
  return 0;
}

static int find_substr(const char* s, int len, const char* needle, int nlen,
                       int from) {
  if (from < 0)
    from = 0;
  if (nlen > len - from)
    return -1;
  for (int i = from; i <= len - nlen; i++)
    if (memcmp(s + i, needle, (size_t)nlen) == 0)
      return i;
  return -1;
}

static int find_substr_last(const char* s, int len, const char* needle,
                            int nlen) {
  for (int i = len - nlen; i >= 0; i--)
    if (memcmp(s + i, needle, (size_t)nlen) == 0)
      return i;
  return -1;
}

#define v6_opt_str_call_max_args 4

static int fold_string_call(ast_node* n) {
  ast_node* callee = n->a;
  if (callee->kind != ast_member)
    return 0;
  ast_node* recv = callee->a;
  if (recv->kind != ast_str || !is_ascii_str(recv->str, recv->str_len))
    return 0;
  const char* key;
  size_t key_len;
  if (!get_member_key(callee, &key, &key_len))
    return 0;

  int argc = n->list.len;
  if (argc > v6_opt_str_call_max_args)
    return 0;
  const_val args[v6_opt_str_call_max_args];
  for (int i = 0; i < argc; i++) {
    if (n->list.items[i]->kind == ast_spread ||
        !get_const(n->list.items[i], &args[i]))
      return 0;
    if (args[i].kind == cv_str && !is_ascii_str(args[i].str, args[i].str_len))
      return 0;
  }

  const char* s = recv->str;
  int len = (int)recv->str_len;

  if (name_matches(key, key_len, "charAt")) {
    if (argc != 1 || args[0].kind == cv_undef)
      return 0;
    int i = to_java_int(to_number(args[0]));
    if (i < 0 || i >= len) {
      set_str(n, "", 0);
      return 1;
    }
    set_str(n, dup_bytes(s + i, 1), 1);
    return 1;
  }
  if (name_matches(key, key_len, "charCodeAt")) {
    if (argc != 1 || args[0].kind == cv_undef)
      return 0;
    int i = to_java_int(to_number(args[0]));
    if (i < 0 || i >= len) {
      set_num(n, NAN);
      return 1;
    }
    set_num(n, (double)(unsigned char)s[i]);
    return 1;
  }
  if (name_matches(key, key_len, "toUpperCase") ||
      name_matches(key, key_len, "toLowerCase")) {
    if (argc != 0)
      return 0;
    int upper = name_matches(key, key_len, "toUpperCase");
    char* r = ast_arena_alloc(g_fold_arena, (size_t)len + 1);
    for (int i = 0; i < len; i++) {
      unsigned char c = (unsigned char)s[i];
      if (upper && c >= 'a' && c <= 'z')
        c = (unsigned char)(c - 'a' + 'A');
      else if (!upper && c >= 'A' && c <= 'Z')
        c = (unsigned char)(c - 'A' + 'a');
      r[i] = (char)c;
    }
    r[len] = '\0';
    set_str(n, r, (size_t)len);
    return 1;
  }
  if (name_matches(key, key_len, "slice")) {
    if (argc > 2)
      return 0;
    int start = argc > 0 && args[0].kind != cv_undef
                    ? norm_index(to_java_int(to_number(args[0])), len)
                    : 0;
    int end = argc > 1 && args[1].kind != cv_undef
                  ? norm_index(to_java_int(to_number(args[1])), len)
                  : len;
    if (start >= end) {
      set_str(n, "", 0);
      return 1;
    }
    set_str(n, dup_bytes(s + start, end - start), (size_t)(end - start));
    return 1;
  }
  if (name_matches(key, key_len, "substring")) {
    if (argc > 2)
      return 0;
    int start = argc > 0 && args[0].kind != cv_undef
                    ? clampi(to_java_int(to_number(args[0])), 0, len)
                    : 0;
    int end = argc > 1 && args[1].kind != cv_undef
                  ? clampi(to_java_int(to_number(args[1])), 0, len)
                  : len;
    if (start > end) {
      int t = start;
      start = end;
      end = t;
    }
    set_str(n, dup_bytes(s + start, end - start), (size_t)(end - start));
    return 1;
  }
  if (name_matches(key, key_len, "repeat")) {
    if (argc != 1 || args[0].kind == cv_undef)
      return 0;
    double nv = to_number(args[0]);
    if (isnan(nv))
      nv = 0;
    if (nv < 0 || isinf(nv))
      return 0;
    int cnt = to_java_int(nv);
    if ((long long)cnt * (long long)len > (1 << 20))
      return 0;
    int total = cnt * len;
    char* r = ast_arena_alloc(g_fold_arena, (size_t)total + 1);
    for (int i = 0; i < cnt; i++)
      memcpy(r + i * len, s, (size_t)len);
    r[total] = '\0';
    set_str(n, r, (size_t)total);
    return 1;
  }
  if (name_matches(key, key_len, "concat")) {
    v6_opt_buf buf;
    v6_opt_buf_init(&buf);
    v6_opt_buf_append(&buf, s, (size_t)len);
    for (int i = 0; i < argc; i++) {
      size_t sl;
      const char* sv = to_string_val(g_fold_arena, args[i], &sl);
      if (!is_ascii_str(sv, sl)) {
        v6_opt_buf_free(&buf);
        return 0;
      }
      v6_opt_buf_append(&buf, sv, sl);
    }
    size_t total;
    char* result = v6_opt_buf_take(&buf, &total);
    char* copy = ast_arena_strdup(g_fold_arena, result, total);
    free(result);
    set_str(n, copy, total);
    return 1;
  }
  if (name_matches(key, key_len, "padStart") ||
      name_matches(key, key_len, "padEnd")) {
    if (argc < 1 || argc > 2 || args[0].kind == cv_undef)
      return 0;
    int target = to_java_int(to_number(args[0]));
    const char* pad = " ";
    int pad_len = 1;
    if (argc > 1) {
      if (args[1].kind != cv_str)
        return 0;
      pad = args[1].str;
      pad_len = (int)args[1].str_len;
    }
    if (pad_len == 0 || len >= target) {
      set_str(n, dup_bytes(s, len), (size_t)len);
      return 1;
    }
    int needed = target - len;
    char* r = ast_arena_alloc(g_fold_arena, (size_t)target + 1);
    int is_start = name_matches(key, key_len, "padStart");
    if (is_start) {
      for (int i = 0; i < needed; i++)
        r[i] = pad[i % pad_len];
      memcpy(r + needed, s, (size_t)len);
    } else {
      memcpy(r, s, (size_t)len);
      for (int i = 0; i < needed; i++)
        r[len + i] = pad[i % pad_len];
    }
    r[target] = '\0';
    set_str(n, r, (size_t)target);
    return 1;
  }
  if (argc == 1 && args[0].kind == cv_str && args[0].str_len > 0) {
    const char* needle = args[0].str;
    int nlen = (int)args[0].str_len;
    if (name_matches(key, key_len, "includes")) {
      set_bool(n, find_substr(s, len, needle, nlen, 0) >= 0);
      return 1;
    }
    if (name_matches(key, key_len, "indexOf")) {
      set_num(n, (double)find_substr(s, len, needle, nlen, 0));
      return 1;
    }
    if (name_matches(key, key_len, "lastIndexOf")) {
      set_num(n, (double)find_substr_last(s, len, needle, nlen));
      return 1;
    }
    if (name_matches(key, key_len, "startsWith")) {
      set_bool(n, len >= nlen && memcmp(s, needle, (size_t)nlen) == 0);
      return 1;
    }
    if (name_matches(key, key_len, "endsWith")) {
      set_bool(n, len >= nlen &&
                      memcmp(s + len - nlen, needle, (size_t)nlen) == 0);
      return 1;
    }
  }
  return 0;
}

#define v6_opt_array_call_max_elems 64

static int fold_array_call(ast_node* n) {
  ast_node* callee = n->a;
  if (callee->kind != ast_member)
    return 0;
  ast_node* recv = callee->a;
  if (recv->kind != ast_array_lit || array_lit_has_dynamic_len(recv))
    return 0;
  const char* key;
  size_t key_len;
  if (!get_member_key(callee, &key, &key_len))
    return 0;

  int elen = recv->list.len;
  if (elen > v6_opt_array_call_max_elems)
    return 0;
  const_val elems[v6_opt_array_call_max_elems];
  for (int i = 0; i < elen; i++)
    if (!get_const(recv->list.items[i], &elems[i]))
      return 0;

  int argc = n->list.len;
  if (argc > 2)
    return 0;
  const_val args[2];
  for (int i = 0; i < argc; i++) {
    if (n->list.items[i]->kind == ast_spread ||
        !get_const(n->list.items[i], &args[i]))
      return 0;
  }

  if (name_matches(key, key_len, "join")) {
    if (argc > 1)
      return 0;
    const char* sep = ",";
    size_t sep_len = 1;
    if (argc == 1 && args[0].kind != cv_undef) {
      size_t sl;
      const char* sv = to_string_val(g_fold_arena, args[0], &sl);
      sep = sv;
      sep_len = sl;
    }
    v6_opt_buf buf;
    v6_opt_buf_init(&buf);
    for (int i = 0; i < elen; i++) {
      if (i > 0)
        v6_opt_buf_append(&buf, sep, sep_len);
      if (elems[i].kind == cv_null || elems[i].kind == cv_undef)
        continue;
      size_t sl;
      const char* sv = to_string_val(g_fold_arena, elems[i], &sl);
      v6_opt_buf_append(&buf, sv, sl);
    }
    size_t total;
    char* result = v6_opt_buf_take(&buf, &total);
    char* copy = ast_arena_strdup(g_fold_arena, result, total);
    free(result);
    set_str(n, copy, total);
    return 1;
  }
  if (name_matches(key, key_len, "indexOf")) {
    if (argc != 1)
      return 0;
    int idx = -1;
    for (int i = 0; i < elen; i++) {
      if (strict_eq(elems[i], args[0])) {
        idx = i;
        break;
      }
    }
    set_num(n, (double)idx);
    return 1;
  }
  if (name_matches(key, key_len, "includes")) {
    if (argc != 1)
      return 0;
    int target_is_nan = args[0].kind == cv_num && isnan(args[0].num);
    int found = 0;
    for (int i = 0; i < elen; i++) {
      if (strict_eq(elems[i], args[0])) {
        found = 1;
        break;
      }
      if (target_is_nan && elems[i].kind == cv_num && isnan(elems[i].num)) {
        found = 1;
        break;
      }
    }
    set_bool(n, found);
    return 1;
  }
  if (name_matches(key, key_len, "slice")) {
    if (argc > 0 && args[0].kind == cv_undef)
      return 0;
    if (argc > 1 && args[1].kind == cv_undef)
      return 0;
    int start =
        argc > 0 ? norm_index(to_java_int(to_number(args[0])), elen) : 0;
    int end =
        argc > 1 ? norm_index(to_java_int(to_number(args[1])), elen) : elen;
    int count = end > start ? end - start : 0;
    ast_node** items =
        count > 0
            ? ast_arena_alloc(g_fold_arena, sizeof(ast_node*) * (size_t)count)
            : NULL;
    for (int i = 0; i < count; i++)
      items[i] = recv->list.items[start + i];
    n->kind = ast_array_lit;
    n->a = n->b = n->c = n->d = NULL;
    n->list.items = items;
    n->list.len = count;
    n->list.cap = count;
    return 1;
  }
  return 0;
}

static int fold_template(ast_node* n) {
  for (int i = 0; i < n->list.len; i++) {
    const_val v;
    if (!get_const(n->list.items[i], &v))
      return 0;
  }
  v6_opt_buf buf;
  v6_opt_buf_init(&buf);
  for (int i = 0; i < n->quasis_cooked.len; i++) {
    ast_node* q = n->quasis_cooked.items[i];
    v6_opt_buf_append(&buf, q->str, q->str_len);
    if (i < n->list.len) {
      const_val v;
      get_const(n->list.items[i], &v);
      size_t sl;
      const char* s = to_string_val(g_fold_arena, v, &sl);
      v6_opt_buf_append(&buf, s, sl);
    }
  }
  size_t total_len;
  char* result = v6_opt_buf_take(&buf, &total_len);
  char* arena_copy = ast_arena_strdup(g_fold_arena, result, total_len);
  free(result);
  set_str(n, arena_copy, total_len);
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
    if (fold_member(n))
      *changed = 1;
    break;
  case ast_call:
    fold_expr(n->a, changed);
    fold_list(&n->list, changed);
    if (fold_call(n) || fold_string_call(n) || fold_global_wrapper_call(n) ||
        fold_array_call(n) || fold_double_wrapper(n))
      *changed = 1;
    break;
  case ast_new:
    fold_expr(n->a, changed);
    fold_list(&n->list, changed);
    if (fold_global_wrapper_call(n))
      *changed = 1;
    break;
  case ast_template:
    fold_list(&n->list, changed);
    if (fold_template(n))
      *changed = 1;
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
  g_math_shadowed = bound_in_stmt(program, "Math");
  g_number_shadowed = bound_in_stmt(program, "Number");
  g_string_shadowed = bound_in_stmt(program, "String");
  g_boolean_shadowed = bound_in_stmt(program, "Boolean");
  int changed = 0;
  fold_stmt(program, &changed);
  return changed;
}
