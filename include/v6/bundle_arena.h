#pragma once

#include <stddef.h>

typedef struct bundle_arena_block {
  struct bundle_arena_block* next;
  size_t cap;
  size_t used;
  char data[1];
} bundle_arena_block;

typedef struct bundle_arena {
  bundle_arena_block* head;
} bundle_arena;

void bundle_arena_init(bundle_arena* a);
void bundle_arena_free(bundle_arena* a);
void* bundle_arena_alloc(bundle_arena* a, size_t size);
char* bundle_arena_strdup(bundle_arena* a, const char* s, size_t len);
