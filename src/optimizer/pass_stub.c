#include "v6/optimizer_pass.h"

int v6_opt_pass_inline_functions(ast_node* program, ast_arena* arena) {
  (void)program;
  (void)arena;
  return 0;
}

int v6_opt_pass_common_subexpr(ast_node* program, ast_arena* arena) {
  (void)program;
  (void)arena;
  return 0;
}

int v6_opt_pass_loop_invariant(ast_node* program, ast_arena* arena) {
  (void)program;
  (void)arena;
  return 0;
}

void v6_opt_pass_mangle(ast_node* program, ast_arena* arena) {
  (void)program;
  (void)arena;
}
