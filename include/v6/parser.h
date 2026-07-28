#pragma once

#include "v6/bytecode.h"
#include "v6/lexer.h"

#define v6_max_params 16
#define v6_max_fns 64
#define v6_max_locals 128

typedef struct param {
  const char* name;
  size_t len;
} param;

typedef struct local {
  const char* name;
  size_t len;
  uint16_t slot;
} local;

typedef struct fn_entry {
  const char* name;
  size_t len;
  int param_count;
} fn_entry;

typedef struct compiler {
  class_file* cf;
  method* m;
  param params[v6_max_params];
  int param_count;
  local locals[v6_max_locals];
  int local_count;
  uint16_t scratch_slot;
  uint16_t next_local_slot;
  fn_entry fns[v6_max_fns];
  size_t fn_count;
} compiler;

typedef struct parser {
  lexer lex;
  tok cur;
  tok prev;
  int had_error;
} parser;

void parser_init(parser* p, const char* src);
int compile_expr(parser* p, compiler* c);
int compile_program(const char* src, class_file* cf);
