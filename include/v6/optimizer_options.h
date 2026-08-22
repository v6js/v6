#pragma once

typedef enum {
  v6_opt_profile_none,
  v6_opt_profile_o1,
  v6_opt_profile_o2,
  v6_opt_profile_o3,
  v6_opt_profile_oz,
} v6_opt_profile;

typedef struct v6_optimizer_options {
  int const_fold;
  int const_prop;
  int algebraic_simplify;
  int dead_code;
  int dead_store;
  int control_flow_simplify;
  int branch_merge;
  int inline_functions;
  int common_subexpr;
  int loop_invariant;
  int no_whitespace;
  int obfuscate;
  int no_comments;
} v6_optimizer_options;

void v6_optimizer_options_init(v6_optimizer_options* opts);
void v6_optimizer_apply_profile(v6_optimizer_options* opts,
                                v6_opt_profile profile);
int v6_optimizer_parse_profile(const char* s, v6_opt_profile* out);
int v6_optimizer_any_js_pass_enabled(const v6_optimizer_options* opts);
int v6_optimizer_any_css_pass_enabled(const v6_optimizer_options* opts);
