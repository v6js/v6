#pragma once

#include "v6/bytecode.h"
#include "v6/lexer.h"

struct module_ctx;

#define v6_max_params 64
#define v6_max_locals 4096
#define v6_initial_locals 24
#define v6_max_loops 32
#define v6_max_breaks 32
#define v6_max_upvalues 1024
#define v6_initial_upvalues 16
#define v6_max_catches 16
#define v6_max_fields 32
#define v6_max_labels 32
#define v6_max_pending_labels 8
#define v6_max_pending_finally 16

typedef struct field_init {
  const char* name;
  size_t name_len;
  void* init_ast;
} field_init;

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
  local* locals;
  int local_count;
  int local_cap;
  uint16_t scratch_slot;
  uint16_t next_local_slot;
  upvalue* upvalues;
  int upvalue_count;
  int upvalue_cap;
  break_ctx breaks[v6_max_loops];
  int break_depth;
  size_t continues[v6_max_loops];
  int continue_depth;
  int catch_depth;
  int brace_depth;
  const char* super_name;
  size_t super_len;
  const char* class_name;
  size_t class_name_len;
  field_init pending_fields[v6_max_fields];
  int pending_field_count;
  int box_locals;
  int use_frame_locals;
  uint16_t frame_slot;
  uint16_t next_frame_slot;
  const char* label_names[v6_max_labels];
  size_t label_lens[v6_max_labels];
  int label_break_depth[v6_max_labels];
  int label_continue_depth[v6_max_labels];
  int label_count;
  const char* pending_label_names[v6_max_pending_labels];
  size_t pending_label_lens[v6_max_pending_labels];
  int pending_label_count;
  void* finally_ast[v6_max_pending_finally];
  int finally_break_depth[v6_max_pending_finally];
  int finally_continue_depth[v6_max_pending_finally];
  int finally_depth;
  int is_async_gen;
  int is_module;
  uint16_t exports_slot;
  const char* this_class_name;
  struct module_ctx* modctx;
  const char* module_dir;
} compiler;

typedef struct parser {
  lexer lex;
  tok cur;
  tok prev;
  int had_error;
  char err_msg[1024];
  int err_line;
} parser;

typedef struct compile_result {
  int ok;
  char message[1024];
  int line;
} compile_result;

void parser_init(parser* p, const char* src);
int compile_expr(parser* p, compiler* c);
compile_result compile_program(const char* src, class_file* cf,
                               const char* entry_path,
                               struct module_ctx* modctx);
