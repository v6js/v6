#include "v6/lexer.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  const char* word;
  tok_kind kind;
} keyword;

static const keyword keywords[] = {
    {"var", tok_kw_var},
    {"let", tok_kw_let},
    {"const", tok_kw_const},
    {"function", tok_kw_function},
    {"return", tok_kw_return},
    {"if", tok_kw_if},
    {"else", tok_kw_else},
    {"while", tok_kw_while},
    {"do", tok_kw_do},
    {"for", tok_kw_for},
    {"true", tok_kw_true},
    {"false", tok_kw_false},
    {"null", tok_kw_null},
    {"undefined", tok_kw_undefined},
    {"break", tok_kw_break},
    {"continue", tok_kw_continue},
    {"switch", tok_kw_switch},
    {"case", tok_kw_case},
    {"default", tok_kw_default},
    {"typeof", tok_kw_typeof},
    {"void", tok_kw_void},
    {"delete", tok_kw_delete},
    {"in", tok_kw_in},
    {"of", tok_kw_of},
    {"this", tok_kw_this},
    {"new", tok_kw_new},
    {"class", tok_kw_class},
    {"extends", tok_kw_extends},
    {"super", tok_kw_super},
    {"try", tok_kw_try},
    {"catch", tok_kw_catch},
    {"finally", tok_kw_finally},
    {"throw", tok_kw_throw},
    {"static", tok_kw_static},
    {"instanceof", tok_kw_instanceof},
    {"get", tok_kw_get},
    {"set", tok_kw_set},
    {"async", tok_kw_async},
    {"await", tok_kw_await},
    {"yield", tok_kw_yield},
    {"import", tok_kw_import},
    {"export", tok_kw_export},
    {"from", tok_kw_from},
    {"as", tok_kw_as},
};

void lex_init(lexer* lx, const char* src) {
  lx->src = src;
  lx->cur = src;
  lx->line = 1;
  lx->auto_regex = 0;
  lx->prev_kind = tok_eof;
}

static int lex_regex_allowed_after(tok_kind k) {
  switch (k) {
  case tok_ident:
  case tok_num:
  case tok_str:
  case tok_regex:
  case tok_rparen:
  case tok_rbracket:
  case tok_kw_this:
  case tok_kw_true:
  case tok_kw_false:
  case tok_kw_null:
  case tok_kw_undefined:
  case tok_plus_plus:
  case tok_minus_minus:
  case tok_template:
    return 0;
  default:
    return 1;
  }
}

static int is_ident_start(char c) {
  return isalpha((unsigned char)c) || c == '_' || c == '$' || c == '#';
}

static int is_ident_part(char c) {
  return isalnum((unsigned char)c) || c == '_' || c == '$';
}

static void skip_ws(lexer* lx) {
  for (;;) {
    char c = *lx->cur;
    if (c == '\n') {
      lx->line++;
      lx->cur++;
    } else if (c == ' ' || c == '\t' || c == '\r') {
      lx->cur++;
    } else if (c == '/' && lx->cur[1] == '/') {
      while (*lx->cur && *lx->cur != '\n')
        lx->cur++;
    } else if (c == '/' && lx->cur[1] == '*') {
      lx->cur += 2;
      while (*lx->cur && !(lx->cur[0] == '*' && lx->cur[1] == '/')) {
        if (*lx->cur == '\n')
          lx->line++;
        lx->cur++;
      }
      if (*lx->cur)
        lx->cur += 2;
    } else {
      break;
    }
  }
}

static tok make(lexer* lx, tok_kind kind, const char* start) {
  tok t;
  t.kind = kind;
  t.start = start;
  t.len = (size_t)(lx->cur - start);
  t.line = lx->line;
  t.num = 0;
  t.is_bigint = 0;
  return t;
}

static tok lex_num(lexer* lx, const char* start) {
  if (start[0] == '0' && (*lx->cur == 'x' || *lx->cur == 'X')) {
    lx->cur++;
    while (isxdigit((unsigned char)*lx->cur))
      lx->cur++;
    tok t = make(lx, tok_num, start);
    t.num = (double)strtoll(start + 2, NULL, 16);
    return t;
  }
  if (start[0] == '0' && (*lx->cur == 'o' || *lx->cur == 'O')) {
    lx->cur++;
    while (*lx->cur >= '0' && *lx->cur <= '7')
      lx->cur++;
    tok t = make(lx, tok_num, start);
    t.num = (double)strtoll(start + 2, NULL, 8);
    return t;
  }
  if (start[0] == '0' && (*lx->cur == 'b' || *lx->cur == 'B')) {
    lx->cur++;
    while (*lx->cur == '0' || *lx->cur == '1')
      lx->cur++;
    tok t = make(lx, tok_num, start);
    t.num = (double)strtoll(start + 2, NULL, 2);
    return t;
  }

  while (isdigit((unsigned char)*lx->cur))
    lx->cur++;
  if (*lx->cur == 'n') {
    lx->cur++;
    tok t = make(lx, tok_num, start);
    t.is_bigint = 1;
    return t;
  }
  if (*lx->cur == '.' && isdigit((unsigned char)lx->cur[1])) {
    lx->cur++;
    while (isdigit((unsigned char)*lx->cur))
      lx->cur++;
  }
  if (*lx->cur == 'e' || *lx->cur == 'E') {
    const char* save = lx->cur;
    lx->cur++;
    if (*lx->cur == '+' || *lx->cur == '-')
      lx->cur++;
    if (isdigit((unsigned char)*lx->cur)) {
      while (isdigit((unsigned char)*lx->cur))
        lx->cur++;
    } else {
      lx->cur = save;
    }
  }
  tok t = make(lx, tok_num, start);
  t.num = strtod(start, NULL);
  return t;
}

static tok lex_ident(lexer* lx, const char* start) {
  while (is_ident_part(*lx->cur))
    lx->cur++;
  size_t len = (size_t)(lx->cur - start);
  for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
    size_t klen = strlen(keywords[i].word);
    if (klen == len && memcmp(keywords[i].word, start, len) == 0) {
      return make(lx, keywords[i].kind, start);
    }
  }
  return make(lx, tok_ident, start);
}

static tok lex_str(lexer* lx, const char* start, char quote) {
  lx->cur++;
  while (*lx->cur && *lx->cur != quote) {
    if (*lx->cur == '\\' && lx->cur[1])
      lx->cur++;
    if (*lx->cur == '\n')
      lx->line++;
    lx->cur++;
  }
  if (*lx->cur == quote)
    lx->cur++;
  return make(lx, tok_str, start);
}

static tok lex_template(lexer* lx, const char* start) {
  lx->cur++;
  int depth = 0;
  while (*lx->cur) {
    if (*lx->cur == '\\' && lx->cur[1]) {
      lx->cur += 2;
      continue;
    }
    if (depth == 0 && *lx->cur == '`') {
      lx->cur++;
      break;
    }
    if (depth == 0 && lx->cur[0] == '$' && lx->cur[1] == '{') {
      lx->cur += 2;
      depth = 1;
      continue;
    }
    if (depth > 0 && (*lx->cur == '"' || *lx->cur == '\'')) {
      char q = *lx->cur;
      lx->cur++;
      while (*lx->cur && *lx->cur != q) {
        if (*lx->cur == '\\' && lx->cur[1])
          lx->cur++;
        lx->cur++;
      }
      if (*lx->cur == q)
        lx->cur++;
      continue;
    }
    if (depth > 0 && *lx->cur == '{')
      depth++;
    else if (depth > 0 && *lx->cur == '}')
      depth--;
    if (*lx->cur == '\n')
      lx->line++;
    lx->cur++;
  }
  return make(lx, tok_template, start);
}

static tok lex_next_scan(lexer* lx) {
  skip_ws(lx);
  const char* start = lx->cur;
  char c = *lx->cur;

  if (c == '\0')
    return make(lx, tok_eof, start);
  if (isdigit((unsigned char)c) ||
      (c == '.' && isdigit((unsigned char)lx->cur[1]))) {
    lx->cur++;
    return lex_num(lx, start);
  }
  if (is_ident_start(c)) {
    lx->cur++;
    return lex_ident(lx, start);
  }
  if (c == '"' || c == '\'')
    return lex_str(lx, start, c);
  if (c == '`')
    return lex_template(lx, start);

  lx->cur++;
  switch (c) {
  case '(':
    return make(lx, tok_lparen, start);
  case ')':
    return make(lx, tok_rparen, start);
  case '{':
    return make(lx, tok_lbrace, start);
  case '}':
    return make(lx, tok_rbrace, start);
  case '[':
    return make(lx, tok_lbracket, start);
  case ']':
    return make(lx, tok_rbracket, start);
  case ';':
    return make(lx, tok_semi, start);
  case ',':
    return make(lx, tok_comma, start);
  case '.':
    if (lx->cur[0] == '.' && lx->cur[1] == '.') {
      lx->cur += 2;
      return make(lx, tok_ellipsis, start);
    }
    return make(lx, tok_dot, start);
  case '+':
    if (*lx->cur == '+') {
      lx->cur++;
      return make(lx, tok_plus_plus, start);
    }
    if (*lx->cur == '=') {
      lx->cur++;
      return make(lx, tok_plus_eq, start);
    }
    return make(lx, tok_plus, start);
  case '-':
    if (*lx->cur == '-') {
      lx->cur++;
      return make(lx, tok_minus_minus, start);
    }
    if (*lx->cur == '=') {
      lx->cur++;
      return make(lx, tok_minus_eq, start);
    }
    return make(lx, tok_minus, start);
  case '*':
    if (*lx->cur == '*') {
      lx->cur++;
      if (*lx->cur == '=') {
        lx->cur++;
        return make(lx, tok_star_star_eq, start);
      }
      return make(lx, tok_star_star, start);
    }
    if (*lx->cur == '=') {
      lx->cur++;
      return make(lx, tok_star_eq, start);
    }
    return make(lx, tok_star, start);
  case '/':
    if (*lx->cur == '=') {
      lx->cur++;
      return make(lx, tok_slash_eq, start);
    }
    return make(lx, tok_slash, start);
  case '%':
    if (*lx->cur == '=') {
      lx->cur++;
      return make(lx, tok_percent_eq, start);
    }
    return make(lx, tok_percent, start);
  case '?':
    if (*lx->cur == '.' && !isdigit((unsigned char)lx->cur[1])) {
      lx->cur++;
      return make(lx, tok_question_dot, start);
    }
    if (*lx->cur == '?') {
      lx->cur++;
      if (*lx->cur == '=') {
        lx->cur++;
        return make(lx, tok_question_question_eq, start);
      }
      return make(lx, tok_question_question, start);
    }
    return make(lx, tok_question, start);
  case ':':
    return make(lx, tok_colon, start);
  case '!':
    if (*lx->cur == '=') {
      lx->cur++;
      if (*lx->cur == '=') {
        lx->cur++;
        return make(lx, tok_neq_strict, start);
      }
      return make(lx, tok_neq, start);
    }
    return make(lx, tok_bang, start);
  case '=':
    if (*lx->cur == '=') {
      lx->cur++;
      if (*lx->cur == '=') {
        lx->cur++;
        return make(lx, tok_eq_strict, start);
      }
      return make(lx, tok_eq, start);
    }
    if (*lx->cur == '>') {
      lx->cur++;
      return make(lx, tok_arrow, start);
    }
    return make(lx, tok_assign, start);
  case '<':
    if (*lx->cur == '=') {
      lx->cur++;
      return make(lx, tok_le, start);
    }
    if (*lx->cur == '<') {
      lx->cur++;
      if (*lx->cur == '=') {
        lx->cur++;
        return make(lx, tok_shl_eq, start);
      }
      return make(lx, tok_shl, start);
    }
    return make(lx, tok_lt, start);
  case '>':
    if (*lx->cur == '=') {
      lx->cur++;
      return make(lx, tok_ge, start);
    }
    if (*lx->cur == '>') {
      lx->cur++;
      if (*lx->cur == '>') {
        lx->cur++;
        if (*lx->cur == '=') {
          lx->cur++;
          return make(lx, tok_ushr_eq, start);
        }
        return make(lx, tok_ushr, start);
      }
      if (*lx->cur == '=') {
        lx->cur++;
        return make(lx, tok_shr_eq, start);
      }
      return make(lx, tok_shr, start);
    }
    return make(lx, tok_gt, start);
  case '&':
    if (*lx->cur == '&') {
      lx->cur++;
      if (*lx->cur == '=') {
        lx->cur++;
        return make(lx, tok_amp_amp_eq, start);
      }
      return make(lx, tok_amp_amp, start);
    }
    if (*lx->cur == '=') {
      lx->cur++;
      return make(lx, tok_amp_eq, start);
    }
    return make(lx, tok_amp, start);
  case '|':
    if (*lx->cur == '|') {
      lx->cur++;
      if (*lx->cur == '=') {
        lx->cur++;
        return make(lx, tok_pipe_pipe_eq, start);
      }
      return make(lx, tok_pipe_pipe, start);
    }
    if (*lx->cur == '=') {
      lx->cur++;
      return make(lx, tok_pipe_eq, start);
    }
    return make(lx, tok_pipe, start);
  case '^':
    if (*lx->cur == '=') {
      lx->cur++;
      return make(lx, tok_caret_eq, start);
    }
    return make(lx, tok_caret, start);
  case '~':
    return make(lx, tok_tilde, start);
  default:
    return make(lx, tok_error, start);
  }
}

tok lex_next(lexer* lx) {
  tok t = lex_next_scan(lx);
  if (lx->auto_regex && (t.kind == tok_slash || t.kind == tok_slash_eq) &&
      lex_regex_allowed_after(lx->prev_kind)) {
    lx->cur = t.start;
    lx->line = t.line;
    t = lex_regex_literal(lx);
  }
  lx->prev_kind = t.kind;
  return t;
}

tok lex_regex_literal(lexer* lx) {
  const char* start = lx->cur;
  lx->cur++;
  int in_class = 0;
  while (*lx->cur && *lx->cur != '\n' && (in_class || *lx->cur != '/')) {
    if (*lx->cur == '\\' && lx->cur[1]) {
      lx->cur += 2;
      continue;
    }
    if (*lx->cur == '[') {
      in_class = 1;
    } else if (*lx->cur == ']') {
      in_class = 0;
    }
    lx->cur++;
  }
  if (*lx->cur == '/')
    lx->cur++;
  while (isalpha((unsigned char)*lx->cur))
    lx->cur++;
  return make(lx, tok_regex, start);
}
