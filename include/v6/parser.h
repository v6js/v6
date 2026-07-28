#pragma once

#include "v6/bytecode.h"
#include "v6/lexer.h"

#define v6_max_params 16
#define v6_max_locals 128
#define v6_max_loops 32
#define v6_max_breaks 32
#define v6_max_upvalues 64
#define v6_max_catches 16

typedef struct param {
  const char* name;
  size_t len;
  uint16_t slot;
} param;

typedef struct local {
  const char* name;
  size_t len;
  uint16_t slot;
  int is_var;
  int is_const;
  int dead;
} local;

typedef struct upvalue {
  const char* name;
  size_t len;
  int from_parent_local;
  uint16_t parent_index;
} upvalue;

typedef struct break_ctx {
  size_t jumps[v6_max_breaks];
  size_t count;
} break_ctx;

typedef struct compiler {
  class_file* cf;
  method* m;
  struct compiler* parent;
  int* lambda_counter;
  int is_arrow;
  param params[v6_max_params];
  int param_count;
  local locals[v6_max_locals];
  int local_count;
  uint16_t scratch_slot;
  uint16_t next_local_slot;
  upvalue upvalues[v6_max_upvalues];
  int upvalue_count;
  break_ctx breaks[v6_max_loops];
  int break_depth;
  size_t continues[v6_max_loops];
  int continue_depth;
  int catch_depth;
  int brace_depth;
  const char* super_name;
  size_t super_len;
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
