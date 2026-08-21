#include "v6/bundler_hmr.h"
#include "v6/bundler_emit.h"
#include "v6/bundler_intern.h"
#include "v6/bundler_strbuf.h"

#include <stdlib.h>
#include <string.h>

void v6_bundler_hmr_snapshot_init(v6_bundler_hmr_snapshot* snap) {
  snap->entries = NULL;
  snap->count = 0;
}

void v6_bundler_hmr_snapshot_free(v6_bundler_hmr_snapshot* snap) {
  for (int i = 0; i < snap->count; i++)
    free(snap->entries[i].id);
  free(snap->entries);
  snap->entries = NULL;
  snap->count = 0;
}

static unsigned long long module_signature(v6_bundler_module* m) {
  v6_bundler_strbuf sig;
  v6_bundler_strbuf_init(&sig);
  v6_bundler_strbuf_append(&sig, m->source, m->source_len);
  for (int i = 0; i < m->import_count; i++) {
    v6_bundler_strbuf_append_cstr(&sig, "\x01");
    v6_bundler_strbuf_append_cstr(&sig, m->imports[i].specifier);
    v6_bundler_strbuf_append_cstr(&sig, "\x02");
    if (m->imports[i].target)
      v6_bundler_strbuf_append_cstr(&sig, m->imports[i].target->id);
  }
  unsigned long long h = v6_bundler_fnv1a(sig.data, sig.len);
  v6_bundler_strbuf_free(&sig);
  return h;
}

void v6_bundler_hmr_snapshot_capture(v6_bundler_hmr_snapshot* snap, v6_bundler_graph* g) {
  v6_bundler_hmr_snapshot_free(snap);
  snap->entries = malloc(sizeof(v6_bundler_hmr_snapshot_entry) *
                        (size_t)(g->count > 0 ? g->count : 1));
  snap->count = g->count;
  for (int i = 0; i < g->count; i++) {
    size_t n = strlen(g->modules[i]->id);
    snap->entries[i].id = malloc(n + 1);
    memcpy(snap->entries[i].id, g->modules[i]->id, n + 1);
    snap->entries[i].hash = module_signature(g->modules[i]);
  }
}

static int find_prev_hash(v6_bundler_hmr_snapshot* prev, const char* id,
                          unsigned long long* out_hash) {
  for (int i = 0; i < prev->count; i++) {
    if (strcmp(prev->entries[i].id, id) == 0) {
      *out_hash = prev->entries[i].hash;
      return 1;
    }
  }
  return 0;
}

char* v6_bundler_hmr_compute_patch(v6_bundler_hmr_snapshot* prev, v6_bundler_graph* g, size_t* out_len) {
  if (prev->count == 0 || g->count == 0)
    return NULL;

  int* affected = calloc((size_t)g->count, sizeof(int));

  int any_changed = 0;
  for (int i = 0; i < g->count; i++) {
    unsigned long long prev_hash;
    unsigned long long cur_hash = module_signature(g->modules[i]);
    if (!find_prev_hash(prev, g->modules[i]->id, &prev_hash) ||
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
        v6_bundler_module* target = g->modules[i]->imports[j].target;
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

  v6_bundler_module** order;
  int order_count;
  v6_bundler_graph_topo_order(g, &order, &order_count);

  v6_bundler_strbuf b;
  v6_bundler_strbuf_init(&b);
  v6_bundler_strbuf_append_cstr(&b, "(function(){\n");

  v6_bundler_strbuf ids;
  v6_bundler_strbuf_init(&ids);
  int first = 1;

  for (int i = 0; i < order_count; i++) {
    v6_bundler_module* m = order[i];
    int idx = -1;
    for (int k = 0; k < g->count; k++) {
      if (g->modules[k] == m) {
        idx = k;
        break;
      }
    }
    if (idx < 0 || !affected[idx])
      continue;

    v6_bundler_emit_one_module(&b, m);

    if (!first)
      v6_bundler_strbuf_append_cstr(&ids, ",");
    v6_bundler_strbuf_append(&ids, "\"", 1);
    for (const char* p = m->id; *p; p++) {
      if (*p == '"' || *p == '\\')
        v6_bundler_strbuf_append(&ids, "\\", 1);
      v6_bundler_strbuf_append(&ids, p, 1);
    }
    v6_bundler_strbuf_append(&ids, "\"", 1);
    first = 0;
  }
  free(order);
  free(affected);

  v6_bundler_strbuf_append_cstr(&b, "__v6_hmr_apply([");
  v6_bundler_strbuf_append(&b, ids.data, ids.len);
  v6_bundler_strbuf_append_cstr(&b, "]);\n");
  v6_bundler_strbuf_free(&ids);

  v6_bundler_strbuf_append_cstr(&b, "})();\n");

  return v6_bundler_strbuf_take(&b, out_len);
}
