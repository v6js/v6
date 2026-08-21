#include "v6/bundle_assets.h"
#include "v6/bundle_fsutil.h"
#include "v6/bundle_intern.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct asset_dedup_entry {
  unsigned long long hash;
  const char* url;
} asset_dedup_entry;

static const char* basename_of(const char* path) {
  const char* slash = strrchr(path, '/');
  const char* bslash = strrchr(path, '\\');
  const char* last = slash;
  if (bslash && (!last || bslash > last))
    last = bslash;
  return last ? last + 1 : path;
}

static void split_stem_ext(const char* base, char* stem, size_t stem_size,
                           char* ext, size_t ext_size) {
  const char* dot = strrchr(base, '.');
  if (!dot || dot == base) {
    snprintf(stem, stem_size, "%s", base);
    ext[0] = '\0';
    return;
  }
  size_t stem_len = (size_t)(dot - base);
  if (stem_len >= stem_size)
    stem_len = stem_size - 1;
  memcpy(stem, base, stem_len);
  stem[stem_len] = '\0';
  snprintf(ext, ext_size, "%s", dot);
}

int bundle_process_assets(bundle_graph* g, const char* outdir) {
  asset_dedup_entry* dedup = NULL;
  int dedup_count = 0, dedup_cap = 0;

  char assets_dir[1024];
  snprintf(assets_dir, sizeof(assets_dir), "%s/assets", outdir);
  int made_dir = 0;

  for (int i = 0; i < g->count; i++) {
    bundle_module* m = g->modules[i];
    if (m->kind != bundle_mod_css && m->kind != bundle_mod_asset)
      continue;

    unsigned long long hash = bundle_fnv1a(m->source, m->source_len);

    const char* existing_url = NULL;
    for (int j = 0; j < dedup_count; j++) {
      if (dedup[j].hash == hash) {
        existing_url = dedup[j].url;
        break;
      }
    }

    if (existing_url) {
      m->asset_url = bundle_intern_cstr(&g->intern, existing_url);
      continue;
    }

    char stem[512], ext[64];
    split_stem_ext(basename_of(m->abs_path), stem, sizeof(stem), ext, sizeof(ext));

    char hashed_name[600];
    snprintf(hashed_name, sizeof(hashed_name), "%s.%08llx%s", stem,
             hash & 0xffffffffULL, ext);

    char url[700];
    snprintf(url, sizeof(url), "/assets/%s", hashed_name);
    m->asset_url = bundle_intern_cstr(&g->intern, url);

    if (!made_dir) {
      bundle_mkdir_p(assets_dir);
      made_dir = 1;
    }

    char dst_path[1200];
    snprintf(dst_path, sizeof(dst_path), "%s/%s", assets_dir, hashed_name);
    if (bundle_write_file(dst_path, m->source, m->source_len) != 0) {
      free(dedup);
      return -1;
    }

    if (dedup_count >= dedup_cap) {
      dedup_cap = dedup_cap == 0 ? 8 : dedup_cap * 2;
      dedup = realloc(dedup, sizeof(asset_dedup_entry) * (size_t)dedup_cap);
    }
    dedup[dedup_count].hash = hash;
    dedup[dedup_count].url = m->asset_url;
    dedup_count++;
  }

  free(dedup);
  return 0;
}
