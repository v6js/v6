#include "v6/optimizer_print.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define prec_seq 1
#define prec_assign 2
#define prec_cond 3
#define prec_nullish 4
#define prec_or 5
#define prec_and 6
#define prec_bitor 7
#define prec_bitxor 8
#define prec_bitand 9
#define prec_eq 10
#define prec_rel 11
#define prec_shift 12
#define prec_add 13
#define prec_mul 14
#define prec_exp 15
#define prec_unary 16
#define prec_postfix 17
#define prec_call 18
#define prec_primary 19

static void print_stmt(v6_opt_buf* out, ast_node* node,
                       const v6_opt_print_opts* opts, int level);
static void print_stmt_list(v6_opt_buf* out, ast_list* list,
                            const v6_opt_print_opts* opts, int level);
static void print_expr_prec(v6_opt_buf* out, ast_node* node, int min_prec,
                            const v6_opt_print_opts* opts);
static void print_pattern(v6_opt_buf* out, ast_node* node,
                          const v6_opt_print_opts* opts);
static void print_function_like(v6_opt_buf* out, ast_node* node,
                                const v6_opt_print_opts* opts, int level,
                                int method_shorthand);
static void print_class(v6_opt_buf* out, ast_node* node,
                        const v6_opt_print_opts* opts, int level,
                        int as_statement);
static void print_block(v6_opt_buf* out, ast_node* node,
                        const v6_opt_print_opts* opts, int level);
static int stmt_leftmost_needs_paren(ast_node* node);

static int g_stmt_level = 0;

static int is_word_char(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '_' || c == '$';
}

static int is_digit_char(char c) {
  return c >= '0' && c <= '9';
}

static void safe_emit(v6_opt_buf* out, const char* s, size_t n) {
  if (n == 0)
    return;
  if (out->len > 0) {
    char last = out->data[out->len - 1];
    char next = s[0];
    int hazard = 0;
    if (is_word_char(last) && is_word_char(next))
      hazard = 1;
    else if (last == '+' && next == '+')
      hazard = 1;
    else if (last == '-' && next == '-')
      hazard = 1;
    else if (last == '/' && next == '/')
      hazard = 1;
    else if (last == '/' && next == '*')
      hazard = 1;
    else if (is_digit_char(last) && next == '.')
      hazard = 1;
    if (hazard)
      v6_opt_buf_append_char(out, ' ');
  }
  v6_opt_buf_append(out, s, n);
}

static void emit_cstr(v6_opt_buf* out, const char* s) {
  safe_emit(out, s, strlen(s));
}

static void emit_char(v6_opt_buf* out, char c) {
  safe_emit(out, &c, 1);
}

static void indent(v6_opt_buf* out, const v6_opt_print_opts* opts, int level) {
  if (opts->minify)
    return;
  int n = level * (opts->indent_size > 0 ? opts->indent_size : 2);
  for (int i = 0; i < n; i++)
    v6_opt_buf_append_char(out, ' ');
}

static void nl(v6_opt_buf* out, const v6_opt_print_opts* opts) {
  if (!opts->minify)
    v6_opt_buf_append_char(out, '\n');
}

static void space(v6_opt_buf* out, const v6_opt_print_opts* opts) {
  if (!opts->minify)
    v6_opt_buf_append_char(out, ' ');
}

int v6_opt_is_valid_ident_name(const char* s, size_t len) {
  if (len == 0)
    return 0;
  size_t start = 0;
  if (s[0] == '#') {
    if (len == 1)
      return 0;
    start = 1;
  }
  for (size_t i = start; i < len; i++) {
    char c = s[i];
    int alpha = c == '_' || c == '$' || (c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z');
    int digit = c >= '0' && c <= '9';
    if (i == start && !alpha)
      return 0;
    if (i > start && !alpha && !digit)
      return 0;
  }
  return 1;
}

void v6_opt_print_string_literal(v6_opt_buf* out, const char* s, size_t len) {
  int has_dq = 0, has_sq = 0;
  for (size_t i = 0; i < len; i++) {
    if (s[i] == '"')
      has_dq = 1;
    else if (s[i] == '\'')
      has_sq = 1;
  }
  char quote = (has_dq && !has_sq) ? '\'' : '"';
  v6_opt_buf_append_char(out, quote);
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c == (unsigned char)quote || c == '\\') {
      v6_opt_buf_append_char(out, '\\');
      v6_opt_buf_append_char(out, (char)c);
    } else if (c == '\n') {
      v6_opt_buf_append_cstr(out, "\\n");
    } else if (c == '\r') {
      v6_opt_buf_append_cstr(out, "\\r");
    } else if (c == '\t') {
      v6_opt_buf_append_cstr(out, "\\t");
    } else if (c < 0x20) {
      v6_opt_buf_append_fmt(out, "\\x%02x", c);
    } else {
      v6_opt_buf_append_char(out, (char)c);
    }
  }
  v6_opt_buf_append_char(out, quote);
}

static void print_template_raw_chunk(v6_opt_buf* out, const char* s,
                                     size_t len) {
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c == '`' || c == '\\') {
      v6_opt_buf_append_char(out, '\\');
      v6_opt_buf_append_char(out, (char)c);
    } else if (c == '$' && i + 1 < len && s[i + 1] == '{') {
      v6_opt_buf_append_cstr(out, "\\${");
      i++;
    } else {
      v6_opt_buf_append_char(out, (char)c);
    }
  }
}

void v6_opt_print_number(v6_opt_buf* out, double val) {
  if (isnan(val)) {
    emit_cstr(out, "NaN");
    return;
  }
  if (isinf(val)) {
    emit_cstr(out, val < 0 ? "-Infinity" : "Infinity");
    return;
  }
  if (val == 0.0) {
    emit_cstr(out, signbit(val) ? "-0" : "0");
    return;
  }

  int negative = val < 0;
  double abs_val = negative ? -val : val;

  char ebuf[64];
  int prec = 0;
  for (; prec <= 16; prec++) {
    snprintf(ebuf, sizeof(ebuf), "%.*e", prec, abs_val);
    if (strtod(ebuf, NULL) == abs_val)
      break;
  }

  char* e_pos = strchr(ebuf, 'e');
  int exp_val = (int)strtol(e_pos + 1, NULL, 10);

  char digit_buf[24];
  int k = 0;
  for (char* p = ebuf; p < e_pos; p++) {
    if (*p >= '0' && *p <= '9')
      digit_buf[k++] = *p;
  }
  int n = exp_val + 1;

  char final[64];
  int fl = 0;

  if (negative)
    final[fl++] = '-';

  if (n >= 1 && n <= 21) {
    if (k <= n) {
      memcpy(final + fl, digit_buf, (size_t)k);
      fl += k;
      for (int i = 0; i < n - k; i++)
        final[fl++] = '0';
    } else {
      memcpy(final + fl, digit_buf, (size_t)n);
      fl += n;
      final[fl++] = '.';
      memcpy(final + fl, digit_buf + n, (size_t)(k - n));
      fl += k - n;
    }
  } else if (n >= -5 && n <= 0) {
    final[fl++] = '0';
    final[fl++] = '.';
    for (int i = 0; i < -n; i++)
      final[fl++] = '0';
    memcpy(final + fl, digit_buf, (size_t)k);
    fl += k;
  } else {
    final[fl++] = digit_buf[0];
    if (k > 1) {
      final[fl++] = '.';
      memcpy(final + fl, digit_buf + 1, (size_t)(k - 1));
      fl += k - 1;
    }
    final[fl++] = 'e';
    int ee = n - 1;
    final[fl++] = ee >= 0 ? '+' : '-';
    if (ee < 0)
      ee = -ee;
    char expdigits[16];
    int el = snprintf(expdigits, sizeof(expdigits), "%d", ee);
    memcpy(final + fl, expdigits, (size_t)el);
    fl += el;
  }

  final[fl] = '\0';
  emit_cstr(out, final);
}

static void print_object_key(v6_opt_buf* out, ast_node* key, int computed,
                             const v6_opt_print_opts* opts) {
  if (computed) {
    emit_char(out, '[');
    print_expr_prec(out, key, prec_assign, opts);
    emit_char(out, ']');
    return;
  }
  if (key->kind == ast_str &&
      v6_opt_is_valid_ident_name(key->str, key->str_len)) {
    safe_emit(out, key->str, key->str_len);
  } else if (key->kind == ast_str) {
    v6_opt_print_string_literal(out, key->str, key->str_len);
  } else {
    print_expr_prec(out, key, prec_assign, opts);
  }
}

static void print_params(v6_opt_buf* out, ast_param_list* params,
                         const v6_opt_print_opts* opts) {
  emit_char(out, '(');
  for (int i = 0; i < params->len; i++) {
    if (i > 0) {
      emit_char(out, ',');
      space(out, opts);
    }
    if (params->items[i].is_rest)
      emit_cstr(out, "...");
    print_pattern(out, params->items[i].pattern, opts);
  }
  emit_char(out, ')');
}

static void print_args(v6_opt_buf* out, ast_list* args,
                       const v6_opt_print_opts* opts) {
  emit_char(out, '(');
  for (int i = 0; i < args->len; i++) {
    if (i > 0) {
      emit_char(out, ',');
      space(out, opts);
    }
    ast_node* arg = args->items[i];
    if (arg->kind == ast_spread) {
      emit_cstr(out, "...");
      print_expr_prec(out, arg->a, prec_assign, opts);
    } else {
      print_expr_prec(out, arg, prec_assign, opts);
    }
  }
  emit_char(out, ')');
}

static void print_template_body(v6_opt_buf* out, ast_node* node,
                                const v6_opt_print_opts* opts) {
  emit_char(out, '`');
  for (int i = 0; i < node->quasis_cooked.len; i++) {
    ast_node* q = node->quasis_raw.items[i];
    print_template_raw_chunk(out, q->str, q->str_len);
    if (i < node->list.len) {
      v6_opt_buf_append_cstr(out, "${");
      print_expr_prec(out, node->list.items[i], prec_assign, opts);
      v6_opt_buf_append_char(out, '}');
    }
  }
  emit_char(out, '`');
}

static void print_pattern(v6_opt_buf* out, ast_node* node,
                          const v6_opt_print_opts* opts) {
  if (!node)
    return;
  switch (node->kind) {
  case ast_pat_ident:
    safe_emit(out, node->str, node->str_len);
    break;
  case ast_pat_hole:
    break;
  case ast_pat_rest:
    emit_cstr(out, "...");
    print_pattern(out, node->a, opts);
    break;
  case ast_pat_assign:
    print_pattern(out, node->a, opts);
    space(out, opts);
    emit_char(out, '=');
    space(out, opts);
    print_expr_prec(out, node->b, prec_assign, opts);
    break;
  case ast_pat_array:
    emit_char(out, '[');
    for (int i = 0; i < node->list.len; i++) {
      if (i > 0) {
        emit_char(out, ',');
        space(out, opts);
      }
      print_pattern(out, node->list.items[i], opts);
    }
    emit_char(out, ']');
    break;
  case ast_pat_object:
    emit_char(out, '{');
    for (int i = 0; i < node->props.len; i++) {
      if (i > 0) {
        emit_char(out, ',');
        space(out, opts);
      }
      ast_prop* p = &node->props.items[i];
      if (p->is_spread) {
        emit_cstr(out, "...");
        print_pattern(out, p->value, opts);
        continue;
      }
      ast_node* val = p->value;
      ast_node* bare = val;
      if (bare->kind == ast_pat_assign)
        bare = bare->a;
      int shorthand_ok = p->key->kind == ast_str &&
                         bare->kind == ast_pat_ident &&
                         bare->str_len == p->key->str_len &&
                         memcmp(bare->str, p->key->str, p->key->str_len) == 0;
      if (shorthand_ok) {
        print_pattern(out, val, opts);
      } else {
        print_object_key(out, p->key, p->computed, opts);
        emit_char(out, ':');
        space(out, opts);
        print_pattern(out, val, opts);
      }
    }
    emit_char(out, '}');
    break;
  default:
    print_expr_prec(out, node, prec_assign, opts);
    break;
  }
}

static int binop_prec(tok_kind op) {
  switch (op) {
  case tok_pipe_pipe:
    return prec_or;
  case tok_amp_amp:
    return prec_and;
  case tok_question_question:
    return prec_nullish;
  case tok_pipe:
    return prec_bitor;
  case tok_caret:
    return prec_bitxor;
  case tok_amp:
    return prec_bitand;
  case tok_eq:
  case tok_neq:
  case tok_eq_strict:
  case tok_neq_strict:
    return prec_eq;
  case tok_lt:
  case tok_gt:
  case tok_le:
  case tok_ge:
  case tok_kw_instanceof:
  case tok_kw_in:
    return prec_rel;
  case tok_shl:
  case tok_shr:
  case tok_ushr:
    return prec_shift;
  case tok_plus:
  case tok_minus:
    return prec_add;
  case tok_star:
  case tok_slash:
  case tok_percent:
    return prec_mul;
  case tok_star_star:
    return prec_exp;
  default:
    return prec_primary;
  }
}

static const char* binop_text(tok_kind op) {
  switch (op) {
  case tok_plus:
    return "+";
  case tok_minus:
    return "-";
  case tok_star:
    return "*";
  case tok_slash:
    return "/";
  case tok_percent:
    return "%";
  case tok_star_star:
    return "**";
  case tok_shl:
    return "<<";
  case tok_shr:
    return ">>";
  case tok_ushr:
    return ">>>";
  case tok_lt:
    return "<";
  case tok_gt:
    return ">";
  case tok_le:
    return "<=";
  case tok_ge:
    return ">=";
  case tok_kw_instanceof:
    return "instanceof";
  case tok_kw_in:
    return "in";
  case tok_eq:
    return "==";
  case tok_neq:
    return "!=";
  case tok_eq_strict:
    return "===";
  case tok_neq_strict:
    return "!==";
  case tok_amp:
    return "&";
  case tok_caret:
    return "^";
  case tok_pipe:
    return "|";
  case tok_amp_amp:
    return "&&";
  case tok_pipe_pipe:
    return "||";
  case tok_question_question:
    return "??";
  default:
    return "";
  }
}

static const char* assign_text(tok_kind op) {
  switch (op) {
  case tok_assign:
    return "=";
  case tok_plus_eq:
    return "+=";
  case tok_minus_eq:
    return "-=";
  case tok_star_eq:
    return "*=";
  case tok_slash_eq:
    return "/=";
  case tok_percent_eq:
    return "%=";
  case tok_amp_eq:
    return "&=";
  case tok_pipe_eq:
    return "|=";
  case tok_caret_eq:
    return "^=";
  case tok_shl_eq:
    return "<<=";
  case tok_shr_eq:
    return ">>=";
  case tok_ushr_eq:
    return ">>>=";
  case tok_star_star_eq:
    return "**=";
  case tok_amp_amp_eq:
    return "&&=";
  case tok_pipe_pipe_eq:
    return "||=";
  case tok_question_question_eq:
    return "?\?=";
  default:
    return "=";
  }
}

static int expr_own_prec(ast_node* node) {
  switch (node->kind) {
  case ast_seq:
    return prec_seq;
  case ast_assign:
  case ast_yield:
    return prec_assign;
  case ast_func_expr:
    return node->flag_a ? prec_assign : prec_primary;
  case ast_cond:
    return prec_cond;
  case ast_binary:
  case ast_logical:
    return binop_prec(node->op);
  case ast_unary:
    return prec_unary;
  case ast_update:
    return node->flag_a ? prec_unary : prec_postfix;
  case ast_await:
    return prec_unary;
  case ast_call:
  case ast_new:
  case ast_member:
  case ast_tagged_template:
  case ast_super_call:
  case ast_super_member:
    return prec_call;
  case ast_paren_pattern_assign:
    return prec_assign;
  default:
    return prec_primary;
  }
}

static int is_object_like_leftmost(ast_node* node);

static int is_object_like_leftmost(ast_node* node) {
  if (!node)
    return 0;
  switch (node->kind) {
  case ast_object_lit:
  case ast_func_expr:
  case ast_class_expr:
    return 1;
  case ast_binary:
  case ast_logical:
  case ast_assign:
    return is_object_like_leftmost(node->a);
  case ast_seq:
    return node->list.len > 0 && is_object_like_leftmost(node->list.items[0]);
  case ast_cond:
    return is_object_like_leftmost(node->a);
  case ast_member:
  case ast_call:
  case ast_tagged_template:
    return is_object_like_leftmost(node->a);
  case ast_update:
    return !node->flag_a && is_object_like_leftmost(node->a);
  case ast_paren_pattern_assign:
    return node->a && node->a->kind == ast_pat_object;
  default:
    return 0;
  }
}

static void print_expr_prec(v6_opt_buf* out, ast_node* node, int min_prec,
                            const v6_opt_print_opts* opts) {
  if (!node)
    return;

  int own_prec = expr_own_prec(node);
  int force_parens = 0;

  if (node->kind == ast_logical && node->op == tok_question_question) {
    if ((node->a->kind == ast_logical &&
         (node->a->op == tok_amp_amp || node->a->op == tok_pipe_pipe)) ||
        (node->b->kind == ast_logical &&
         (node->b->op == tok_amp_amp || node->b->op == tok_pipe_pipe)))
      force_parens = 1;
  }

  int need_parens = force_parens || own_prec < min_prec;
  if (need_parens)
    emit_char(out, '(');

  switch (node->kind) {
  case ast_num:
    v6_opt_print_number(out, node->num);
    break;
  case ast_bigint:
    safe_emit(out, node->str, node->str_len);
    v6_opt_buf_append_char(out, 'n');
    break;
  case ast_str:
    v6_opt_print_string_literal(out, node->str, node->str_len);
    break;
  case ast_template:
    print_template_body(out, node, opts);
    break;
  case ast_tagged_template:
    print_expr_prec(out, node->a, prec_call, opts);
    print_template_body(out, node, opts);
    break;
  case ast_regex:
    emit_char(out, '/');
    v6_opt_buf_append(out, node->str, node->str_len);
    emit_char(out, '/');
    v6_opt_buf_append(out, node->str2, node->str2_len);
    break;
  case ast_bool:
    emit_cstr(out, node->flag_a ? "true" : "false");
    break;
  case ast_null:
    emit_cstr(out, "null");
    break;
  case ast_undef:
    emit_cstr(out, "void 0");
    break;
  case ast_ident:
    safe_emit(out, node->str, node->str_len);
    break;
  case ast_this:
    emit_cstr(out, "this");
    break;
  case ast_new_target:
    emit_cstr(out, "new.target");
    break;
  case ast_super_call:
    emit_cstr(out, "super");
    print_args(out, &node->list, opts);
    break;
  case ast_super_member:
    emit_cstr(out, "super");
    emit_char(out, '.');
    v6_opt_buf_append(out, node->str, node->str_len);
    break;
  case ast_array_lit:
    emit_char(out, '[');
    for (int i = 0; i < node->list.len; i++) {
      if (i > 0) {
        emit_char(out, ',');
        space(out, opts);
      }
      ast_node* el = node->list.items[i];
      if (el->kind == ast_pat_hole)
        continue;
      if (el->kind == ast_spread) {
        emit_cstr(out, "...");
        print_expr_prec(out, el->a, prec_assign, opts);
      } else {
        print_expr_prec(out, el, prec_assign, opts);
      }
    }
    emit_char(out, ']');
    break;
  case ast_object_lit: {
    emit_char(out, '{');
    for (int i = 0; i < node->props.len; i++) {
      if (i > 0) {
        emit_char(out, ',');
        space(out, opts);
      }
      ast_prop* p = &node->props.items[i];
      if (p->is_spread) {
        emit_cstr(out, "...");
        print_expr_prec(out, p->value, prec_assign, opts);
        continue;
      }
      if (p->is_getter || p->is_setter) {
        emit_cstr(out, p->is_getter ? "get" : "set");
        emit_char(out, ' ');
        print_object_key(out, p->key, p->computed, opts);
        print_function_like(out, p->value, opts, g_stmt_level, 1);
        continue;
      }
      if (p->is_method) {
        if (p->is_async)
          emit_cstr(out, "async ");
        if (p->is_generator)
          emit_char(out, '*');
        print_object_key(out, p->key, p->computed, opts);
        print_function_like(out, p->value, opts, g_stmt_level, 1);
        continue;
      }
      int shorthand_ok =
          !p->computed && p->key->kind == ast_str &&
          p->value->kind == ast_ident && p->value->str_len == p->key->str_len &&
          memcmp(p->value->str, p->key->str, p->key->str_len) == 0;
      if (shorthand_ok) {
        safe_emit(out, p->key->str, p->key->str_len);
      } else {
        print_object_key(out, p->key, p->computed, opts);
        emit_char(out, ':');
        space(out, opts);
        print_expr_prec(out, p->value, prec_assign, opts);
      }
    }
    emit_char(out, '}');
    break;
  }
  case ast_func_expr:
    print_function_like(out, node, opts, g_stmt_level, 0);
    break;
  case ast_class_expr:
    print_class(out, node, opts, 0, 0);
    break;
  case ast_unary:
    if (node->op == tok_kw_typeof || node->op == tok_kw_void ||
        node->op == tok_kw_delete) {
      switch (node->op) {
      case tok_kw_typeof:
        emit_cstr(out, "typeof");
        break;
      case tok_kw_void:
        emit_cstr(out, "void");
        break;
      case tok_kw_delete:
        emit_cstr(out, "delete");
        break;
      default:
        break;
      }
      emit_char(out, ' ');
    } else {
      switch (node->op) {
      case tok_plus:
        emit_char(out, '+');
        break;
      case tok_minus:
        emit_char(out, '-');
        break;
      case tok_bang:
        emit_char(out, '!');
        break;
      case tok_tilde:
        emit_char(out, '~');
        break;
      default:
        break;
      }
    }
    {
      int operand_min = prec_unary;
      if (node->a && node->a->kind == ast_binary &&
          node->a->op == tok_star_star)
        operand_min = prec_call;
      print_expr_prec(out, node->a, operand_min, opts);
    }
    break;
  case ast_update:
    if (node->flag_a) {
      emit_cstr(out, node->op == tok_plus_plus ? "++" : "--");
      print_expr_prec(out, node->a, prec_unary, opts);
    } else {
      print_expr_prec(out, node->a, prec_postfix, opts);
      emit_cstr(out, node->op == tok_plus_plus ? "++" : "--");
    }
    break;
  case ast_binary: {
    int prec = binop_prec(node->op);
    int is_pow = node->op == tok_star_star;
    int left_min = is_pow ? prec + 1 : prec;
    int right_min = is_pow ? prec : prec + 1;
    if (is_pow && (node->a->kind == ast_unary || node->a->kind == ast_await))
      left_min = prec_call;
    print_expr_prec(out, node->a, left_min, opts);
    space(out, opts);
    emit_cstr(out, binop_text(node->op));
    space(out, opts);
    print_expr_prec(out, node->b, right_min, opts);
    break;
  }
  case ast_logical: {
    int prec = binop_prec(node->op);
    print_expr_prec(out, node->a, prec, opts);
    space(out, opts);
    emit_cstr(out, binop_text(node->op));
    space(out, opts);
    print_expr_prec(out, node->b, prec + 1, opts);
    break;
  }
  case ast_assign:
    print_expr_prec(out, node->a, prec_call, opts);
    space(out, opts);
    emit_cstr(out, assign_text(node->op));
    space(out, opts);
    print_expr_prec(out, node->b, prec_assign, opts);
    break;
  case ast_cond:
    print_expr_prec(out, node->a, prec_nullish, opts);
    space(out, opts);
    emit_char(out, '?');
    space(out, opts);
    print_expr_prec(out, node->b, prec_assign, opts);
    space(out, opts);
    emit_char(out, ':');
    space(out, opts);
    print_expr_prec(out, node->c, prec_assign, opts);
    break;
  case ast_call:
    print_expr_prec(out, node->a, prec_call, opts);
    if (node->flag_a)
      emit_cstr(out, "?.");
    print_args(out, &node->list, opts);
    break;
  case ast_new: {
    emit_cstr(out, "new ");
    int callee_min = prec_call;
    if (node->a->kind != ast_ident && node->a->kind != ast_this &&
        node->a->kind != ast_member)
      callee_min = prec_primary + 1;
    print_expr_prec(out, node->a, callee_min, opts);
    print_args(out, &node->list, opts);
    break;
  }
  case ast_member:
    print_expr_prec(out, node->a, prec_call, opts);
    if (node->flag_a) {
      if (node->flag_b)
        emit_cstr(out, "?.");
      emit_char(out, '[');
      print_expr_prec(out, node->b, prec_assign, opts);
      emit_char(out, ']');
    } else {
      emit_cstr(out, node->flag_b ? "?." : ".");
      v6_opt_buf_append(out, node->str, node->str_len);
    }
    break;
  case ast_seq:
    for (int i = 0; i < node->list.len; i++) {
      if (i > 0) {
        emit_char(out, ',');
        space(out, opts);
      }
      print_expr_prec(out, node->list.items[i], prec_assign, opts);
    }
    break;
  case ast_spread:
    emit_cstr(out, "...");
    print_expr_prec(out, node->a, prec_assign, opts);
    break;
  case ast_yield:
    emit_cstr(out, node->flag_a ? "yield*" : "yield");
    if (node->a) {
      emit_char(out, ' ');
      print_expr_prec(out, node->a, prec_assign, opts);
    }
    break;
  case ast_await:
    emit_cstr(out, "await ");
    print_expr_prec(out, node->a, prec_unary, opts);
    break;
  case ast_paren_pattern_assign:
    print_pattern(out, node->a, opts);
    space(out, opts);
    emit_char(out, '=');
    space(out, opts);
    print_expr_prec(out, node->b, prec_assign, opts);
    break;
  default:
    break;
  }

  if (need_parens)
    emit_char(out, ')');
}

void v6_opt_print_expr(v6_opt_buf* out, ast_node* node,
                       const v6_opt_print_opts* opts) {
  print_expr_prec(out, node, prec_seq, opts);
}

static void print_function_like(v6_opt_buf* out, ast_node* node,
                                const v6_opt_print_opts* opts, int level,
                                int method_shorthand) {
  if (!method_shorthand) {
    if (node->flag_b)
      emit_cstr(out, "async ");
    if (!node->flag_a) {
      emit_cstr(out, "function");
      if (node->flag_c)
        emit_char(out, '*');
      if (node->str_len > 0) {
        emit_char(out, ' ');
        v6_opt_buf_append(out, node->str, node->str_len);
      } else {
        space(out, opts);
      }
    }
  }
  print_params(out, &node->params, opts);
  if (node->flag_a) {
    space(out, opts);
    emit_cstr(out, "=>");
  }
  if (node->flag_d) {
    space(out, opts);
    print_block(out, node->a, opts, level);
  } else {
    if (node->flag_a)
      space(out, opts);
    if (node->flag_a && is_object_like_leftmost(node->a)) {
      emit_char(out, '(');
      print_expr_prec(out, node->a, prec_assign, opts);
      emit_char(out, ')');
    } else {
      print_expr_prec(out, node->a, prec_assign, opts);
    }
  }
}

static void print_class_member(v6_opt_buf* out, ast_class_member* m,
                               const v6_opt_print_opts* opts, int level) {
  g_stmt_level = level;
  indent(out, opts, level);
  if (m->is_static)
    emit_cstr(out, "static ");
  if (m->is_getter || m->is_setter) {
    emit_cstr(out, m->is_getter ? "get " : "set ");
    print_object_key(out, m->key, m->computed, opts);
    print_function_like(out, m->value, opts, level, 1);
  } else if (m->is_field) {
    print_object_key(out, m->key, m->computed, opts);
    if (m->value) {
      space(out, opts);
      emit_char(out, '=');
      space(out, opts);
      print_expr_prec(out, m->value, prec_assign, opts);
    }
    emit_char(out, ';');
  } else {
    if (m->is_async)
      emit_cstr(out, "async ");
    if (m->is_generator)
      emit_char(out, '*');
    print_object_key(out, m->key, m->computed, opts);
    print_function_like(out, m->value, opts, level, 1);
  }
  nl(out, opts);
}

static void print_class(v6_opt_buf* out, ast_node* node,
                        const v6_opt_print_opts* opts, int level,
                        int as_statement) {
  (void)as_statement;
  emit_cstr(out, "class");
  if (node->str_len > 0) {
    emit_char(out, ' ');
    v6_opt_buf_append(out, node->str, node->str_len);
  }
  if (node->flag_a) {
    emit_cstr(out, " extends ");
    print_expr_prec(out, node->a, prec_call, opts);
  }
  space(out, opts);
  emit_char(out, '{');
  nl(out, opts);
  for (int i = 0; i < node->members.len; i++)
    print_class_member(out, &node->members.items[i], opts, level + 1);
  indent(out, opts, level);
  emit_char(out, '}');
}

static void print_var_decl(v6_opt_buf* out, ast_node* node,
                           const v6_opt_print_opts* opts) {
  switch (node->flag_a) {
  case tok_kw_let:
    emit_cstr(out, "let ");
    break;
  case tok_kw_const:
    emit_cstr(out, "const ");
    break;
  default:
    emit_cstr(out, "var ");
    break;
  }
  for (int i = 0; i < node->list.len; i++) {
    if (i > 0) {
      emit_char(out, ',');
      space(out, opts);
    }
    ast_node* d = node->list.items[i];
    if (d->kind == ast_pat_assign) {
      print_pattern(out, d->a, opts);
      space(out, opts);
      emit_char(out, '=');
      space(out, opts);
      print_expr_prec(out, d->b, prec_assign, opts);
    } else {
      print_pattern(out, d, opts);
    }
  }
}

static void print_block(v6_opt_buf* out, ast_node* node,
                        const v6_opt_print_opts* opts, int level) {
  emit_char(out, '{');
  nl(out, opts);
  print_stmt_list(out, &node->list, opts, level + 1);
  indent(out, opts, level);
  emit_char(out, '}');
}

static void print_body_as_block(v6_opt_buf* out, ast_node* body,
                                const v6_opt_print_opts* opts, int level) {
  if (!body || body->kind == ast_empty) {
    emit_cstr(out, "{}");
    return;
  }
  if (body->kind == ast_block) {
    print_block(out, body, opts, level);
    return;
  }
  emit_char(out, '{');
  nl(out, opts);
  print_stmt(out, body, opts, level + 1);
  indent(out, opts, level);
  emit_char(out, '}');
}

static int stmt_leftmost_needs_paren(ast_node* node) {
  if (!node)
    return 0;
  if (node->kind != ast_expr_stmt)
    return 0;
  return is_object_like_leftmost(node->a);
}

static void print_stmt_ex(v6_opt_buf* out, ast_node* node,
                          const v6_opt_print_opts* opts, int level,
                          int skip_indent) {
  if (!node)
    return;
  g_stmt_level = level;
  if (!skip_indent)
    indent(out, opts, level);
  switch (node->kind) {
  case ast_empty:
    emit_char(out, ';');
    nl(out, opts);
    break;
  case ast_debugger:
    emit_cstr(out, "debugger;");
    nl(out, opts);
    break;
  case ast_block:
    print_block(out, node, opts, level);
    nl(out, opts);
    break;
  case ast_expr_stmt: {
    int wrap = stmt_leftmost_needs_paren(node);
    if (wrap)
      emit_char(out, '(');
    print_expr_prec(out, node->a, prec_seq, opts);
    if (wrap)
      emit_char(out, ')');
    emit_char(out, ';');
    nl(out, opts);
    break;
  }
  case ast_paren_pattern_assign: {
    int wrap = node->a && node->a->kind == ast_pat_object;
    if (wrap)
      emit_char(out, '(');
    print_pattern(out, node->a, opts);
    space(out, opts);
    emit_char(out, '=');
    space(out, opts);
    print_expr_prec(out, node->b, prec_assign, opts);
    if (wrap)
      emit_char(out, ')');
    emit_char(out, ';');
    nl(out, opts);
    break;
  }
  case ast_var_decl:
    print_var_decl(out, node, opts);
    emit_char(out, ';');
    nl(out, opts);
    break;
  case ast_func_decl:
    print_function_like(out, node, opts, level, 0);
    nl(out, opts);
    break;
  case ast_class_decl:
    print_class(out, node, opts, level, 1);
    nl(out, opts);
    break;
  case ast_if:
    emit_cstr(out, "if");
    space(out, opts);
    emit_char(out, '(');
    print_expr_prec(out, node->a, prec_seq, opts);
    emit_char(out, ')');
    space(out, opts);
    print_body_as_block(out, node->b, opts, level);
    if (node->c) {
      space(out, opts);
      emit_cstr(out, "else");
      if (node->c->kind == ast_if) {
        emit_char(out, ' ');
        ast_node* elseif = node->c;
        emit_cstr(out, "if");
        space(out, opts);
        emit_char(out, '(');
        print_expr_prec(out, elseif->a, prec_seq, opts);
        emit_char(out, ')');
        space(out, opts);
        print_body_as_block(out, elseif->b, opts, level);
        if (elseif->c) {
          space(out, opts);
          emit_cstr(out, "else");
          space(out, opts);
          print_body_as_block(out, elseif->c, opts, level);
        }
      } else {
        space(out, opts);
        print_body_as_block(out, node->c, opts, level);
      }
    }
    nl(out, opts);
    break;
  case ast_while:
    emit_cstr(out, "while");
    space(out, opts);
    emit_char(out, '(');
    print_expr_prec(out, node->a, prec_seq, opts);
    emit_char(out, ')');
    space(out, opts);
    print_body_as_block(out, node->b, opts, level);
    nl(out, opts);
    break;
  case ast_do_while:
    emit_cstr(out, "do");
    space(out, opts);
    print_body_as_block(out, node->a, opts, level);
    space(out, opts);
    emit_cstr(out, "while");
    space(out, opts);
    emit_char(out, '(');
    print_expr_prec(out, node->b, prec_seq, opts);
    emit_cstr(out, ");");
    nl(out, opts);
    break;
  case ast_for: {
    emit_cstr(out, "for");
    space(out, opts);
    emit_char(out, '(');
    if (node->a) {
      if (node->a->kind == ast_var_decl) {
        print_var_decl(out, node->a, opts);
      } else {
        int wrap_in = node->a->kind == ast_binary && node->a->op == tok_kw_in;
        if (wrap_in)
          emit_char(out, '(');
        print_expr_prec(out, node->a, prec_seq, opts);
        if (wrap_in)
          emit_char(out, ')');
      }
    }
    emit_char(out, ';');
    space(out, opts);
    if (node->b)
      print_expr_prec(out, node->b, prec_seq, opts);
    emit_char(out, ';');
    space(out, opts);
    if (node->c)
      print_expr_prec(out, node->c, prec_seq, opts);
    emit_char(out, ')');
    space(out, opts);
    print_body_as_block(out, node->d, opts, level);
    nl(out, opts);
    break;
  }
  case ast_for_in:
  case ast_for_of: {
    emit_cstr(out, "for");
    space(out, opts);
    if (node->flag_b)
      emit_cstr(out, "await ");
    emit_char(out, '(');
    if (node->flag_a == tok_kw_var)
      emit_cstr(out, "var ");
    else if (node->flag_a == tok_kw_let)
      emit_cstr(out, "let ");
    else if (node->flag_a == tok_kw_const)
      emit_cstr(out, "const ");
    print_pattern(out, node->a, opts);
    emit_cstr(out, node->kind == ast_for_in ? " in " : " of ");
    print_expr_prec(out, node->b,
                    node->kind == ast_for_in ? prec_seq : prec_assign, opts);
    emit_char(out, ')');
    space(out, opts);
    print_body_as_block(out, node->c, opts, level);
    nl(out, opts);
    break;
  }
  case ast_switch: {
    emit_cstr(out, "switch");
    space(out, opts);
    emit_char(out, '(');
    print_expr_prec(out, node->a, prec_seq, opts);
    emit_char(out, ')');
    space(out, opts);
    emit_char(out, '{');
    nl(out, opts);
    for (int i = 0; i < node->cases.len; i++) {
      ast_switch_case* c = &node->cases.items[i];
      indent(out, opts, level + 1);
      if (c->test) {
        emit_cstr(out, "case ");
        print_expr_prec(out, c->test, prec_seq, opts);
      } else {
        emit_cstr(out, "default");
      }
      emit_char(out, ':');
      nl(out, opts);
      print_stmt_list(out, &c->body, opts, level + 2);
    }
    indent(out, opts, level);
    emit_char(out, '}');
    nl(out, opts);
    break;
  }
  case ast_try:
    emit_cstr(out, "try");
    space(out, opts);
    print_block(out, node->a, opts, level);
    if (node->flag_a) {
      space(out, opts);
      emit_cstr(out, "catch");
      if (node->b) {
        space(out, opts);
        emit_char(out, '(');
        print_pattern(out, node->b, opts);
        emit_char(out, ')');
      }
      space(out, opts);
      print_block(out, node->c, opts, level);
    }
    if (node->flag_b) {
      space(out, opts);
      emit_cstr(out, "finally");
      space(out, opts);
      print_block(out, node->d, opts, level);
    }
    nl(out, opts);
    break;
  case ast_throw:
    emit_cstr(out, "throw ");
    print_expr_prec(out, node->a, prec_seq, opts);
    emit_char(out, ';');
    nl(out, opts);
    break;
  case ast_return:
    emit_cstr(out, "return");
    if (node->a) {
      emit_char(out, ' ');
      print_expr_prec(out, node->a, prec_seq, opts);
    }
    emit_char(out, ';');
    nl(out, opts);
    break;
  case ast_break:
    emit_cstr(out, "break");
    if (node->str_len > 0) {
      emit_char(out, ' ');
      v6_opt_buf_append(out, node->str, node->str_len);
    }
    emit_char(out, ';');
    nl(out, opts);
    break;
  case ast_continue:
    emit_cstr(out, "continue");
    if (node->str_len > 0) {
      emit_char(out, ' ');
      v6_opt_buf_append(out, node->str, node->str_len);
    }
    emit_char(out, ';');
    nl(out, opts);
    break;
  case ast_labeled:
    v6_opt_buf_append(out, node->str, node->str_len);
    emit_char(out, ':');
    space(out, opts);
    print_stmt_ex(out, node->a, opts, level, 1);
    break;
  case ast_import:
    emit_cstr(out, "import ");
    if (node->flag_a) {
      v6_opt_print_string_literal(out, node->str, node->str_len);
      emit_char(out, ';');
      nl(out, opts);
      break;
    }
    if (node->import_binding) {
      ast_import_binding* ib = node->import_binding;
      int wrote = 0;
      if (ib->has_default) {
        v6_opt_buf_append(out, ib->default_name, ib->default_len);
        wrote = 1;
      }
      if (ib->has_namespace) {
        if (wrote) {
          emit_char(out, ',');
          space(out, opts);
        }
        emit_cstr(out, "* as ");
        v6_opt_buf_append(out, ib->namespace_name, ib->namespace_len);
        wrote = 1;
      } else if (ib->named_count > 0 || !ib->has_default) {
        if (wrote) {
          emit_char(out, ',');
          space(out, opts);
        }
        emit_char(out, '{');
        for (int i = 0; i < ib->named_count; i++) {
          if (i > 0) {
            emit_char(out, ',');
            space(out, opts);
          }
          v6_opt_buf_append(out, ib->named[i].key, ib->named[i].key_len);
          if (ib->named[i].local_len != ib->named[i].key_len ||
              memcmp(ib->named[i].local, ib->named[i].key,
                     ib->named[i].key_len) != 0) {
            emit_cstr(out, " as ");
            v6_opt_buf_append(out, ib->named[i].local, ib->named[i].local_len);
          }
        }
        emit_char(out, '}');
        wrote = 1;
      }
      if (wrote)
        emit_cstr(out, " from ");
    }
    v6_opt_print_string_literal(out, node->str, node->str_len);
    emit_char(out, ';');
    nl(out, opts);
    break;
  default:
    print_expr_prec(out, node, prec_seq, opts);
    emit_char(out, ';');
    nl(out, opts);
    break;
  }
}

static void print_stmt(v6_opt_buf* out, ast_node* node,
                       const v6_opt_print_opts* opts, int level) {
  print_stmt_ex(out, node, opts, level, 0);
}

static void print_stmt_list(v6_opt_buf* out, ast_list* list,
                            const v6_opt_print_opts* opts, int level) {
  for (int i = 0; i < list->len; i++)
    print_stmt(out, list->items[i], opts, level);
}

void v6_opt_print_program(v6_opt_buf* out, ast_node* program,
                          const v6_opt_print_opts* opts) {
  print_stmt_list(out, &program->list, opts, 0);
}
