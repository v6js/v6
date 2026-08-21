#include "v6/bundler_arena.h"

#include <stdlib.h>
#include <string.h>

#define v6_bundler_arena_block_size (256 * 1024)

void v6_bundler_arena_init(v6_bundler_arena* a) {
  a->head = NULL;
}

void v6_bundler_arena_free(v6_bundler_arena* a) {
  v6_bundler_arena_block* b = a->head;
  while (b) {
    v6_bundler_arena_block* next = b->next;
    free(b);
    b = next;
  }
  a->head = NULL;
}

static v6_bundler_arena_block* v6_bundler_arena_new_block(size_t min_cap) {
  size_t cap = v6_bundler_arena_block_size;
  if (min_cap > cap)
    cap = min_cap;
  v6_bundler_arena_block* b = malloc(sizeof(v6_bundler_arena_block) - 1 + cap);
  b->cap = cap;
  b->used = 0;
  return b;
}

void* v6_bundler_arena_alloc(v6_bundler_arena* a, size_t size) {
  size = (size + 7) & ~(size_t)7;
  if (!a->head || a->head->used + size > a->head->cap) {
    v6_bundler_arena_block* b = v6_bundler_arena_new_block(size);
    b->next = a->head;
    a->head = b;
  }
  void* p = a->head->data + a->head->used;
  a->head->used += size;
  return p;
}

char* v6_bundler_arena_strdup(v6_bundler_arena* a, const char* s, size_t len) {
  char* out = v6_bundler_arena_alloc(a, len + 1);
  memcpy(out, s, len);
  out[len] = '\0';
  return out;
}
