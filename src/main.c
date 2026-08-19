#include "v6/cli.h"
#include "v6/color.h"
#include "v6/repl.h"

int main(int argc, char** argv) {
  v6_cli_options opts;
  v6_cli_action action = v6_cli_parse(argc, argv, &opts);

  switch (action) {
  case v6_action_version:
    v6_cli_print_version();
    return 0;
  case v6_action_help:
    v6_cli_print_help(argv[0]);
    return 0;
  case v6_action_daemon_serve:
    return v6_cli_serve_daemon(opts.daemon_lock_path);
  case v6_action_eval:
    v6_color_init(opts.color_mode);
    return v6_cli_eval(&opts);
  case v6_action_run_script:
    v6_color_init(opts.color_mode);
    return v6_cli_run_script(&opts);
  case v6_action_repl:
    return v6_repl_run(&opts);
  case v6_action_error:
  default:
    v6_cli_print_usage(argv[0]);
    return 1;
  }
}
