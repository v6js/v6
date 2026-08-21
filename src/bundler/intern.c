#include "v6/bundle_intern.h"

#include <stdlib.h>
#include <string.h>

#define bundle_intern_initial_cap 256

unsigned long long bundle_fnv1a(const char* s, size_t len) {
  unsigned long long h = 1469598103934665603ULL;
  for (size_t i = 0; i < len; i++) {
    h ^= (unsigned char)s[i];
    h *= 1099511628211ULL;
  }
  return h;
}

void bundle_intern_init(bundle_intern_table* t, bundle_arena* arena) {
  t->cap = bundle_intern_initial_cap;
  t->count = 0;
  t->arena = arena;
  t->entries = calloc(t->cap, sizeof(bundle_intern_entry));
}

void bundle_intern_free(bundle_intern_table* t) {
  free(t->entries);
  t->entries = NULL;
  t->cap = 0;
  t->count = 0;
}

static void bundle_intern_insert_raw(bundle_intern_entry* entries, size_t cap,
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

static void bundle_intern_grow(bundle_intern_table* t) {
  size_t new_cap = t->cap * 2;
  bundle_intern_entry* new_entries =
      calloc(new_cap, sizeof(bundle_intern_entry));
  for (size_t i = 0; i < t->cap; i++) {
    if (t->entries[i].used) {
      bundle_intern_insert_raw(new_entries, new_cap, t->entries[i].str,
                               t->entries[i].len, t->entries[i].hash);
    }
  }
  free(t->entries);
  t->entries = new_entries;
  t->cap = new_cap;
}

const char* bundle_intern(bundle_intern_table* t, const char* s, size_t len) {
  unsigned long long hash = bundle_fnv1a(s, len);
  size_t idx = (size_t)(hash & (t->cap - 1));
  while (t->entries[idx].used) {
    if (t->entries[idx].hash == hash && t->entries[idx].len == len &&
        memcmp(t->entries[idx].str, s, len) == 0) {
      return t->entries[idx].str;
    }
    idx = (idx + 1) & (t->cap - 1);
  }
  if ((t->count + 1) * 10 > t->cap * 7) {
    bundle_intern_grow(t);
  }
  const char* owned = bundle_arena_strdup(t->arena, s, len);
  bundle_intern_insert_raw(t->entries, t->cap, owned, len, hash);
  t->count++;
  return owned;
}

const char* bundle_intern_cstr(bundle_intern_table* t, const char* s) {
  return bundle_intern(t, s, strlen(s));
}
