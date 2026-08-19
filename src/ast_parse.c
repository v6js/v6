#include "v6/ast_parse.h"

#include "v6/internal.h"
#include "v6/literal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ast_arena* g_arena;

static ast_node* mk(ast_kind k, parser* p) {
  return ast_new_node(g_arena, k, p->cur.line);
}

static ast_node* ast_parse_assign(parser* p);
static ast_node* ast_parse_expr(parser* p);
static ast_node* ast_parse_seq_expr(parser* p);
static ast_node* ast_parse_stmt(parser* p);
static ast_node* ast_parse_block(parser* p);
static ast_node* ast_parse_primary(parser* p);
static ast_node* ast_parse_postfix(parser* p);
static ast_node* ast_parse_binding_pattern(parser* p);
static ast_node* ast_parse_array_pattern(parser* p);
static ast_node* ast_parse_object_pattern(parser* p);
static ast_node* ast_parse_class_body(parser* p, int is_expr);
static ast_node* ast_parse_function_like(parser* p, int is_arrow,
                                         int parens_params, int is_async,
                                         int is_gen, int is_decl);
static void ast_parse_params(parser* p, ast_param_list* out);
ast_node* ast_parse_unary_pub(parser* p);
ast_node* ast_parse_ident_or_arrow_from_prev(parser* p);

static int is_assign_op(tok_kind k) {
  switch (k) {
  case tok_assign:
  case tok_plus_eq:
  case tok_minus_eq:
  case tok_star_eq:
  case tok_slash_eq:
  case tok_percent_eq:
  case tok_amp_eq:
  case tok_pipe_eq:
  case tok_caret_eq:
  case tok_shl_eq:
  case tok_shr_eq:
  case tok_ushr_eq:
  case tok_star_star_eq:
  case tok_amp_amp_eq:
  case tok_pipe_pipe_eq:
  case tok_question_question_eq:
    return 1;
  default:
    return 0;
  }
}

static int peek_is_arrow(parser* p) {
  lexer save_lex = p->lex;
  tok after = lex_next(&p->lex);
  int is_arrow = after.kind == tok_arrow;
  p->lex = save_lex;
  return is_arrow;
}

static int peek_arrow_after_parens(parser* p) {
  lexer save_lex = p->lex;
  tok save_cur = p->cur;
  tok save_prev = p->prev;
  p->lex.auto_regex = 1;

  int depth = 0;
  tok t = p->cur;
  for (;;) {
    if (t.kind == tok_lparen) {
      depth++;
    } else if (t.kind == tok_rparen) {
      depth--;
      if (depth == 0) {
        t = lex_next(&p->lex);
        break;
      }
    } else if (t.kind == tok_eof) {
      break;
    }
    t = lex_next(&p->lex);
  }
  int is_arrow = t.kind == tok_arrow;

  p->lex = save_lex;
  p->cur = save_cur;
  p->prev = save_prev;
  return is_arrow;
}

static ast_node* str_node_decoded(tok t, parser* p) {
  ast_node* n = mk(ast_str, p);
  char* s = decode_string(t);
  n->str = s;
  n->str_len = strlen(s);
  return n;
}

static ast_node* ident_key_node(tok t, parser* p) {
  ast_node* n = mk(ast_str, p);
  char* s = dup_tok(t);
  n->str = s;
  n->str_len = strlen(s);
  return n;
}

static void ast_parse_template_parts(parser* p, tok t, ast_node* out) {
  const char* s = t.start;
  size_t len = t.len;
  size_t i = 1;
  size_t chunk_start = 1;
  while (i < len - 1) {
    if (s[i] == '\\' && i + 1 < len - 1) {
      i += 2;
      continue;
    }
    if (s[i] == '$' && i + 1 < len - 1 && s[i + 1] == '{') {
      {
        size_t chunk_len = i - chunk_start;
        char* cooked = malloc(chunk_len + 1);
        size_t j2 = 0;
        for (size_t k = 0; k < chunk_len; k++) {
          char ch = s[chunk_start + k];
          if (ch == '\\' && k + 1 < chunk_len) {
            k++;
            char e = s[chunk_start + k];
            if (e == 'n')
              cooked[j2++] = '\n';
            else if (e == 't')
              cooked[j2++] = '\t';
            else if (e == 'r')
              cooked[j2++] = '\r';
            else if (e == '`')
              cooked[j2++] = '`';
            else if (e == '$')
              cooked[j2++] = '$';
            else
              cooked[j2++] = e;
          } else {
            cooked[j2++] = ch;
          }
        }
        cooked[j2] = '\0';
        ast_node* cn = mk(ast_str, p);
        cn->str = cooked;
        cn->str_len = j2;
        ast_list_push(g_arena, &out->quasis_cooked, cn);

        char* raw = malloc(chunk_len + 1);
        memcpy(raw, s + chunk_start, chunk_len);
        raw[chunk_len] = '\0';
        ast_node* rn = mk(ast_str, p);
        rn->str = raw;
        rn->str_len = chunk_len;
        ast_list_push(g_arena, &out->quasis_raw, rn);
      }

      size_t expr_start = i + 2;
      int depth = 1;
      size_t j = expr_start;
      while (j < len - 1 && depth > 0) {
        if (s[j] == '{') {
          depth++;
        } else if (s[j] == '}') {
          depth--;
          if (depth == 0)
            break;
        } else if (s[j] == '"' || s[j] == '\'') {
          char q = s[j];
          j++;
          while (j < len - 1 && s[j] != q) {
            if (s[j] == '\\' && j + 1 < len - 1)
              j++;
            j++;
          }
        }
        j++;
      }

      parser ep;
      parser_init(&ep, s + expr_start);
      ast_node* expr = ast_parse_expr(&ep);
      ast_list_push(g_arena, &out->list, expr);

      i = j + 1;
      chunk_start = i;
      continue;
    }
    i++;
  }

  size_t chunk_len = i - chunk_start;
  char* cooked = malloc(chunk_len + 1);
  size_t j2 = 0;
  for (size_t k = 0; k < chunk_len; k++) {
    char ch = s[chunk_start + k];
    if (ch == '\\' && k + 1 < chunk_len) {
      k++;
      char e = s[chunk_start + k];
      if (e == 'n')
        cooked[j2++] = '\n';
      else if (e == 't')
        cooked[j2++] = '\t';
      else if (e == 'r')
        cooked[j2++] = '\r';
      else if (e == '`')
        cooked[j2++] = '`';
      else if (e == '$')
        cooked[j2++] = '$';
      else
        cooked[j2++] = e;
    } else {
      cooked[j2++] = ch;
    }
  }
  cooked[j2] = '\0';
  ast_node* cn = mk(ast_str, p);
  cn->str = cooked;
  cn->str_len = j2;
  ast_list_push(g_arena, &out->quasis_cooked, cn);

  char* raw = malloc(chunk_len + 1);
  memcpy(raw, s + chunk_start, chunk_len);
  raw[chunk_len] = '\0';
  ast_node* rn = mk(ast_str, p);
  rn->str = raw;
  rn->str_len = chunk_len;
  ast_list_push(g_arena, &out->quasis_raw, rn);
}

static void ast_parse_call_args(parser* p, ast_list* out) {
  if (check(p, tok_rparen)) {
    advance(p);
    return;
  }
  for (;;) {
    if (match(p, tok_ellipsis)) {
      ast_node* sp = mk(ast_spread, p);
      sp->a = ast_parse_assign(p);
      ast_list_push(g_arena, out, sp);
    } else {
      ast_list_push(g_arena, out, ast_parse_assign(p));
    }
    if (!match(p, tok_comma))
      break;
    if (check(p, tok_rparen))
      break;
  }
  expect(p, tok_rparen);
}

static ast_node* ast_parse_new(parser* p) {
  ast_node* n = mk(ast_new, p);
  ast_node* callee;
  if (match(p, tok_kw_this)) {
    callee = mk(ast_this, p);
  } else if (match(p, tok_lparen)) {
    callee = ast_parse_seq_expr(p);
    expect(p, tok_rparen);
  } else {
    if (!expect(p, tok_ident)) {
      n->a = mk(ast_undef, p);
      return n;
    }
    tok name = p->prev;
    callee = mk(ast_ident, p);
    callee->str = name.start;
    callee->str_len = name.len;
  }

  while (check(p, tok_dot) || check(p, tok_lbracket)) {
    ast_node* m = mk(ast_member, p);
    m->a = callee;
    if (match(p, tok_dot)) {
      if (!match_property_name(p)) {
        error_at(p, "expected property name");
        return n;
      }
      m->str = dup_tok(p->prev);
      m->str_len = strlen(m->str);
    } else {
      advance(p);
      m->flag_a = 1;
      m->b = ast_parse_expr(p);
      if (!expect(p, tok_rbracket))
        return n;
    }
    callee = m;
  }

  n->a = callee;
  if (match(p, tok_lparen)) {
    ast_parse_call_args(p, &n->list);
  }
  return n;
}

static ast_node* ast_parse_super_primary(parser* p) {
  if (match(p, tok_lparen)) {
    ast_node* n = mk(ast_super_call, p);
    ast_parse_call_args(p, &n->list);
    return n;
  }
  if (!expect(p, tok_dot)) {
    ast_node* n = mk(ast_undef, p);
    return n;
  }
  ast_node* n = mk(ast_super_member, p);
  if (!match_property_name(p)) {
    error_at(p, "expected property name");
    return n;
  }
  n->str = dup_tok(p->prev);
  n->str_len = strlen(n->str);
  return n;
}

static ast_node* ast_parse_regex(parser* p) {
  lexer regex_lex = p->lex;
  regex_lex.cur = p->cur.start;
  regex_lex.line = p->cur.line;
  tok regex_tok = lex_regex_literal(&regex_lex);
  p->lex = regex_lex;
  p->cur = regex_tok;
  advance(p);
  tok t = p->prev;

  ast_node* n = mk(ast_regex, p);
  const char* s = t.start;
  size_t i = 1;
  int in_class = 0;
  while (i < t.len && (in_class || s[i] != '/')) {
    if (s[i] == '\\' && i + 1 < t.len) {
      i += 2;
      continue;
    }
    if (s[i] == '[')
      in_class = 1;
    else if (s[i] == ']')
      in_class = 0;
    i++;
  }
  size_t source_len = i - 1;
  char* source = malloc(source_len + 1);
  memcpy(source, s + 1, source_len);
  source[source_len] = '\0';
  size_t flags_start = i + 1;
  size_t flags_len = (flags_start <= t.len) ? t.len - flags_start : 0;
  char* flags = malloc(flags_len + 1);
  if (flags_len)
    memcpy(flags, s + flags_start, flags_len);
  flags[flags_len] = '\0';

  n->str = source;
  n->str_len = source_len;
  n->str2 = flags;
  n->str2_len = flags_len;
  return n;
}

static ast_node* ast_parse_object_literal(parser* p) {
  ast_node* n = mk(ast_object_lit, p);
  if (check(p, tok_rbrace)) {
    advance(p);
    return n;
  }
  for (;;) {
    if (check(p, tok_rbrace))
      break;
    ast_prop prop;
    memset(&prop, 0, sizeof(prop));

    if (match(p, tok_ellipsis)) {
      prop.is_spread = 1;
      prop.value = ast_parse_assign(p);
      ast_prop_list_push(g_arena, &n->props, prop);
      if (!match(p, tok_comma))
        break;
      continue;
    }

    int is_async = 0;
    if (check(p, tok_kw_async)) {
      lexer sl = p->lex;
      tok sc = p->cur, sp = p->prev;
      advance(p);
      if (check(p, tok_colon) || check(p, tok_lparen) || check(p, tok_comma) ||
          check(p, tok_rbrace)) {
        p->lex = sl;
        p->cur = sc;
        p->prev = sp;
      } else {
        is_async = 1;
      }
    }

    int is_gen = match(p, tok_star);

    int is_getter = 0, is_setter = 0;
    if (!is_gen && !is_async &&
        (check(p, tok_kw_get) || check(p, tok_kw_set))) {
      tok_kind modifier = p->cur.kind;
      lexer sl = p->lex;
      tok sc = p->cur, sp = p->prev;
      advance(p);
      if (check(p, tok_colon) || check(p, tok_lparen) || check(p, tok_comma) ||
          check(p, tok_rbrace)) {
        p->lex = sl;
        p->cur = sc;
        p->prev = sp;
      } else {
        is_getter = modifier == tok_kw_get;
        is_setter = modifier == tok_kw_set;
      }
    }

    tok ident_tok;
    int shorthand_ok = 0;
    if (match(p, tok_lbracket)) {
      prop.computed = 1;
      prop.key = ast_parse_expr(p);
      if (!expect(p, tok_rbracket))
        break;
    } else if (check(p, tok_str)) {
      tok t = p->cur;
      advance(p);
      prop.key = str_node_decoded(t, p);
    } else if (check(p, tok_num)) {
      tok t = p->cur;
      advance(p);
      char buf[64];
      if (t.num == (double)(long long)t.num)
        snprintf(buf, sizeof(buf), "%lld", (long long)t.num);
      else
        snprintf(buf, sizeof(buf), "%g", t.num);
      ast_node* kn = mk(ast_str, p);
      char* s = malloc(strlen(buf) + 1);
      strcpy(s, buf);
      kn->str = s;
      kn->str_len = strlen(s);
      prop.key = kn;
    } else if (check(p, tok_ident)) {
      ident_tok = p->cur;
      advance(p);
      prop.key = ident_key_node(ident_tok, p);
      shorthand_ok = 1;
    } else if (match_property_name(p)) {
      prop.key = ident_key_node(p->prev, p);
    } else {
      break;
    }

    if (is_getter || is_setter) {
      if (!check(p, tok_lparen)) {
        error_at(p, "expected '('");
        break;
      }
      prop.is_getter = is_getter;
      prop.is_setter = is_setter;
      prop.value = ast_parse_function_like(p, 0, 1, 0, 0, 0);
      ast_prop_list_push(g_arena, &n->props, prop);
      if (!match(p, tok_comma))
        break;
      continue;
    }

    if (check(p, tok_lparen)) {
      prop.is_method = 1;
      prop.is_async = is_async;
      prop.is_generator = is_gen;
      prop.value = ast_parse_function_like(p, 0, 1, is_async, is_gen, 0);
    } else if (match(p, tok_colon)) {
      prop.value = ast_parse_assign(p);
    } else if (shorthand_ok && !prop.computed) {
      prop.shorthand = 1;
      ast_node* idn = mk(ast_ident, p);
      idn->str = ident_tok.start;
      idn->str_len = ident_tok.len;
      prop.value = idn;
    } else {
      error_at(p, "expected ':'");
      break;
    }
    ast_prop_list_push(g_arena, &n->props, prop);
    if (!match(p, tok_comma))
      break;
  }
  expect(p, tok_rbrace);
  return n;
}

static ast_node* ast_parse_array_literal(parser* p) {
  ast_node* n = mk(ast_array_lit, p);
  if (check(p, tok_rbracket)) {
    advance(p);
    return n;
  }
  for (;;) {
    if (check(p, tok_rbracket))
      break;
    if (check(p, tok_comma)) {
      ast_list_push(g_arena, &n->list, mk(ast_pat_hole, p));
      advance(p);
      continue;
    }
    if (match(p, tok_ellipsis)) {
      ast_node* sp = mk(ast_spread, p);
      sp->a = ast_parse_assign(p);
      ast_list_push(g_arena, &n->list, sp);
    } else {
      ast_list_push(g_arena, &n->list, ast_parse_assign(p));
    }
    if (!match(p, tok_comma))
      break;
  }
  expect(p, tok_rbracket);
  return n;
}

static ast_node* ast_parse_ident_or_arrow(parser* p) {
  if (check(p, tok_ident) && peek_is_arrow(p)) {
    return ast_parse_function_like(p, 1, 0, 0, 0, 0);
  }
  advance(p);
  tok name = p->prev;
  ast_node* n = mk(ast_ident, p);
  n->str = name.start;
  n->str_len = name.len;
  return n;
}

static ast_node* ast_parse_primary(parser* p) {
  if (match(p, tok_kw_new)) {
    if (match(p, tok_dot)) {
      if (!check(p, tok_ident) || p->cur.len != 6 ||
          memcmp(p->cur.start, "target", 6) != 0) {
        error_at(p, "expected 'target'");
        return mk(ast_undef, p);
      }
      advance(p);
      return mk(ast_new_target, p);
    }
    return ast_parse_new(p);
  }

  if (match(p, tok_kw_super)) {
    return ast_parse_super_primary(p);
  }

  if (match(p, tok_num)) {
    tok t = p->prev;
    if (t.is_bigint) {
      ast_node* n = mk(ast_bigint, p);
      size_t digits_len = t.len - 1;
      char* digits = malloc(digits_len + 1);
      memcpy(digits, t.start, digits_len);
      digits[digits_len] = '\0';
      n->str = digits;
      n->str_len = digits_len;
      return n;
    }
    ast_node* n = mk(ast_num, p);
    n->num = t.num;
    return n;
  }

  if (match(p, tok_kw_true)) {
    ast_node* n = mk(ast_bool, p);
    n->flag_a = 1;
    return n;
  }
  if (match(p, tok_kw_false)) {
    ast_node* n = mk(ast_bool, p);
    n->flag_a = 0;
    return n;
  }
  if (match(p, tok_kw_null))
    return mk(ast_null, p);
  if (match(p, tok_kw_undefined))
    return mk(ast_undef, p);

  if (check(p, tok_str)) {
    tok t = p->cur;
    advance(p);
    return str_node_decoded(t, p);
  }

  if (check(p, tok_template)) {
    tok t = p->cur;
    advance(p);
    ast_node* n = mk(ast_template, p);
    ast_parse_template_parts(p, t, n);
    return n;
  }

  if (check(p, tok_plus_plus) || check(p, tok_minus_minus)) {
    ast_node* n = mk(ast_update, p);
    n->op = p->cur.kind;
    n->flag_a = 1;
    advance(p);
    n->a = ast_parse_postfix(p);
    return n;
  }

  if (match(p, tok_kw_this))
    return mk(ast_this, p);

  if (match(p, tok_kw_await)) {
    ast_node* n = mk(ast_await, p);
    n->a = ast_parse_unary_pub(p);
    return n;
  }

  if (match(p, tok_kw_yield)) {
    ast_node* n = mk(ast_yield, p);
    n->flag_a = match(p, tok_star);
    int has_operand =
        !(check(p, tok_semi) || check(p, tok_rparen) || check(p, tok_rbrace) ||
          check(p, tok_rbracket) || check(p, tok_comma) || check(p, tok_eof));
    if (has_operand)
      n->a = ast_parse_assign(p);
    return n;
  }

  if (check(p, tok_kw_async)) {
    lexer sl = p->lex;
    tok sc = p->cur, sp = p->prev;
    advance(p);
    if (match(p, tok_kw_function)) {
      int is_gen = match(p, tok_star);
      tok name;
      name.start = "";
      name.len = 0;
      if (is_contextual_ident(p->cur.kind)) {
        name = p->cur;
        advance(p);
      }
      ast_node* fn = ast_parse_function_like(p, 0, 1, 1, is_gen, 0);
      fn->str = name.start;
      fn->str_len = name.len;
      return fn;
    }
    if (check(p, tok_ident) && peek_is_arrow(p)) {
      return ast_parse_function_like(p, 1, 0, 1, 0, 0);
    }
    if (check(p, tok_lparen) && peek_arrow_after_parens(p)) {
      return ast_parse_function_like(p, 1, 1, 1, 0, 0);
    }
    p->lex = sl;
    p->cur = sc;
    p->prev = sp;
    advance(p);
    return ast_parse_ident_or_arrow_from_prev(p);
  }

  if (match(p, tok_kw_function)) {
    int is_gen = match(p, tok_star);
    tok name;
    name.start = "";
    name.len = 0;
    if (is_contextual_ident(p->cur.kind)) {
      name = p->cur;
      advance(p);
    }
    ast_node* fn = ast_parse_function_like(p, 0, 1, 0, is_gen, 0);
    fn->str = name.start;
    fn->str_len = name.len;
    return fn;
  }

  if (match(p, tok_kw_class)) {
    return ast_parse_class_body(p, 1);
  }

  if (check(p, tok_ident) && peek_is_arrow(p)) {
    return ast_parse_ident_or_arrow(p);
  }

  if (is_contextual_ident(p->cur.kind)) {
    advance(p);
    tok name = p->prev;
    ast_node* n = mk(ast_ident, p);
    n->str = name.start;
    n->str_len = name.len;
    return n;
  }

  if (check(p, tok_lparen) && peek_arrow_after_parens(p)) {
    return ast_parse_function_like(p, 1, 1, 0, 0, 0);
  }

  if (match(p, tok_lparen)) {
    if (check(p, tok_lbrace)) {
      lexer sl = p->lex;
      tok sc = p->cur, sp = p->prev;
      advance(p);
      const char* pattern_start = p->prev.start;
      skip_balanced(p, tok_lbrace, tok_rbrace);
      if (check(p, tok_assign)) {
        advance(p);
        ast_node* value = ast_parse_assign(p);
        if (!match(p, tok_rparen)) {
          error_at(p, "expected ')'");
          return mk(ast_undef, p);
        }
        parser pp;
        parser_init(&pp, pattern_start);
        ast_node* pat = ast_parse_object_pattern(&pp);
        ast_node* n = mk(ast_paren_pattern_assign, p);
        n->a = pat;
        n->b = value;
        return n;
      }
      p->lex = sl;
      p->cur = sc;
      p->prev = sp;
    }
    ast_node* e = ast_parse_seq_expr(p);
    if (!match(p, tok_rparen))
      error_at(p, "expected ')'");
    return e;
  }

  if (match(p, tok_lbrace))
    return ast_parse_object_literal(p);

  if (match(p, tok_lbracket))
    return ast_parse_array_literal(p);

  if (check(p, tok_slash) || check(p, tok_slash_eq))
    return ast_parse_regex(p);

  error_at(p, "expected expression");
  advance(p);
  return mk(ast_undef, p);
}

ast_node* ast_parse_ident_or_arrow_from_prev(parser* p) {
  tok name = p->prev;
  ast_node* n = mk(ast_ident, p);
  n->str = name.start;
  n->str_len = name.len;
  return n;
}

static ast_node* ast_parse_postfix(parser* p) {
  ast_node* node = ast_parse_primary(p);

  for (;;) {
    if (check(p, tok_question_dot)) {
      advance(p);
      if (check(p, tok_lparen)) {
        advance(p);
        ast_node* c = mk(ast_call, p);
        c->a = node;
        c->flag_a = 1;
        ast_parse_call_args(p, &c->list);
        node = c;
        continue;
      }
      int is_bracket = check(p, tok_lbracket);
      ast_node* m = mk(ast_member, p);
      m->a = node;
      m->flag_b = 1;
      if (is_bracket) {
        advance(p);
        m->flag_a = 1;
        m->b = ast_parse_expr(p);
        if (!expect(p, tok_rbracket))
          return m;
      } else if (!match_property_name(p)) {
        error_at(p, "expected property name");
        return m;
      } else {
        m->str = dup_tok(p->prev);
        m->str_len = strlen(m->str);
      }
      if (check(p, tok_lparen)) {
        advance(p);
        ast_node* c = mk(ast_call, p);
        c->a = m;
        ast_parse_call_args(p, &c->list);
        node = c;
        continue;
      }
      node = m;
      continue;
    }

    if (check(p, tok_dot) || check(p, tok_lbracket)) {
      ast_node* m = mk(ast_member, p);
      m->a = node;
      if (match(p, tok_dot)) {
        if (!match_property_name(p)) {
          error_at(p, "expected property name");
          return m;
        }
        m->str = dup_tok(p->prev);
        m->str_len = strlen(m->str);
      } else {
        advance(p);
        m->flag_a = 1;
        m->b = ast_parse_expr(p);
        if (!expect(p, tok_rbracket))
          return m;
      }

      if (check(p, tok_lparen)) {
        advance(p);
        ast_node* c = mk(ast_call, p);
        c->a = m;
        ast_parse_call_args(p, &c->list);
        node = c;
        continue;
      }
      if (check(p, tok_template)) {
        tok t = p->cur;
        advance(p);
        ast_node* tt = mk(ast_tagged_template, p);
        tt->a = m;
        ast_parse_template_parts(p, t, tt);
        node = tt;
        continue;
      }
      node = m;
      continue;
    }

    if (check(p, tok_lparen)) {
      const char* call_src = p->cur.start;
      advance(p);
      ast_node* c = mk(ast_call, p);
      c->a = node;
      c->raw_src = call_src;
      ast_parse_call_args(p, &c->list);
      node = c;
      continue;
    }

    if (check(p, tok_template)) {
      tok t = p->cur;
      advance(p);
      ast_node* tt = mk(ast_tagged_template, p);
      tt->a = node;
      ast_parse_template_parts(p, t, tt);
      node = tt;
      continue;
    }

    break;
  }

  if (check(p, tok_plus_plus) || check(p, tok_minus_minus)) {
    ast_node* n = mk(ast_update, p);
    n->op = p->cur.kind;
    n->flag_a = 0;
    n->a = node;
    advance(p);
    return n;
  }

  return node;
}

ast_node* ast_parse_unary_pub(parser* p) {
  if (match(p, tok_kw_delete)) {
    ast_node* n = mk(ast_unary, p);
    n->op = tok_kw_delete;
    n->a = ast_parse_postfix(p);
    return n;
  }
  if (check(p, tok_plus) || check(p, tok_minus) || check(p, tok_bang) ||
      check(p, tok_tilde) || check(p, tok_kw_void)) {
    tok_kind op = p->cur.kind;
    advance(p);
    ast_node* n = mk(ast_unary, p);
    n->op = op;
    n->a = ast_parse_unary_pub(p);
    return n;
  }
  if (match(p, tok_kw_typeof)) {
    ast_node* n = mk(ast_unary, p);
    n->op = tok_kw_typeof;
    n->a = ast_parse_unary_pub(p);
    return n;
  }
  return ast_parse_postfix(p);
}

static ast_node* ast_parse_exp(parser* p) {
  ast_node* left = ast_parse_unary_pub(p);
  if (check(p, tok_star_star)) {
    advance(p);
    ast_node* n = mk(ast_binary, p);
    n->op = tok_star_star;
    n->a = left;
    n->b = ast_parse_exp(p);
    return n;
  }
  return left;
}

static ast_node* ast_parse_mul(parser* p) {
  ast_node* left = ast_parse_exp(p);
  while (check(p, tok_star) || check(p, tok_slash) || check(p, tok_percent)) {
    tok_kind k = p->cur.kind;
    advance(p);
    ast_node* n = mk(ast_binary, p);
    n->op = k;
    n->a = left;
    n->b = ast_parse_exp(p);
    left = n;
  }
  return left;
}

static ast_node* ast_parse_add(parser* p) {
  ast_node* left = ast_parse_mul(p);
  while (check(p, tok_plus) || check(p, tok_minus)) {
    tok_kind k = p->cur.kind;
    advance(p);
    ast_node* n = mk(ast_binary, p);
    n->op = k;
    n->a = left;
    n->b = ast_parse_mul(p);
    left = n;
  }
  return left;
}

static ast_node* ast_parse_shift(parser* p) {
  ast_node* left = ast_parse_add(p);
  while (check(p, tok_shl) || check(p, tok_shr) || check(p, tok_ushr)) {
    tok_kind k = p->cur.kind;
    advance(p);
    ast_node* n = mk(ast_binary, p);
    n->op = k;
    n->a = left;
    n->b = ast_parse_add(p);
    left = n;
  }
  return left;
}

static ast_node* ast_parse_cmp(parser* p) {
  ast_node* left = ast_parse_shift(p);
  for (;;) {
    if (check(p, tok_lt) || check(p, tok_gt) || check(p, tok_le) ||
        check(p, tok_ge) || check(p, tok_kw_instanceof) ||
        check(p, tok_kw_in)) {
      tok_kind k = p->cur.kind;
      advance(p);
      ast_node* n = mk(ast_binary, p);
      n->op = k;
      n->a = left;
      n->b = ast_parse_shift(p);
      left = n;
    } else {
      break;
    }
  }
  return left;
}

static ast_node* ast_parse_eq(parser* p) {
  ast_node* left = ast_parse_cmp(p);
  while (check(p, tok_eq) || check(p, tok_neq) || check(p, tok_eq_strict) ||
         check(p, tok_neq_strict)) {
    tok_kind k = p->cur.kind;
    advance(p);
    ast_node* n = mk(ast_binary, p);
    n->op = k;
    n->a = left;
    n->b = ast_parse_cmp(p);
    left = n;
  }
  return left;
}

static ast_node* ast_parse_bitand(parser* p) {
  ast_node* left = ast_parse_eq(p);
  while (check(p, tok_amp)) {
    advance(p);
    ast_node* n = mk(ast_binary, p);
    n->op = tok_amp;
    n->a = left;
    n->b = ast_parse_eq(p);
    left = n;
  }
  return left;
}

static ast_node* ast_parse_bitxor(parser* p) {
  ast_node* left = ast_parse_bitand(p);
  while (check(p, tok_caret)) {
    advance(p);
    ast_node* n = mk(ast_binary, p);
    n->op = tok_caret;
    n->a = left;
    n->b = ast_parse_bitand(p);
    left = n;
  }
  return left;
}

static ast_node* ast_parse_bitor(parser* p) {
  ast_node* left = ast_parse_bitxor(p);
  while (check(p, tok_pipe)) {
    advance(p);
    ast_node* n = mk(ast_binary, p);
    n->op = tok_pipe;
    n->a = left;
    n->b = ast_parse_bitxor(p);
    left = n;
  }
  return left;
}

static ast_node* ast_parse_and(parser* p) {
  ast_node* left = ast_parse_bitor(p);
  while (check(p, tok_amp_amp)) {
    advance(p);
    ast_node* n = mk(ast_logical, p);
    n->op = tok_amp_amp;
    n->a = left;
    n->b = ast_parse_bitor(p);
    left = n;
  }
  return left;
}

static ast_node* ast_parse_or(parser* p) {
  ast_node* left = ast_parse_and(p);
  while (check(p, tok_pipe_pipe)) {
    advance(p);
    ast_node* n = mk(ast_logical, p);
    n->op = tok_pipe_pipe;
    n->a = left;
    n->b = ast_parse_and(p);
    left = n;
  }
  return left;
}

static ast_node* ast_parse_nullish(parser* p) {
  ast_node* left = ast_parse_or(p);
  while (check(p, tok_question_question)) {
    advance(p);
    ast_node* n = mk(ast_logical, p);
    n->op = tok_question_question;
    n->a = left;
    n->b = ast_parse_or(p);
    left = n;
  }
  return left;
}

static ast_node* ast_parse_cond(parser* p) {
  ast_node* test = ast_parse_nullish(p);
  if (match(p, tok_question)) {
    ast_node* n = mk(ast_cond, p);
    n->a = test;
    n->b = ast_parse_assign(p);
    expect(p, tok_colon);
    n->c = ast_parse_assign(p);
    return n;
  }
  return test;
}

static ast_node* ast_parse_assign(parser* p) {
  ast_node* left = ast_parse_cond(p);
  if (is_assign_op(p->cur.kind)) {
    tok_kind op = p->cur.kind;
    if (left->kind != ast_ident &&
        !(left->kind == ast_member && !left->flag_b)) {
      error_at(p, "invalid assignment target");
      return left;
    }
    advance(p);
    ast_node* n = mk(ast_assign, p);
    n->op = op;
    n->a = left;
    n->b = ast_parse_assign(p);
    return n;
  }
  return left;
}

static ast_node* ast_parse_expr(parser* p) {
  return ast_parse_assign(p);
}

static ast_node* ast_parse_seq_expr(parser* p) {
  ast_node* first = ast_parse_expr(p);
  if (!check(p, tok_comma))
    return first;
  ast_node* n = mk(ast_seq, p);
  ast_list_push(g_arena, &n->list, first);
  while (match(p, tok_comma))
    ast_list_push(g_arena, &n->list, ast_parse_expr(p));
  return n;
}

static ast_node* ast_parse_array_pattern(parser* p) {
  ast_node* n = mk(ast_pat_array, p);
  advance(p);
  for (;;) {
    if (check(p, tok_rbracket))
      break;
    if (match(p, tok_comma)) {
      ast_list_push(g_arena, &n->list, mk(ast_pat_hole, p));
      continue;
    }
    if (match(p, tok_ellipsis)) {
      if (!expect(p, tok_ident))
        return n;
      ast_node* rest = mk(ast_pat_rest, p);
      ast_node* target = ast_new_node(g_arena, ast_pat_ident, p->prev.line);
      target->str = p->prev.start;
      target->str_len = p->prev.len;
      rest->a = target;
      ast_list_push(g_arena, &n->list, rest);
      break;
    }
    ast_node* target = ast_parse_binding_pattern(p);
    if (match(p, tok_assign)) {
      ast_node* wrap = mk(ast_pat_assign, p);
      wrap->a = target;
      wrap->b = ast_parse_assign(p);
      target = wrap;
    }
    ast_list_push(g_arena, &n->list, target);
    if (!match(p, tok_comma))
      break;
  }
  expect(p, tok_rbracket);
  return n;
}

static ast_node* ast_parse_object_pattern(parser* p) {
  ast_node* n = mk(ast_pat_object, p);
  advance(p);
  for (;;) {
    if (check(p, tok_rbrace))
      break;
    if (!expect(p, tok_ident))
      return n;
    tok key_name = p->prev;
    ast_prop prop;
    memset(&prop, 0, sizeof(prop));
    prop.key = ident_key_node(key_name, p);

    if (match(p, tok_colon)) {
      prop.value = ast_parse_binding_pattern(p);
    } else {
      ast_node* target = ast_new_node(g_arena, ast_pat_ident, key_name.line);
      target->str = key_name.start;
      target->str_len = key_name.len;
      prop.value = target;
    }
    if (match(p, tok_assign)) {
      ast_node* wrap = mk(ast_pat_assign, p);
      wrap->a = prop.value;
      wrap->b = ast_parse_assign(p);
      prop.value = wrap;
    }
    ast_prop_list_push(g_arena, &n->props, prop);
    if (!match(p, tok_comma))
      break;
  }
  expect(p, tok_rbrace);
  return n;
}

static ast_node* ast_parse_binding_pattern(parser* p) {
  if (check(p, tok_lbracket))
    return ast_parse_array_pattern(p);
  if (check(p, tok_lbrace))
    return ast_parse_object_pattern(p);
  if (!is_contextual_ident(p->cur.kind)) {
    error_at(p, "expected identifier");
    return mk(ast_pat_hole, p);
  }
  advance(p);
  ast_node* n = ast_new_node(g_arena, ast_pat_ident, p->prev.line);
  n->str = p->prev.start;
  n->str_len = p->prev.len;
  return n;
}

static void ast_parse_params(parser* p, ast_param_list* out) {
  expect(p, tok_lparen);
  if (!check(p, tok_rparen)) {
    for (;;) {
      if (match(p, tok_ellipsis)) {
        if (!is_contextual_ident(p->cur.kind)) {
          error_at(p, "expected identifier");
          break;
        }
        advance(p);
        ast_param param;
        param.is_rest = 1;
        param.pattern = ast_new_node(g_arena, ast_pat_ident, p->prev.line);
        param.pattern->str = p->prev.start;
        param.pattern->str_len = p->prev.len;
        ast_param_list_push(g_arena, out, param);
        break;
      }
      ast_node* target = ast_parse_binding_pattern(p);
      if (match(p, tok_assign)) {
        ast_node* wrap = mk(ast_pat_assign, p);
        wrap->a = target;
        wrap->b = ast_parse_assign(p);
        target = wrap;
      }
      ast_param param;
      param.is_rest = 0;
      param.pattern = target;
      ast_param_list_push(g_arena, out, param);
      if (!match(p, tok_comma))
        break;
    }
  }
  expect(p, tok_rparen);
}

static ast_node* ast_parse_function_like(parser* p, int is_arrow,
                                         int parens_params, int is_async,
                                         int is_gen, int is_decl) {
  ast_node* n = mk(is_decl ? ast_func_decl : ast_func_expr, p);
  n->flag_a = is_arrow;
  n->flag_b = is_async;
  n->flag_c = is_gen;

  if (parens_params) {
    ast_parse_params(p, &n->params);
  } else if (expect(p, tok_ident)) {
    ast_param param;
    param.is_rest = 0;
    param.pattern = ast_new_node(g_arena, ast_pat_ident, p->prev.line);
    param.pattern->str = p->prev.start;
    param.pattern->str_len = p->prev.len;
    ast_param_list_push(g_arena, &n->params, param);
  }

  if (is_arrow && !expect(p, tok_arrow))
    return n;

  if (check(p, tok_lbrace)) {
    n->flag_d = 1;
    n->a = ast_parse_block(p);
  } else {
    n->flag_d = 0;
    n->a = ast_parse_assign(p);
  }
  return n;
}

static ast_node* ast_parse_class_body(parser* p, int is_expr) {
  ast_node* n = mk(is_expr ? ast_class_expr : ast_class_decl, p);
  if (is_expr && !check(p, tok_ident)) {
    n->str = "";
    n->str_len = 0;
  } else {
    if (!expect(p, tok_ident))
      return n;
    n->str = p->prev.start;
    n->str_len = p->prev.len;
  }

  if (match(p, tok_kw_extends)) {
    n->flag_a = 1;
    n->a = ast_parse_unary_pub(p);
  }

  if (!expect(p, tok_lbrace))
    return n;

  while (!check(p, tok_rbrace) && !check(p, tok_eof) && !p->had_error) {
    if (match(p, tok_semi))
      continue;

    ast_class_member m;
    memset(&m, 0, sizeof(m));

    if (check(p, tok_kw_static)) {
      lexer sl = p->lex;
      tok sc = p->cur, sp = p->prev;
      advance(p);
      if (check(p, tok_lparen) || check(p, tok_assign) || check(p, tok_semi) ||
          check(p, tok_rbrace)) {
        p->lex = sl;
        p->cur = sc;
        p->prev = sp;
      } else {
        m.is_static = 1;
      }
    }

    int is_async = 0;
    if (check(p, tok_kw_async)) {
      lexer sl = p->lex;
      tok sc = p->cur, sp = p->prev;
      advance(p);
      if (check(p, tok_lparen)) {
        p->lex = sl;
        p->cur = sc;
        p->prev = sp;
      } else {
        is_async = 1;
      }
    }

    int is_gen = match(p, tok_star);

    int is_getter = 0, is_setter = 0;
    if (!is_async && (check(p, tok_kw_get) || check(p, tok_kw_set))) {
      tok_kind modifier = p->cur.kind;
      lexer sl = p->lex;
      tok sc = p->cur, sp = p->prev;
      advance(p);
      if (check(p, tok_lparen)) {
        p->lex = sl;
        p->cur = sc;
        p->prev = sp;
      } else {
        is_getter = modifier == tok_kw_get;
        is_setter = modifier == tok_kw_set;
      }
    }

    if (match(p, tok_lbracket)) {
      m.computed = 1;
      m.key = ast_parse_expr(p);
      if (!expect(p, tok_rbracket))
        break;
    } else if (!match_property_name(p)) {
      break;
    } else {
      m.key = ident_key_node(p->prev, p);
    }

    int is_ctor = !m.computed && !m.is_static && !is_getter && !is_setter &&
                  m.key->str_len == 11 &&
                  memcmp(m.key->str, "constructor", 11) == 0;

    if (is_getter || is_setter) {
      m.is_getter = is_getter;
      m.is_setter = is_setter;
      m.value = ast_parse_function_like(p, 0, 1, 0, 0, 0);
    } else if (is_ctor) {
      m.is_ctor = 1;
      m.value = ast_parse_function_like(p, 0, 1, 0, 0, 0);
    } else if (check(p, tok_lparen)) {
      m.is_async = is_async;
      m.is_generator = is_gen;
      m.value = ast_parse_function_like(p, 0, 1, is_async, is_gen, 0);
    } else if (m.computed) {
      error_at(p, "computed field names are not supported");
      break;
    } else {
      m.is_field = 1;
      if (match(p, tok_assign)) {
        m.value = ast_parse_assign(p);
      }
      match(p, tok_semi);
    }
    ast_class_member_list_push(g_arena, &n->members, m);
  }
  expect(p, tok_rbrace);
  return n;
}

static ast_node* ast_parse_var_decl(parser* p, tok_kind kind) {
  ast_node* n = mk(ast_var_decl, p);
  n->flag_a = kind;
  for (;;) {
    ast_node* target = ast_parse_binding_pattern(p);
    if (match(p, tok_assign)) {
      ast_node* wrap = mk(ast_pat_assign, p);
      wrap->a = target;
      wrap->b = ast_parse_assign(p);
      target = wrap;
    }
    ast_list_push(g_arena, &n->list, target);
    if (!match(p, tok_comma))
      break;
  }
  return n;
}

static ast_node* ast_parse_block(parser* p) {
  ast_node* n = mk(ast_block, p);
  expect(p, tok_lbrace);
  while (!check(p, tok_rbrace) && !check(p, tok_eof) && !p->had_error)
    ast_list_push(g_arena, &n->list, ast_parse_stmt(p));
  expect(p, tok_rbrace);
  return n;
}

static ast_node* ast_parse_for(parser* p) {
  int is_await = match(p, tok_kw_await);
  expect(p, tok_lparen);

  if (check(p, tok_kw_var) || check(p, tok_kw_let) || check(p, tok_kw_const)) {
    lexer sl = p->lex;
    tok sc = p->cur, sp = p->prev;
    advance(p);
    tok_kind kind = p->prev.kind;
    if (check(p, tok_lbracket) || check(p, tok_lbrace)) {
      ast_node* pat = ast_parse_binding_pattern(p);
      if (match(p, tok_kw_of) || match(p, tok_kw_in)) {
        int is_of = p->prev.kind == tok_kw_of;
        ast_node* n = mk(is_of ? ast_for_of : ast_for_in, p);
        n->flag_a = kind;
        n->flag_b = is_await;
        n->a = pat;
        n->b = ast_parse_seq_expr(p);
        expect(p, tok_rparen);
        n->c = ast_parse_stmt(p);
        return n;
      }
      p->lex = sl;
      p->cur = sc;
      p->prev = sp;
    } else {
      p->lex = sl;
      p->cur = sc;
      p->prev = sp;
    }
  }

  if (match(p, tok_kw_var) || match(p, tok_kw_let) || match(p, tok_kw_const)) {
    tok_kind kind = p->prev.kind;
    if (!expect(p, tok_ident)) {
      ast_node* n = mk(ast_empty, p);
      return n;
    }
    tok name = p->prev;
    if (match(p, tok_kw_in) || match(p, tok_kw_of)) {
      int is_of = p->prev.kind == tok_kw_of;
      ast_node* n = mk(is_of ? ast_for_of : ast_for_in, p);
      n->flag_a = kind;
      n->flag_b = is_of && is_await;
      ast_node* pat = ast_new_node(g_arena, ast_pat_ident, name.line);
      pat->str = name.start;
      pat->str_len = name.len;
      n->a = pat;
      n->b = ast_parse_expr(p);
      expect(p, tok_rparen);
      n->c = ast_parse_stmt(p);
      return n;
    }
    ast_node* decl = mk(ast_var_decl, p);
    decl->flag_a = kind;
    ast_node* target = ast_new_node(g_arena, ast_pat_ident, name.line);
    target->str = name.start;
    target->str_len = name.len;
    if (match(p, tok_assign)) {
      ast_node* wrap = mk(ast_pat_assign, p);
      wrap->a = target;
      wrap->b = ast_parse_assign(p);
      target = wrap;
    }
    ast_list_push(g_arena, &decl->list, target);
    while (match(p, tok_comma)) {
      ast_node* t2 = ast_parse_binding_pattern(p);
      if (match(p, tok_assign)) {
        ast_node* wrap = mk(ast_pat_assign, p);
        wrap->a = t2;
        wrap->b = ast_parse_assign(p);
        t2 = wrap;
      }
      ast_list_push(g_arena, &decl->list, t2);
    }
    expect(p, tok_semi);
    ast_node* n = mk(ast_for, p);
    n->a = decl;
    if (!check(p, tok_semi))
      n->b = ast_parse_seq_expr(p);
    expect(p, tok_semi);
    if (!check(p, tok_rparen))
      n->c = ast_parse_seq_expr(p);
    expect(p, tok_rparen);
    n->d = ast_parse_stmt(p);
    return n;
  }

  if (is_contextual_ident(p->cur.kind)) {
    lexer sl = p->lex;
    tok sc = p->cur, sp = p->prev;
    tok name = p->cur;
    advance(p);
    if (match(p, tok_kw_in) || match(p, tok_kw_of)) {
      int is_of = p->prev.kind == tok_kw_of;
      ast_node* n = mk(is_of ? ast_for_of : ast_for_in, p);
      n->flag_a = tok_eof;
      n->flag_b = is_of && is_await;
      ast_node* pat = ast_new_node(g_arena, ast_pat_ident, name.line);
      pat->str = name.start;
      pat->str_len = name.len;
      n->a = pat;
      n->b = ast_parse_expr(p);
      expect(p, tok_rparen);
      n->c = ast_parse_stmt(p);
      return n;
    }
    p->lex = sl;
    p->cur = sc;
    p->prev = sp;
  }

  ast_node* n = mk(ast_for, p);
  if (!check(p, tok_semi))
    n->a = ast_parse_seq_expr(p);
  expect(p, tok_semi);
  if (!check(p, tok_semi))
    n->b = ast_parse_seq_expr(p);
  expect(p, tok_semi);
  if (!check(p, tok_rparen))
    n->c = ast_parse_seq_expr(p);
  expect(p, tok_rparen);
  n->d = ast_parse_stmt(p);
  return n;
}

static ast_node* ast_parse_try(parser* p) {
  ast_node* n = mk(ast_try, p);
  n->a = ast_parse_block(p);
  if (match(p, tok_kw_catch)) {
    n->flag_a = 1;
    if (match(p, tok_lparen)) {
      if (check(p, tok_ident)) {
        advance(p);
        n->b = ast_new_node(g_arena, ast_pat_ident, p->prev.line);
        n->b->str = p->prev.start;
        n->b->str_len = p->prev.len;
      }
      expect(p, tok_rparen);
    }
    n->c = ast_parse_block(p);
  }
  if (match(p, tok_kw_finally)) {
    n->flag_b = 1;
    n->d = ast_parse_block(p);
  } else if (!n->flag_a) {
    error_at(p, "expected 'catch' or 'finally'");
  }
  return n;
}

static ast_node* ast_parse_switch(parser* p) {
  ast_node* n = mk(ast_switch, p);
  expect(p, tok_lparen);
  n->a = ast_parse_seq_expr(p);
  expect(p, tok_rparen);
  expect(p, tok_lbrace);
  while (!check(p, tok_rbrace) && !check(p, tok_eof) && !p->had_error) {
    ast_switch_case sc;
    memset(&sc, 0, sizeof(sc));
    if (match(p, tok_kw_case)) {
      sc.test = ast_parse_expr(p);
      expect(p, tok_colon);
    } else if (match(p, tok_kw_default)) {
      sc.test = NULL;
      expect(p, tok_colon);
    } else {
      error_at(p, "expected 'case' or 'default'");
      break;
    }
    while (!check(p, tok_kw_case) && !check(p, tok_kw_default) &&
           !check(p, tok_rbrace) && !check(p, tok_eof))
      ast_list_push(g_arena, &sc.body, ast_parse_stmt(p));
    ast_switch_case_list_push(g_arena, &n->cases, sc);
  }
  expect(p, tok_rbrace);
  return n;
}

static ast_node* ast_parse_import(parser* p) {
  ast_node* n = mk(ast_import, p);
  n->raw_src = p->cur.start;

  if (check(p, tok_str)) {
    tok spec_tok = p->cur;
    advance(p);
    n->str = decode_string(spec_tok);
    n->str_len = strlen(n->str);
    n->flag_a = 1;
    expect_semi(p);
    return n;
  }

  ast_import_binding* ib = malloc(sizeof(ast_import_binding));
  memset(ib, 0, sizeof(*ib));
  n->import_binding = ib;

  if (check(p, tok_ident)) {
    ib->has_default = 1;
    ib->default_name = p->cur.start;
    ib->default_len = p->cur.len;
    advance(p);
    if (match(p, tok_comma)) {
      if (match(p, tok_star)) {
        if (!expect(p, tok_kw_as))
          return n;
        if (!expect(p, tok_ident))
          return n;
        ib->has_namespace = 1;
        ib->namespace_name = p->prev.start;
        ib->namespace_len = p->prev.len;
      } else if (match(p, tok_lbrace)) {
        if (!check(p, tok_rbrace)) {
          for (;;) {
            if (!expect(p, tok_ident))
              return n;
            tok key = p->prev;
            tok local = key;
            if (match(p, tok_kw_as)) {
              if (!expect(p, tok_ident))
                return n;
              local = p->prev;
            }
            if (ib->named_count < 64) {
              ib->named[ib->named_count].key = key.start;
              ib->named[ib->named_count].key_len = key.len;
              ib->named[ib->named_count].local = local.start;
              ib->named[ib->named_count].local_len = local.len;
              ib->named_count++;
            }
            if (!match(p, tok_comma))
              break;
            if (check(p, tok_rbrace))
              break;
          }
        }
        if (!expect(p, tok_rbrace))
          return n;
      }
    }
  } else if (match(p, tok_star)) {
    if (!expect(p, tok_kw_as))
      return n;
    if (!expect(p, tok_ident))
      return n;
    ib->has_namespace = 1;
    ib->namespace_name = p->prev.start;
    ib->namespace_len = p->prev.len;
  } else if (match(p, tok_lbrace)) {
    if (!check(p, tok_rbrace)) {
      for (;;) {
        if (!expect(p, tok_ident))
          return n;
        tok key = p->prev;
        tok local = key;
        if (match(p, tok_kw_as)) {
          if (!expect(p, tok_ident))
            return n;
          local = p->prev;
        }
        if (ib->named_count < 64) {
          ib->named[ib->named_count].key = key.start;
          ib->named[ib->named_count].key_len = key.len;
          ib->named[ib->named_count].local = local.start;
          ib->named[ib->named_count].local_len = local.len;
          ib->named_count++;
        }
        if (!match(p, tok_comma))
          break;
        if (check(p, tok_rbrace))
          break;
      }
    }
    if (!expect(p, tok_rbrace))
      return n;
  }

  if (!expect(p, tok_kw_from))
    return n;
  if (!check(p, tok_str)) {
    error_at(p, "expected module specifier string");
    return n;
  }
  tok spec_tok = p->cur;
  advance(p);
  n->str = decode_string(spec_tok);
  n->str_len = strlen(n->str);
  expect_semi(p);
  return n;
}

static ast_node* ast_parse_labeled(parser* p, tok label) {
  ast_node* n = mk(ast_labeled, p);
  n->str = label.start;
  n->str_len = label.len;
  n->a = ast_parse_stmt(p);
  return n;
}

static ast_node* ast_parse_stmt(parser* p) {
  if (match(p, tok_semi))
    return mk(ast_empty, p);

  if (check(p, tok_lbrace))
    return ast_parse_block(p);

  if (match(p, tok_kw_function)) {
    int is_gen = match(p, tok_star);
    if (!is_contextual_ident(p->cur.kind)) {
      error_at(p, "expected identifier");
      return mk(ast_empty, p);
    }
    advance(p);
    tok name = p->prev;
    ast_node* fn = ast_parse_function_like(p, 0, 1, 0, is_gen, 1);
    fn->str = name.start;
    fn->str_len = name.len;
    return fn;
  }

  if (check(p, tok_kw_async)) {
    lexer sl = p->lex;
    tok sc = p->cur, sp = p->prev;
    advance(p);
    if (match(p, tok_kw_function)) {
      int is_gen = match(p, tok_star);
      if (!is_contextual_ident(p->cur.kind)) {
        error_at(p, "expected identifier");
        return mk(ast_empty, p);
      }
      advance(p);
      tok name = p->prev;
      ast_node* fn = ast_parse_function_like(p, 0, 1, 1, is_gen, 1);
      fn->str = name.start;
      fn->str_len = name.len;
      return fn;
    }
    p->lex = sl;
    p->cur = sc;
    p->prev = sp;
  }

  if (match(p, tok_kw_class))
    return ast_parse_class_body(p, 0);

  if (match(p, tok_kw_import))
    return ast_parse_import(p);

  if (match(p, tok_kw_if)) {
    ast_node* n = mk(ast_if, p);
    expect(p, tok_lparen);
    n->a = ast_parse_seq_expr(p);
    expect(p, tok_rparen);
    n->b = ast_parse_stmt(p);
    if (match(p, tok_kw_else))
      n->c = ast_parse_stmt(p);
    return n;
  }

  if (match(p, tok_kw_while)) {
    ast_node* n = mk(ast_while, p);
    expect(p, tok_lparen);
    n->a = ast_parse_seq_expr(p);
    expect(p, tok_rparen);
    n->b = ast_parse_stmt(p);
    return n;
  }

  if (match(p, tok_kw_do)) {
    ast_node* n = mk(ast_do_while, p);
    n->a = ast_parse_stmt(p);
    if (!expect(p, tok_kw_while))
      return n;
    expect(p, tok_lparen);
    n->b = ast_parse_seq_expr(p);
    expect(p, tok_rparen);
    expect_semi(p);
    return n;
  }

  if (match(p, tok_kw_for))
    return ast_parse_for(p);

  if (match(p, tok_kw_switch))
    return ast_parse_switch(p);

  if (match(p, tok_kw_break)) {
    ast_node* n = mk(ast_break, p);
    if (check(p, tok_ident) && p->cur.line == p->prev.line) {
      n->str = p->cur.start;
      n->str_len = p->cur.len;
      advance(p);
    }
    expect_semi(p);
    return n;
  }

  if (match(p, tok_kw_continue)) {
    ast_node* n = mk(ast_continue, p);
    if (check(p, tok_ident) && p->cur.line == p->prev.line) {
      n->str = p->cur.start;
      n->str_len = p->cur.len;
      advance(p);
    }
    expect_semi(p);
    return n;
  }

  if (match(p, tok_kw_throw)) {
    ast_node* n = mk(ast_throw, p);
    n->a = ast_parse_expr(p);
    expect_semi(p);
    return n;
  }

  if (match(p, tok_kw_try))
    return ast_parse_try(p);

  if (match(p, tok_kw_var) || match(p, tok_kw_let) || match(p, tok_kw_const)) {
    ast_node* n = ast_parse_var_decl(p, p->prev.kind);
    expect_semi(p);
    return n;
  }

  if (match(p, tok_kw_return)) {
    ast_node* n = mk(ast_return, p);
    if (!(check(p, tok_semi) || check(p, tok_rbrace) || check(p, tok_eof) ||
          p->cur.line > p->prev.line))
      n->a = ast_parse_seq_expr(p);
    expect_semi(p);
    return n;
  }

  if (check(p, tok_ident)) {
    lexer sl = p->lex;
    tok sc = p->cur, sp = p->prev;
    tok label = p->cur;
    advance(p);
    if (check(p, tok_colon)) {
      advance(p);
      return ast_parse_labeled(p, label);
    }
    p->lex = sl;
    p->cur = sc;
    p->prev = sp;
  }

  if (check(p, tok_lbracket)) {
    lexer sl = p->lex;
    tok sc = p->cur, sp = p->prev;
    advance(p);
    const char* pattern_start = p->prev.start;
    skip_balanced(p, tok_lbracket, tok_rbracket);
    if (check(p, tok_assign)) {
      advance(p);
      ast_node* value = ast_parse_expr(p);
      expect_semi(p);
      parser pp;
      parser_init(&pp, pattern_start);
      ast_node* pat = ast_parse_array_pattern(&pp);
      ast_node* n = mk(ast_paren_pattern_assign, p);
      n->a = pat;
      n->b = value;
      return n;
    }
    p->lex = sl;
    p->cur = sc;
    p->prev = sp;
  }

  ast_node* n = mk(ast_expr_stmt, p);
  n->a = ast_parse_seq_expr(p);
  expect_semi(p);
  return n;
}

ast_node* ast_parse_program_from(ast_arena* arena, parser* p) {
  g_arena = arena;
  ast_node* n = ast_new_node(arena, ast_program, 1);
  while (!check(p, tok_eof) && !p->had_error)
    ast_list_push(arena, &n->list, ast_parse_stmt(p));
  return n;
}

ast_node* ast_parse_expr_from(ast_arena* arena, parser* p) {
  g_arena = arena;
  return ast_parse_seq_expr(p);
}
