#pragma once

#include "v6/bytecode.h"

#include <stddef.h>

#define v6_max_modules 256
#define v6_max_path 1024

typedef struct compiled_module {
  char abs_path[v6_max_path];
  int kind;
  char class_name[32];
  class_file* cf;
  int state;
} compiled_module;

typedef struct module_ctx {
  compiled_module modules[v6_max_modules];
  int count;
} module_ctx;

void module_ctx_init(module_ctx* mc);

int resolve_module_specifier(const char* importer_dir, const char* specifier,
                             char* out_path, size_t out_size, char* err,
                             size_t err_size);

void path_dirname(const char* path, char* out, size_t out_size);
void path_normalize(const char* path, char* out, size_t out_size);
int path_is_regular_file(const char* path);
