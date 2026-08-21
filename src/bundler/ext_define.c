#include "v6/bundler_ext_define.h"
#include "v6/bundler_strbuf.h"
#include "v6/lexer.h"

#include <stdlib.h>
#include <string.h>

#define v6_bundler_define_max_parts 8

typedef struct v6_bundler_define_entry {
  char* key;
  char* value;
  char* parts[v6_bundler_define_max_parts];
  int part_count;
} v6_bundler_define_entry;

#define v6_bundler_define_max_entries 64

struct v6_bundler_define_state {
  v6_bundler_define_entry entries[v6_bundler_define_max_entries];
  int count;
};

v6_bundler_define_state* v6_bundler_define_state_create(void) {
  v6_bundler_define_state* state = malloc(sizeof(v6_bundler_define_state));
  state->count = 0;
  return state;
}

void v6_bundler_define_state_add(v6_bundler_define_state* state,
                                 const char* key, const char* value) {
  if (state->count >= v6_bundler_define_max_entries)
    return;
  v6_bundler_define_entry* e = &state->entries[state->count++];
  size_t klen = strlen(key);
  e->key = malloc(klen + 1);
  memcpy(e->key, key, klen + 1);
  size_t vlen = strlen(value);
  e->value = malloc(vlen + 1);
  memcpy(e->value, value, vlen + 1);

  e->part_count = 0;
  char buf[512];
  size_t n = klen >= sizeof(buf) ? sizeof(buf) - 1 : klen;
  memcpy(buf, key, n);
  buf[n] = '\0';
  char* p = buf;
  while (*p && e->part_count < v6_bundler_define_max_parts) {
    char* dot = strchr(p, '.');
    if (dot)
      *dot = '\0';
    size_t plen = strlen(p);
    char* part = malloc(plen + 1);
    memcpy(part, p, plen + 1);
    e->parts[e->part_count++] = part;
    if (!dot)
      break;
    p = dot + 1;
  }
}

void v6_bundler_define_state_free(v6_bundler_define_state* state) {
  for (int i = 0; i < state->count; i++) {
    free(state->entries[i].key);
    free(state->entries[i].value);
    for (int j = 0; j < state->entries[i].part_count; j++)
      free(state->entries[i].parts[j]);
  }
  free(state);
}

static int try_match(lexer* lx, tok first, v6_bundler_define_entry* e,
                     const char** out_end) {
  if (e->part_count == 0)
    return 0;
  if (strlen(e->parts[0]) != first.len ||
      memcmp(e->parts[0], first.start, first.len) != 0)
    return 0;
  if (e->part_count == 1) {
    *out_end = first.start + first.len;
    return 1;
  }
  lexer peek = *lx;
  const char* end = first.start + first.len;
  for (int i = 1; i < e->part_count; i++) {
    tok dot = lex_next(&peek);
    if (dot.kind != tok_dot)
      return 0;
    tok part = lex_next(&peek);
    if (part.kind != tok_ident || strlen(e->parts[i]) != part.len ||
        memcmp(e->parts[i], part.start, part.len) != 0)
      return 0;
    end = part.start + part.len;
  }
  *lx = peek;
  *out_end = end;
  return 1;
}

static int is_js_path(const char* path) {
  const char* dot = strrchr(path, '.');
  if (!dot)
    return 0;
  return strcmp(dot, ".js") == 0 || strcmp(dot, ".mjs") == 0 ||
         strcmp(dot, ".cjs") == 0;
}

static char* define_transform(void* state_v, const char* path, char* source,
                              size_t source_len, size_t* out_len) {
  v6_bundler_define_state* state = (v6_bundler_define_state*)state_v;
  if (state->count == 0 || !is_js_path(path)) {
    *out_len = source_len;
    return source;
  }

  v6_bundler_strbuf out;
  v6_bundler_strbuf_init(&out);

  lexer lx;
  lex_init(&lx, source);
  const char* prev_end = source;

  for (;;) {
    tok t = lex_next(&lx);
    if (t.start > prev_end)
      v6_bundler_strbuf_append(&out, prev_end, (size_t)(t.start - prev_end));
    if (t.kind == tok_eof)
      break;

    if (t.kind == tok_ident) {
      int matched = 0;
      for (int i = 0; i < state->count; i++) {
        const char* end = NULL;
        if (try_match(&lx, t, &state->entries[i], &end)) {
          v6_bundler_strbuf_append_cstr(&out, state->entries[i].value);
          prev_end = end;
          matched = 1;
          break;
        }
      }
      if (!matched) {
        v6_bundler_strbuf_append(&out, t.start, t.len);
        prev_end = t.start + t.len;
      }
    } else {
      v6_bundler_strbuf_append(&out, t.start, t.len);
      prev_end = t.start + t.len;
    }
  }

  return v6_bundler_strbuf_take(&out, out_len);
}

v6_bundler_extension
v6_bundler_define_extension(v6_bundler_define_state* state) {
  v6_bundler_extension ext;
  ext.name = "define";
  ext.state = state;
  ext.resolve = NULL;
  ext.transform = define_transform;
  ext.finalize = NULL;
  ext.emit = NULL;
  return ext;
}
