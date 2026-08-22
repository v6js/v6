#include "v6/optimizer.h"
#include "v6/optimizer_asset.h"
#include "v6/optimizer_buf.h"
#include "v6/optimizer_pass.h"
#include "v6/optimizer_print.h"

#include "v6/ast.h"
#include "v6/ast_parse.h"
#include "v6/parser.h"

#include <stdio.h>
#include <string.h>

#define v6_opt_max_fixpoint_iters 20

char* v6_optimizer_run_js(const char* source, size_t source_len,
                          const v6_optimizer_options* opts, size_t* out_len,
                          char* err_buf, size_t err_buf_size,
                          int* out_err_line) {
  (void)source_len;

  ast_arena arena;
  ast_arena_init(&arena);

  parser p;
  parser_init(&p, source);
  ast_node* program = ast_parse_program_from(&arena, &p);

  if (p.had_error) {
    if (err_buf && err_buf_size > 0)
      snprintf(err_buf, err_buf_size, "%s", p.err_msg);
    if (out_err_line)
      *out_err_line = p.err_line;
    ast_arena_free(&arena);
    return NULL;
  }

  for (int iter = 0; iter < v6_opt_max_fixpoint_iters; iter++) {
    int changed = 0;
    if (opts->const_fold)
      changed |= v6_opt_pass_const_fold(program, &arena);
    if (opts->const_prop)
      changed |= v6_opt_pass_const_prop(program, &arena);
    if (opts->algebraic_simplify)
      changed |= v6_opt_pass_algebraic_simplify(program, &arena);
    if (opts->dead_code)
      changed |= v6_opt_pass_dead_code(program, &arena);
    if (opts->dead_store)
      changed |= v6_opt_pass_dead_store(program, &arena);
    if (opts->control_flow_simplify)
      changed |= v6_opt_pass_control_flow_simplify(program, &arena);
    if (!changed)
      break;
  }

  if (opts->inline_functions) {
    v6_opt_pass_inline_functions(program, &arena);
    for (int iter = 0; iter < v6_opt_max_fixpoint_iters; iter++) {
      int changed = 0;
      if (opts->const_fold)
        changed |= v6_opt_pass_const_fold(program, &arena);
      if (opts->algebraic_simplify)
        changed |= v6_opt_pass_algebraic_simplify(program, &arena);
      if (opts->dead_code)
        changed |= v6_opt_pass_dead_code(program, &arena);
      if (opts->dead_store)
        changed |= v6_opt_pass_dead_store(program, &arena);
      if (opts->control_flow_simplify)
        changed |= v6_opt_pass_control_flow_simplify(program, &arena);
      if (!changed)
        break;
    }
  }

  if (opts->common_subexpr || opts->loop_invariant) {
    int any_hoist_changed = 0;
    for (int iter = 0; iter < v6_opt_max_fixpoint_iters; iter++) {
      int changed = 0;
      if (opts->common_subexpr)
        changed |= v6_opt_pass_common_subexpr(program, &arena);
      if (opts->loop_invariant)
        changed |= v6_opt_pass_loop_invariant(program, &arena);
      any_hoist_changed |= changed;
      if (!changed)
        break;
    }
    if (any_hoist_changed) {
      for (int iter = 0; iter < v6_opt_max_fixpoint_iters; iter++) {
        int changed = 0;
        if (opts->const_fold)
          changed |= v6_opt_pass_const_fold(program, &arena);
        if (opts->algebraic_simplify)
          changed |= v6_opt_pass_algebraic_simplify(program, &arena);
        if (opts->dead_code)
          changed |= v6_opt_pass_dead_code(program, &arena);
        if (opts->dead_store)
          changed |= v6_opt_pass_dead_store(program, &arena);
        if (opts->control_flow_simplify)
          changed |= v6_opt_pass_control_flow_simplify(program, &arena);
        if (!changed)
          break;
      }
    }
  }
  if (opts->obfuscate)
    v6_opt_pass_mangle(program, &arena);

  v6_opt_buf out;
  v6_opt_buf_init(&out);
  v6_opt_print_opts print_opts;
  print_opts.minify = opts->no_whitespace;
  print_opts.indent_size = 2;
  v6_opt_print_program(&out, program, &print_opts);

  ast_arena_free(&arena);

  return v6_opt_buf_take(&out, out_len);
}

char* v6_optimizer_run_css(const char* source, size_t source_len,
                           const v6_optimizer_options* opts, size_t* out_len) {
  return v6_opt_strip_css(source, source_len, opts->no_whitespace,
                          opts->no_comments, out_len);
}

char* v6_optimizer_run_json(const char* source, size_t source_len,
                            const v6_optimizer_options* opts, size_t* out_len) {
  return v6_opt_strip_json_whitespace(source, source_len, opts->no_whitespace,
                                      out_len);
}
