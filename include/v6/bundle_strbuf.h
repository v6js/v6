#pragma once

#include <stddef.h>

typedef struct bundle_strbuf {
  char* data;
  size_t len;
  size_t cap;
} bundle_strbuf;

void bundle_strbuf_init(bundle_strbuf* b);
void bundle_strbuf_free(bundle_strbuf* b);
void bundle_strbuf_append(bundle_strbuf* b, const char* s, size_t n);
void bundle_strbuf_append_cstr(bundle_strbuf* b, const char* s);
void bundle_strbuf_append_fmt(bundle_strbuf* b, const char* fmt, ...);
char* bundle_strbuf_take(bundle_strbuf* b, size_t* out_len);
