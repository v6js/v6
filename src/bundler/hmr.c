#include "v6/bundle_hmr.h"
#include "v6/bundle_emit.h"
#include "v6/bundle_intern.h"
#include "v6/bundle_strbuf.h"

#include <stdlib.h>
#include <string.h>

void bundle_hmr_snapshot_init(hmr_snapshot* snap) {
  snap->entries = NULL;
  snap->count = 0;
}

void bundle_hmr_snapshot_free(hmr_snapshot* snap) {
  for (int i = 0; i < snap->count; i++)
    free(snap->entries[i].abs_path);
  free(snap->entries);
  snap->entries = NULL;
  snap->count = 0;
}

static unsigned long long module_signature(bundle_module* m) {
  bundle_strbuf sig;
  bundle_strbuf_init(&sig);
  bundle_strbuf_append(&sig, m->source, m->source_len);
  for (int i = 0; i < m->import_count; i++) {
    bundle_strbuf_append_cstr(&sig, "\x01");
    bundle_strbuf_append_cstr(&sig, m->imports[i].specifier);
    bundle_strbuf_append_cstr(&sig, "\x02");
    if (m->imports[i].target)
      bundle_strbuf_append_cstr(&sig, m->imports[i].target->abs_path);
  }
  unsigned long long h = bundle_fnv1a(sig.data, sig.len);
  bundle_strbuf_free(&sig);
  return h;
}

void bundle_hmr_snapshot_capture(hmr_snapshot* snap, bundle_graph* g) {
  bundle_hmr_snapshot_free(snap);
  snap->entries = malloc(sizeof(hmr_snapshot_entry) * (size_t)(g->count > 0 ? g->count : 1));
  snap->count = g->count;
  for (int i = 0; i < g->count; i++) {
    size_t n = strlen(g->modules[i]->abs_path);
    snap->entries[i].abs_path = malloc(n + 1);
    memcpy(snap->entries[i].abs_path, g->modules[i]->abs_path, n + 1);
    snap->entries[i].hash = module_signature(g->modules[i]);
  }
}

static int find_prev_hash(hmr_snapshot* prev, const char* abs_path,
                          unsigned long long* out_hash) {
  for (int i = 0; i < prev->count; i++) {
    if (strcmp(prev->entries[i].abs_path, abs_path) == 0) {
      *out_hash = prev->entries[i].hash;
      return 1;
    }
  }
  return 0;
}

char* bundle_hmr_compute_patch(hmr_snapshot* prev, bundle_graph* g, size_t* out_len) {
  if (prev->count == 0 || g->count == 0)
    return NULL;

  int* affected = calloc((size_t)g->count, sizeof(int));

  int any_changed = 0;
  for (int i = 0; i < g->count; i++) {
    unsigned long long prev_hash;
    unsigned long long cur_hash = module_signature(g->modules[i]);
    if (!find_prev_hash(prev, g->modules[i]->abs_path, &prev_hash) ||
        prev_hash != cur_hash) {
      affected[i] = 1;
      any_changed = 1;
    }
  }

  if (!any_changed) {
    free(affected);
    return NULL;
  }

  int changed_more = 1;
  while (changed_more) {
    changed_more = 0;
    for (int i = 0; i < g->count; i++) {
      if (affected[i])
        continue;
      for (int j = 0; j < g->modules[i]->import_count; j++) {
        bundle_module* target = g->modules[i]->imports[j].target;
        if (!target)
          continue;
        for (int k = 0; k < g->count; k++) {
          if (g->modules[k] == target && affected[k]) {
            affected[i] = 1;
            changed_more = 1;
            break;
          }
        }
        if (affected[i])
          break;
      }
    }
  }

  bundle_module** order;
  int order_count;
  bundle_graph_topo_order(g, &order, &order_count);

  bundle_strbuf b;
  bundle_strbuf_init(&b);
  bundle_strbuf_append_cstr(&b, "(function(){\n");

  bundle_strbuf ids;
  bundle_strbuf_init(&ids);
  int first = 1;

  for (int i = 0; i < order_count; i++) {
    bundle_module* m = order[i];
    int idx = -1;
    for (int k = 0; k < g->count; k++) {
      if (g->modules[k] == m) {
        idx = k;
        break;
      }
    }
    if (idx < 0 || !affected[idx])
      continue;

    bundle_emit_one_module(&b, m);

    if (!first)
      bundle_strbuf_append_cstr(&ids, ",");
    bundle_strbuf_append(&ids, "\"", 1);
    for (const char* p = m->abs_path; *p; p++) {
      if (*p == '"' || *p == '\\')
        bundle_strbuf_append(&ids, "\\", 1);
      bundle_strbuf_append(&ids, p, 1);
    }
    bundle_strbuf_append(&ids, "\"", 1);
    first = 0;
  }
  free(order);
  free(affected);

  bundle_strbuf_append_cstr(&b, "__v6_hmr_apply([");
  bundle_strbuf_append(&b, ids.data, ids.len);
  bundle_strbuf_append_cstr(&b, "]);\n");
  bundle_strbuf_free(&ids);

  bundle_strbuf_append_cstr(&b, "})();\n");

  return bundle_strbuf_take(&b, out_len);
}
