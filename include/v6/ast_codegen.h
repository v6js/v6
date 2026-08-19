#pragma once

#include "v6/ast.h"
#include "v6/internal.h"

void ast_codegen_stmt_list(compiler* c, ast_list* body);
void ast_codegen_stmt(compiler* c, ast_node* s);
void ast_codegen_expr(compiler* c, ast_node* e);
void ast_codegen_function_value(compiler* c, ast_node* fn,
                                char* out_lambda_name);

void ast_cg_reset_error(void);
int ast_cg_had_error(char* out_msg, size_t out_msg_cap, int* out_line);
