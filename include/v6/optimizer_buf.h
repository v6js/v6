#pragma once

#include <stddef.h>

typedef struct v6_opt_buf {
  char* data;
  size_t len;
  size_t cap;
} v6_opt_buf;

void v6_opt_buf_init(v6_opt_buf* b);
void v6_opt_buf_free(v6_opt_buf* b);
void v6_opt_buf_append(v6_opt_buf* b, const char* s, size_t n);
void v6_opt_buf_append_cstr(v6_opt_buf* b, const char* s);
void v6_opt_buf_append_char(v6_opt_buf* b, char c);
void v6_opt_buf_append_fmt(v6_opt_buf* b, const char* fmt, ...);
char* v6_opt_buf_take(v6_opt_buf* b, size_t* out_len);
