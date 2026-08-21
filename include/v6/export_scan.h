#pragma once

#include <stddef.h>

#define v6_max_exports 128

typedef struct export_binding {
  char local_name[64];
  char export_key[64];
} export_binding;

void preprocess_exports(char* src, export_binding* bindings, int* count);
