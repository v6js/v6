#include "v6/bundle_arena.h"

#include <stdlib.h>
#include <string.h>

#define bundle_arena_block_size (256 * 1024)

void bundle_arena_init(bundle_arena* a) {
  a->head = NULL;
}

void bundle_arena_free(bundle_arena* a) {
  bundle_arena_block* b = a->head;
  while (b) {
    bundle_arena_block* next = b->next;
    free(b);
    b = next;
  }
  a->head = NULL;
}

static bundle_arena_block* bundle_arena_new_block(size_t min_cap) {
  size_t cap = bundle_arena_block_size;
  if (min_cap > cap)
    cap = min_cap;
  bundle_arena_block* b = malloc(sizeof(bundle_arena_block) - 1 + cap);
  b->cap = cap;
  b->used = 0;
  return b;
}

void* bundle_arena_alloc(bundle_arena* a, size_t size) {
  size = (size + 7) & ~(size_t)7;
  if (!a->head || a->head->used + size > a->head->cap) {
    bundle_arena_block* b = bundle_arena_new_block(size);
    b->next = a->head;
    a->head = b;
  }
  void* p = a->head->data + a->head->used;
  a->head->used += size;
  return p;
}

char* bundle_arena_strdup(bundle_arena* a, const char* s, size_t len) {
  char* out = bundle_arena_alloc(a, len + 1);
  memcpy(out, s, len);
  out[len] = '\0';
  return out;
}
