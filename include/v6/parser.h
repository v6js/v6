#pragma once

#include "v6/bytecode.h"
#include "v6/lexer.h"

#define v6_max_params 16
#define v6_max_fns 64
#define v6_max_locals 128
#define v6_max_loops 32
#define v6_max_breaks 32

typedef struct param {
  const char* name;
  size_t len;
} param;

typedef struct local {
  const char* name;
  size_t len;
  uint16_t slot;
  int is_var;
  int is_const;
  int dead;
} local;

typedef struct fn_entry {
  const char* name;
  size_t len;
  int param_count;
} fn_entry;

typedef struct break_ctx {
  size_t jumps[v6_max_breaks];
  size_t count;
} break_ctx;

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
  break_ctx breaks[v6_max_loops];
  int break_depth;
  size_t continues[v6_max_loops];
  int continue_depth;
} compiler;

typedef struct parser {
  lexer lex;
  tok cur;
  tok prev;
  int had_error;
  char err_msg[160];
  int err_line;
} parser;

typedef struct compile_result {
  int ok;
  char message[160];
  int line;
} compile_result;

void parser_init(parser* p, const char* src);
int compile_expr(parser* p, compiler* c);
compile_result compile_program(const char* src, class_file* cf);
