#include "v6/bundler_intern.h"

#include <stdlib.h>
#include <string.h>

#define v6_bundler_intern_initial_cap 256

unsigned long long v6_bundler_fnv1a(const char* s, size_t len) {
  unsigned long long h = 1469598103934665603ULL;
  for (size_t i = 0; i < len; i++) {
    h ^= (unsigned char)s[i];
    h *= 1099511628211ULL;
  }
  return h;
}

void v6_bundler_intern_init(v6_bundler_intern_table* t, v6_bundler_arena* arena) {
  t->cap = v6_bundler_intern_initial_cap;
  t->count = 0;
  t->arena = arena;
  t->entries = calloc(t->cap, sizeof(v6_bundler_intern_entry));
}

void v6_bundler_intern_free(v6_bundler_intern_table* t) {
  free(t->entries);
  t->entries = NULL;
  t->cap = 0;
  t->count = 0;
}

static void v6_bundler_intern_insert_raw(v6_bundler_intern_entry* entries, size_t cap,
                                     const char* str, size_t len,
                                     unsigned long long hash) {
  size_t idx = (size_t)(hash & (cap - 1));
  while (entries[idx].used) {
    idx = (idx + 1) & (cap - 1);
  }
  entries[idx].str = str;
  entries[idx].len = len;
  entries[idx].hash = hash;
  entries[idx].used = 1;
}

static void v6_bundler_intern_grow(v6_bundler_intern_table* t) {
  size_t new_cap = t->cap * 2;
  v6_bundler_intern_entry* new_entries =
      calloc(new_cap, sizeof(v6_bundler_intern_entry));
  for (size_t i = 0; i < t->cap; i++) {
    if (t->entries[i].used) {
      v6_bundler_intern_insert_raw(new_entries, new_cap, t->entries[i].str,
                               t->entries[i].len, t->entries[i].hash);
    }
  }
  free(t->entries);
  t->entries = new_entries;
  t->cap = new_cap;
}

const char* v6_bundler_intern(v6_bundler_intern_table* t, const char* s, size_t len) {
  unsigned long long hash = v6_bundler_fnv1a(s, len);
  size_t idx = (size_t)(hash & (t->cap - 1));
  while (t->entries[idx].used) {
    if (t->entries[idx].hash == hash && t->entries[idx].len == len &&
        memcmp(t->entries[idx].str, s, len) == 0) {
      return t->entries[idx].str;
    }
    idx = (idx + 1) & (t->cap - 1);
  }
  if ((t->count + 1) * 10 > t->cap * 7) {
    v6_bundler_intern_grow(t);
  }
  const char* owned = v6_bundler_arena_strdup(t->arena, s, len);
  v6_bundler_intern_insert_raw(t->entries, t->cap, owned, len, hash);
  t->count++;
  return owned;
}

const char* v6_bundler_intern_cstr(v6_bundler_intern_table* t, const char* s) {
  return v6_bundler_intern(t, s, strlen(s));
}
