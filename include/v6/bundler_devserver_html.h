#pragma once

#include "v6/cli.h"

int v6_bundler_run_watch_loop_html(const char* html_path, v6_cli_options* opts,
                                   const char* outdir);
int v6_bundler_devserver_run_html(const char* html_path, v6_cli_options* opts,
                                  const char* outdir, int port);
