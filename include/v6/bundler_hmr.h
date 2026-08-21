#pragma once

#include "v6/bundler_graph.h"

#include <stddef.h>

typedef struct v6_bundler_hmr_snapshot_entry {
  char* id;
  unsigned long long hash;
} v6_bundler_hmr_snapshot_entry;

typedef struct v6_bundler_hmr_snapshot {
  v6_bundler_hmr_snapshot_entry* entries;
  int count;
} v6_bundler_hmr_snapshot;

void v6_bundler_hmr_snapshot_init(v6_bundler_hmr_snapshot* snap);
void v6_bundler_hmr_snapshot_free(v6_bundler_hmr_snapshot* snap);
void v6_bundler_hmr_snapshot_capture(v6_bundler_hmr_snapshot* snap,
                                     v6_bundler_graph* g);

char* v6_bundler_hmr_compute_patch(v6_bundler_hmr_snapshot* prev,
                                   v6_bundler_graph* g, size_t* out_len);
