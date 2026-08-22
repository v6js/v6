#pragma once

#include "v6/optimizer_options.h"

#include <stddef.h>

char* v6_optimizer_run_js(const char* source, size_t source_len,
                          const v6_optimizer_options* opts, size_t* out_len,
                          char* err_buf, size_t err_buf_size,
                          int* out_err_line);
char* v6_optimizer_run_css(const char* source, size_t source_len,
                           const v6_optimizer_options* opts, size_t* out_len);
char* v6_optimizer_run_json(const char* source, size_t source_len,
                            const v6_optimizer_options* opts, size_t* out_len);
