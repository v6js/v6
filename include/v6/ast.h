#pragma once

#include "v6/lexer.h"

#include <stddef.h>
#include <stdint.h>

typedef enum {
  ast_num,
  ast_bigint,
  ast_str,
  ast_template,
  ast_tagged_template,
  ast_regex,
  ast_bool,
  ast_null,
  ast_undef,
  ast_ident,
  ast_this,
  ast_new_target,
  ast_super_call,
  ast_super_member,
  ast_array_lit,
  ast_object_lit,
  ast_func_expr,
  ast_class_expr,
  ast_unary,
  ast_update,
  ast_binary,
  ast_logical,
  ast_assign,
  ast_cond,
  ast_call,
  ast_new,
  ast_member,
  ast_seq,
  ast_spread,
  ast_yield,
  ast_await,
  ast_require,
  ast_paren_pattern_assign,

  ast_pat_ident,
  ast_pat_array,
  ast_pat_object,
  ast_pat_assign,
  ast_pat_rest,
  ast_pat_hole,

  ast_program,
  ast_block,
  ast_expr_stmt,
  ast_var_decl,
  ast_func_decl,
  ast_class_decl,
  ast_if,
  ast_for,
  ast_for_in,
  ast_for_of,
  ast_while,
  ast_do_while,
  ast_switch,
  ast_try,
  ast_throw,
  ast_return,
  ast_break,
  ast_continue,
  ast_labeled,
  ast_empty,
  ast_debugger,
  ast_import,
} ast_kind;

typedef struct ast_node ast_node;

typedef struct ast_list {
  ast_node** items;
  int len;
  int cap;
} ast_list;

typedef struct ast_prop {
  ast_node* key;
  ast_node* value;
  int computed;
  int shorthand;
  int is_spread;
  int is_getter;
  int is_setter;
  int is_method;
  int is_async;
  int is_generator;
} ast_prop;

typedef struct ast_prop_list {
  ast_prop* items;
  int len;
  int cap;
} ast_prop_list;

typedef struct ast_class_member {
  ast_node* key;
  ast_node* value;
  int computed;
  int is_static;
  int is_getter;
  int is_setter;
  int is_ctor;
  int is_field;
  int is_async;
  int is_generator;
} ast_class_member;

typedef struct ast_class_member_list {
  ast_class_member* items;
  int len;
  int cap;
} ast_class_member_list;

typedef struct ast_param {
  ast_node* pattern;
  int is_rest;
} ast_param;

typedef struct ast_param_list {
  ast_param* items;
  int len;
  int cap;
} ast_param_list;

typedef struct ast_switch_case {
  ast_node* test;
  ast_list body;
} ast_switch_case;

typedef struct ast_switch_case_list {
  ast_switch_case* items;
  int len;
  int cap;
} ast_switch_case_list;

typedef struct ast_import_named {
  const char* key;
  size_t key_len;
  const char* local;
  size_t local_len;
} ast_import_named;

typedef struct ast_import_binding {
  int has_default;
  const char* default_name;
  size_t default_len;
  int has_namespace;
  const char* namespace_name;
  size_t namespace_len;
  ast_import_named named[64];
  int named_count;
  int is_bare;
} ast_import_binding;

struct ast_node {
  ast_kind kind;
  int line;

  const char* str;
  size_t str_len;
  const char* str2;
  size_t str2_len;
  double num;
  int is_bigint;

  tok_kind op;
  int flag_a;
  int flag_b;
  int flag_c;
  int flag_d;

  ast_node* a;
  ast_node* b;
  ast_node* c;
  ast_node* d;

  ast_list list;
  ast_list quasis_cooked;
  ast_list quasis_raw;
  ast_prop_list props;
  ast_class_member_list members;
  ast_param_list params;
  ast_switch_case_list cases;

  const char* raw_src;
  ast_import_binding* import_binding;

  char* text_owned;
};

typedef struct ast_arena_block {
  struct ast_arena_block* next;
  size_t cap;
  size_t used;
  char data[1];
} ast_arena_block;

typedef struct ast_arena {
  ast_arena_block* head;
} ast_arena;

void ast_arena_init(ast_arena* a);
void ast_arena_free(ast_arena* a);
void* ast_arena_alloc(ast_arena* a, size_t size);
char* ast_arena_strdup(ast_arena* a, const char* s, size_t len);

ast_node* ast_new_node(ast_arena* a, ast_kind kind, int line);
void ast_list_push(ast_arena* a, ast_list* list, ast_node* node);
void ast_prop_list_push(ast_arena* a, ast_prop_list* list, ast_prop prop);
void ast_class_member_list_push(ast_arena* a, ast_class_member_list* list,
                                ast_class_member member);
void ast_param_list_push(ast_arena* a, ast_param_list* list, ast_param param);
void ast_switch_case_list_push(ast_arena* a, ast_switch_case_list* list,
                               ast_switch_case c);
