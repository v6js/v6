#pragma once

#include "v6/bytecode.h"
#include "v6/lexer.h"

typedef struct parser {
  lexer lex;
  tok cur;
  tok prev;
  int had_error;
} parser;

void parser_init(parser *p, const char *src);
int compile_expr(parser *p, class_file *cf, method *m);
