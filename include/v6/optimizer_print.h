#pragma once

#include "v6/optimizer_buf.h"
#include "v6/ast.h"

typedef struct v6_opt_print_opts {
  int minify;
  int indent_size;
} v6_opt_print_opts;

void v6_opt_print_program(v6_opt_buf* out, ast_node* program,
                          const v6_opt_print_opts* opts);
void v6_opt_print_expr(v6_opt_buf* out, ast_node* node,
                       const v6_opt_print_opts* opts);
void v6_opt_print_number(v6_opt_buf* out, double val);
void v6_opt_print_string_literal(v6_opt_buf* out, const char* s, size_t len);
int v6_opt_is_valid_ident_name(const char* s, size_t len);
