#pragma once

#include <stddef.h>

char* v6_opt_strip_css(const char* src, size_t len, int strip_whitespace,
                       int strip_comments, size_t* out_len);
char* v6_opt_strip_json_whitespace(const char* src, size_t len,
                                   int strip_whitespace, size_t* out_len);
