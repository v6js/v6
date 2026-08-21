#pragma once

#include "v6/bundler_graph.h"

#include <stddef.h>

typedef struct hmr_snapshot_entry {
  char* id;
  unsigned long long hash;
} hmr_snapshot_entry;

typedef struct hmr_snapshot {
  hmr_snapshot_entry* entries;
  int count;
} hmr_snapshot;

void v6_bundler_hmr_snapshot_init(hmr_snapshot* snap);
void v6_bundler_hmr_snapshot_free(hmr_snapshot* snap);
void v6_bundler_hmr_snapshot_capture(hmr_snapshot* snap, v6_bundler_graph* g);

char* v6_bundler_hmr_compute_patch(hmr_snapshot* prev, v6_bundler_graph* g,
                                   size_t* out_len);
