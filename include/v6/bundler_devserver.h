#pragma once

#include "v6/cli.h"

int v6_bundler_devserver_run(const char* entry, v6_cli_options* opts,
                             const char* outfile, int port);
