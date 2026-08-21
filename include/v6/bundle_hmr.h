#pragma once

#include "v6/bundle_graph.h"

#include <stddef.h>

typedef struct hmr_snapshot_entry {
  char* abs_path;
  unsigned long long hash;
} hmr_snapshot_entry;

typedef struct hmr_snapshot {
  hmr_snapshot_entry* entries;
  int count;
} hmr_snapshot;

void bundle_hmr_snapshot_init(hmr_snapshot* snap);
void bundle_hmr_snapshot_free(hmr_snapshot* snap);
void bundle_hmr_snapshot_capture(hmr_snapshot* snap, bundle_graph* g);

char* bundle_hmr_compute_patch(hmr_snapshot* prev, bundle_graph* g,
                               size_t* out_len);
