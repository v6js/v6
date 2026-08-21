#pragma once

#include <stddef.h>

typedef struct v6_bundler_arena_block {
  struct v6_bundler_arena_block* next;
  size_t cap;
  size_t used;
  char data[1];
} v6_bundler_arena_block;

typedef struct v6_bundler_arena {
  v6_bundler_arena_block* head;
} v6_bundler_arena;

void v6_bundler_arena_init(v6_bundler_arena* a);
void v6_bundler_arena_free(v6_bundler_arena* a);
void* v6_bundler_arena_alloc(v6_bundler_arena* a, size_t size);
char* v6_bundler_arena_strdup(v6_bundler_arena* a, const char* s, size_t len);
