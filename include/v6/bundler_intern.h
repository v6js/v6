#pragma once

#include "v6/bundler_arena.h"

#include <stddef.h>

typedef struct v6_bundler_intern_entry {
  const char* str;
  size_t len;
  unsigned long long hash;
  int used;
} v6_bundler_intern_entry;

typedef struct v6_bundler_intern_table {
  v6_bundler_intern_entry* entries;
  size_t cap;
  size_t count;
  v6_bundler_arena* arena;
} v6_bundler_intern_table;

void v6_bundler_intern_init(v6_bundler_intern_table* t,
                            v6_bundler_arena* arena);
void v6_bundler_intern_free(v6_bundler_intern_table* t);
const char* v6_bundler_intern(v6_bundler_intern_table* t, const char* s,
                              size_t len);
const char* v6_bundler_intern_cstr(v6_bundler_intern_table* t, const char* s);
unsigned long long v6_bundler_fnv1a(const char* s, size_t len);
