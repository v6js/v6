#pragma once

#include "v6/ast.h"

int v6_opt_pass_const_fold(ast_node* program, ast_arena* arena);
int v6_opt_pass_const_prop(ast_node* program, ast_arena* arena);
int v6_opt_pass_algebraic_simplify(ast_node* program, ast_arena* arena);
int v6_opt_pass_dead_code(ast_node* program, ast_arena* arena);
int v6_opt_pass_dead_store(ast_node* program, ast_arena* arena);
int v6_opt_pass_control_flow_simplify(ast_node* program, ast_arena* arena);
int v6_opt_pass_inline_functions(ast_node* program, ast_arena* arena);
int v6_opt_pass_common_subexpr(ast_node* program, ast_arena* arena);
int v6_opt_pass_loop_invariant(ast_node* program, ast_arena* arena);
void v6_opt_pass_mangle(ast_node* program, ast_arena* arena);
