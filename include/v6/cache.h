#pragma once

#include <stddef.h>

typedef struct v6_cache_entry {
  char* name;
  unsigned char* data;
  size_t len;
} v6_cache_entry;

typedef struct v6_cache_result {
  v6_cache_entry* entries;
  int count;
} v6_cache_result;

int v6_cache_try_load(const char* entry_path, v6_cache_result* out);
void v6_cache_free_result(v6_cache_result* r);

void v6_cache_store(const char* entry_path, const char** tracked_paths,
                    int tracked_count, const char** entry_names,
                    const unsigned char** entry_datas, const size_t* entry_lens,
                    int entry_count);
