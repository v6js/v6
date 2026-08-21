#include "v6/bundler_strbuf.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void v6_bundler_strbuf_init(v6_bundler_strbuf* b) {
  b->cap = 4096;
  b->len = 0;
  b->data = malloc(b->cap);
  b->data[0] = '\0';
}

void v6_bundler_strbuf_free(v6_bundler_strbuf* b) {
  free(b->data);
  b->data = NULL;
  b->len = 0;
  b->cap = 0;
}

static void ensure_cap(v6_bundler_strbuf* b, size_t extra) {
  if (b->len + extra + 1 <= b->cap)
    return;
  size_t new_cap = b->cap * 2;
  while (new_cap < b->len + extra + 1)
    new_cap *= 2;
  b->data = realloc(b->data, new_cap);
  b->cap = new_cap;
}

void v6_bundler_strbuf_append(v6_bundler_strbuf* b, const char* s, size_t n) {
  ensure_cap(b, n);
  memcpy(b->data + b->len, s, n);
  b->len += n;
  b->data[b->len] = '\0';
}

void v6_bundler_strbuf_append_cstr(v6_bundler_strbuf* b, const char* s) {
  v6_bundler_strbuf_append(b, s, strlen(s));
}

void v6_bundler_strbuf_append_fmt(v6_bundler_strbuf* b, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  va_list ap2;
  va_copy(ap2, ap);
  int need = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);
  if (need < 0) {
    va_end(ap2);
    return;
  }
  ensure_cap(b, (size_t)need);
  vsnprintf(b->data + b->len, (size_t)need + 1, fmt, ap2);
  va_end(ap2);
  b->len += (size_t)need;
}

char* v6_bundler_strbuf_take(v6_bundler_strbuf* b, size_t* out_len) {
  char* out = b->data;
  if (out_len)
    *out_len = b->len;
  b->data = NULL;
  b->len = 0;
  b->cap = 0;
  return out;
}
