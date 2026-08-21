#pragma once

#include "v6/color.h"

typedef enum {
  v6_action_run_script,
  v6_action_eval,
  v6_action_repl,
  v6_action_version,
  v6_action_help,
  v6_action_daemon_serve,
  v6_action_bundle,
  v6_action_error,
} v6_cli_action;

typedef struct {
  const char* script_path;
  const char* eval_code;
  const char* out_path;
  const char* classpath;
  const char* daemon_lock_path;
  int no_daemon;
  char** script_args;
  int script_argc;
  v6_color_mode color_mode;
  const char* prog;
  int wasi_deny_args;
  int wasi_deny_env;
  int wasi_deny_random;
  int wasi_deny_clock;

  int bundle_mode;
  const char* bundle_entry;
  const char* bundle_outfile;
  const char* bundle_format;
  const char* bundle_global_name;
  int bundle_watch;
  int bundle_serve;
  int bundle_serve_port;
} v6_cli_options;

v6_cli_action v6_cli_parse(int argc, char** argv, v6_cli_options* opts);
void v6_cli_print_usage(const char* prog);
void v6_cli_print_help(const char* prog);
void v6_cli_print_version(void);

int v6_cli_run_source(const char* src, const char* in_path,
                      v6_cli_options* opts);
int v6_cli_run_wasm(v6_cli_options* opts);
int v6_cli_run_script(v6_cli_options* opts);
int v6_cli_eval(v6_cli_options* opts);
int v6_cli_serve_daemon(const char* lock_path);
int v6_cli_run_bundle(v6_cli_options* opts);
