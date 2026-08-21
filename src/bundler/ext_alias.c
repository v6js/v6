#include "v6/bundler_ext_alias.h"
#include "v6/module.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct v6_bundler_alias_entry {
  char* from;
  char* to;
} v6_bundler_alias_entry;

#define v6_bundler_alias_max_entries 32

struct v6_bundler_alias_state {
  v6_bundler_alias_entry entries[v6_bundler_alias_max_entries];
  int count;
  char base_dir[1024];
};

v6_bundler_alias_state* v6_bundler_alias_state_create(const char* base_dir) {
  v6_bundler_alias_state* state = malloc(sizeof(v6_bundler_alias_state));
  state->count = 0;
  snprintf(state->base_dir, sizeof(state->base_dir), "%s", base_dir);
  return state;
}

void v6_bundler_alias_state_add(v6_bundler_alias_state* state, const char* from,
                                const char* to) {
  if (state->count >= v6_bundler_alias_max_entries)
    return;
  v6_bundler_alias_entry* e = &state->entries[state->count++];
  size_t flen = strlen(from);
  e->from = malloc(flen + 1);
  memcpy(e->from, from, flen + 1);
  size_t tlen = strlen(to);
  e->to = malloc(tlen + 1);
  memcpy(e->to, to, tlen + 1);
}

void v6_bundler_alias_state_free(v6_bundler_alias_state* state) {
  for (int i = 0; i < state->count; i++) {
    free(state->entries[i].from);
    free(state->entries[i].to);
  }
  free(state);
}

static int alias_resolve(void* state_v, const char* importer_dir,
                         const char* specifier, char* out_path,
                         size_t out_size) {
  (void)importer_dir;
  v6_bundler_alias_state* state = (v6_bundler_alias_state*)state_v;

  for (int i = 0; i < state->count; i++) {
    size_t flen = strlen(state->entries[i].from);
    if (strncmp(specifier, state->entries[i].from, flen) != 0)
      continue;

    char combined[1024];
    snprintf(combined, sizeof(combined), "%s%s", state->entries[i].to,
             specifier + flen);

    char qualified[1024];
    const char* for_resolve = combined;
    int looks_relative = combined[0] == '.' || combined[0] == '/' ||
                         combined[0] == '\\' ||
                         (combined[0] != '\0' && combined[1] == ':');
    if (!looks_relative) {
      snprintf(qualified, sizeof(qualified), "./%s", combined);
      for_resolve = qualified;
    }

    char err[256];
    if (resolve_module_specifier(state->base_dir, for_resolve, out_path,
                                 out_size, err, sizeof(err)) == 0)
      return 0;
    return -1;
  }

  return -1;
}

v6_bundler_extension v6_bundler_alias_extension(v6_bundler_alias_state* state) {
  v6_bundler_extension ext;
  ext.name = "alias";
  ext.state = state;
  ext.resolve = alias_resolve;
  ext.transform = NULL;
  ext.finalize = NULL;
  ext.emit = NULL;
  return ext;
}
