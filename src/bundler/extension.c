#include "v6/bundler_extension.h"

#include <stdlib.h>
#include <string.h>

void v6_bundler_extension_set_init(v6_bundler_extension_set* set) {
  set->count = 0;
}

void v6_bundler_extension_set_add(v6_bundler_extension_set* set,
                                  v6_bundler_extension ext) {
  if (set->count >= v6_bundler_max_extensions)
    return;
  set->items[set->count++] = ext;
}

int v6_bundler_extension_run_resolve(v6_bundler_extension_set* set,
                                     const char* importer_dir,
                                     const char* specifier, char* out_path,
                                     size_t out_size) {
  if (!set)
    return -1;
  for (int i = 0; i < set->count; i++) {
    if (!set->items[i].resolve)
      continue;
    if (set->items[i].resolve(set->items[i].state, importer_dir, specifier,
                              out_path, out_size) == 0)
      return 0;
  }
  return -1;
}

char* v6_bundler_extension_run_transform(v6_bundler_extension_set* set,
                                         const char* path, char* source,
                                         size_t source_len, size_t* out_len) {
  char* current = source;
  size_t current_len = source_len;
  if (set) {
    for (int i = 0; i < set->count; i++) {
      if (!set->items[i].transform)
        continue;
      size_t new_len = 0;
      char* next = set->items[i].transform(set->items[i].state, path, current,
                                           current_len, &new_len);
      if (next && next != current) {
        free(current);
        current = next;
        current_len = new_len;
      }
    }
  }
  *out_len = current_len;
  return current;
}

char* v6_bundler_extension_run_finalize(v6_bundler_extension_set* set,
                                        char* output, size_t output_len,
                                        size_t* out_len) {
  char* current = output;
  size_t current_len = output_len;
  if (set) {
    for (int i = 0; i < set->count; i++) {
      if (!set->items[i].finalize)
        continue;
      size_t new_len = 0;
      char* next = set->items[i].finalize(set->items[i].state, current,
                                          current_len, &new_len);
      if (next && next != current) {
        free(current);
        current = next;
        current_len = new_len;
      }
    }
  }
  *out_len = current_len;
  return current;
}

void v6_bundler_extension_run_emit(v6_bundler_extension_set* set,
                                   const char* outdir) {
  if (!set)
    return;
  for (int i = 0; i < set->count; i++) {
    if (set->items[i].emit)
      set->items[i].emit(set->items[i].state, outdir);
  }
}
