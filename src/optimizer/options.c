#include "v6/optimizer_options.h"

#include <string.h>

void v6_optimizer_options_init(v6_optimizer_options* opts) {
  opts->const_fold = 0;
  opts->const_prop = 0;
  opts->algebraic_simplify = 0;
  opts->dead_code = 0;
  opts->dead_store = 0;
  opts->control_flow_simplify = 0;
  opts->branch_merge = 0;
  opts->inline_functions = 0;
  opts->common_subexpr = 0;
  opts->loop_invariant = 0;
  opts->no_whitespace = 0;
  opts->obfuscate = 0;
  opts->no_comments = 0;
}

void v6_optimizer_apply_profile(v6_optimizer_options* opts,
                                v6_opt_profile profile) {
  switch (profile) {
  case v6_opt_profile_o1:
    opts->const_fold = 1;
    opts->algebraic_simplify = 1;
    opts->dead_code = 1;
    break;
  case v6_opt_profile_o2:
    opts->const_fold = 1;
    opts->const_prop = 1;
    opts->algebraic_simplify = 1;
    opts->dead_code = 1;
    opts->dead_store = 1;
    opts->control_flow_simplify = 1;
    opts->branch_merge = 1;
    break;
  case v6_opt_profile_o3:
    opts->const_fold = 1;
    opts->const_prop = 1;
    opts->algebraic_simplify = 1;
    opts->dead_code = 1;
    opts->dead_store = 1;
    opts->control_flow_simplify = 1;
    opts->branch_merge = 1;
    opts->inline_functions = 1;
    opts->common_subexpr = 1;
    opts->loop_invariant = 1;
    break;
  case v6_opt_profile_oz:
    opts->const_fold = 1;
    opts->const_prop = 1;
    opts->algebraic_simplify = 1;
    opts->dead_code = 1;
    opts->dead_store = 1;
    opts->control_flow_simplify = 1;
    opts->branch_merge = 1;
    opts->no_whitespace = 1;
    opts->obfuscate = 1;
    opts->no_comments = 1;
    break;
  case v6_opt_profile_none:
  default:
    break;
  }
}

int v6_optimizer_parse_profile(const char* s, v6_opt_profile* out) {
  if (strcmp(s, "0") == 0) {
    *out = v6_opt_profile_none;
    return 0;
  }
  if (strcmp(s, "1") == 0) {
    *out = v6_opt_profile_o1;
    return 0;
  }
  if (strcmp(s, "2") == 0) {
    *out = v6_opt_profile_o2;
    return 0;
  }
  if (strcmp(s, "3") == 0) {
    *out = v6_opt_profile_o3;
    return 0;
  }
  if (strcmp(s, "z") == 0 || strcmp(s, "Z") == 0) {
    *out = v6_opt_profile_oz;
    return 0;
  }
  return -1;
}

int v6_optimizer_any_js_pass_enabled(const v6_optimizer_options* opts) {
  return opts->const_fold || opts->const_prop || opts->algebraic_simplify ||
         opts->dead_code || opts->dead_store || opts->control_flow_simplify ||
         opts->branch_merge || opts->inline_functions || opts->common_subexpr ||
         opts->loop_invariant || opts->no_whitespace || opts->obfuscate;
}

int v6_optimizer_any_css_pass_enabled(const v6_optimizer_options* opts) {
  return opts->no_whitespace || opts->no_comments;
}
