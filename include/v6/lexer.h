#pragma once

#include <stddef.h>

typedef enum {
  tok_eof,
  tok_error,
  tok_num,
  tok_str,
  tok_ident,
  tok_kw_var,
  tok_kw_let,
  tok_kw_const,
  tok_kw_function,
  tok_kw_return,
  tok_kw_if,
  tok_kw_else,
  tok_kw_while,
  tok_kw_for,
  tok_kw_true,
  tok_kw_false,
  tok_kw_null,
  tok_kw_undefined,
  tok_kw_break,
  tok_kw_continue,
  tok_kw_switch,
  tok_kw_case,
  tok_kw_default,
  tok_kw_typeof,
  tok_kw_in,
  tok_kw_of,
  tok_lparen,
  tok_rparen,
  tok_lbrace,
  tok_rbrace,
  tok_lbracket,
  tok_rbracket,
  tok_semi,
  tok_comma,
  tok_dot,
  tok_plus,
  tok_minus,
  tok_star,
  tok_slash,
  tok_percent,
  tok_assign,
  tok_eq,
  tok_neq,
  tok_eq_strict,
  tok_neq_strict,
  tok_lt,
  tok_gt,
  tok_le,
  tok_ge,
  tok_amp_amp,
  tok_pipe_pipe,
  tok_bang,
  tok_plus_eq,
  tok_minus_eq,
  tok_star_eq,
  tok_slash_eq,
  tok_percent_eq,
  tok_plus_plus,
  tok_minus_minus,
  tok_question,
  tok_colon,
  tok_amp,
  tok_pipe,
  tok_caret,
  tok_tilde,
  tok_shl,
  tok_shr,
  tok_ushr,
  tok_amp_eq,
  tok_pipe_eq,
  tok_caret_eq,
  tok_shl_eq,
  tok_shr_eq,
  tok_ushr_eq,
} tok_kind;

typedef struct tok {
  tok_kind kind;
  const char* start;
  size_t len;
  int line;
  double num;
} tok;

typedef struct lexer {
  const char* src;
  const char* cur;
  int line;
} lexer;

void lex_init(lexer* lx, const char* src);
tok lex_next(lexer* lx);
