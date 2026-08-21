#pragma once

#include <stddef.h>

typedef struct v6_bundler_strbuf {
  char* data;
  size_t len;
  size_t cap;
} v6_bundler_strbuf;

void v6_bundler_strbuf_init(v6_bundler_strbuf* b);
void v6_bundler_strbuf_free(v6_bundler_strbuf* b);
void v6_bundler_strbuf_append(v6_bundler_strbuf* b, const char* s, size_t n);
void v6_bundler_strbuf_append_cstr(v6_bundler_strbuf* b, const char* s);
void v6_bundler_strbuf_append_fmt(v6_bundler_strbuf* b, const char* fmt, ...);
char* v6_bundler_strbuf_take(v6_bundler_strbuf* b, size_t* out_len);
