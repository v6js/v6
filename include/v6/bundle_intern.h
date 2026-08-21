#pragma once

#include "v6/bundle_arena.h"

#include <stddef.h>

typedef struct bundle_intern_entry {
  const char* str;
  size_t len;
  unsigned long long hash;
  int used;
} bundle_intern_entry;

typedef struct bundle_intern_table {
  bundle_intern_entry* entries;
  size_t cap;
  size_t count;
  bundle_arena* arena;
} bundle_intern_table;

void bundle_intern_init(bundle_intern_table* t, bundle_arena* arena);
void bundle_intern_free(bundle_intern_table* t);
const char* bundle_intern(bundle_intern_table* t, const char* s, size_t len);
const char* bundle_intern_cstr(bundle_intern_table* t, const char* s);
unsigned long long bundle_fnv1a(const char* s, size_t len);
