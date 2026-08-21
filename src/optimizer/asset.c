#include "v6/optimizer_asset.h"
#include "v6/optimizer_buf.h"

#include <stdlib.h>
#include <string.h>

static char* dup_range(const char* s, size_t n) {
  char* out = malloc(n + 1);
  memcpy(out, s, n);
  out[n] = '\0';
  return out;
}

static int is_css_ws(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

static int is_safe_boundary(char c) {
  return c == '{' || c == '}' || c == ':' || c == ';' || c == ',' || c == '(' ||
         c == ')';
}

char* v6_opt_strip_css(const char* src, size_t len, int strip_whitespace,
                       int strip_comments, size_t* out_len) {
  if (!strip_whitespace && !strip_comments) {
    *out_len = len;
    return dup_range(src, len);
  }

  v6_opt_buf out;
  v6_opt_buf_init(&out);

  size_t i = 0;
  while (i < len) {
    char c = src[i];

    if (c == '/' && i + 1 < len && src[i + 1] == '*') {
      size_t comment_start = i;
      i += 2;
      while (i + 1 < len && !(src[i] == '*' && src[i + 1] == '/'))
        i++;
      i = (i + 1 < len) ? i + 2 : len;
      if (!strip_comments)
        v6_opt_buf_append(&out, src + comment_start, i - comment_start);
      continue;
    }

    if (c == '"' || c == '\'') {
      char quote = c;
      v6_opt_buf_append_char(&out, c);
      i++;
      while (i < len && src[i] != quote) {
        if (src[i] == '\\' && i + 1 < len) {
          v6_opt_buf_append_char(&out, src[i]);
          i++;
        }
        v6_opt_buf_append_char(&out, src[i]);
        i++;
      }
      if (i < len) {
        v6_opt_buf_append_char(&out, src[i]);
        i++;
      }
      continue;
    }

    if (strip_whitespace && is_css_ws(c)) {
      size_t j = i;
      while (j < len && is_css_ws(src[j]))
        j++;
      char prev = out.len > 0 ? out.data[out.len - 1] : '\0';
      char next = j < len ? src[j] : '\0';
      if (!(is_safe_boundary(prev) || is_safe_boundary(next) || prev == '\0' ||
            next == '\0'))
        v6_opt_buf_append_char(&out, ' ');
      i = j;
      continue;
    }

    v6_opt_buf_append_char(&out, c);
    i++;
  }

  if (!strip_whitespace)
    return v6_opt_buf_take(&out, out_len);

  size_t start = 0, end = out.len;
  while (start < end && is_css_ws(out.data[start]))
    start++;
  while (end > start && is_css_ws(out.data[end - 1]))
    end--;
  if (start > 0 || end < out.len) {
    size_t new_len = end - start;
    char* trimmed = dup_range(out.data + start, new_len);
    v6_opt_buf_free(&out);
    *out_len = new_len;
    return trimmed;
  }

  return v6_opt_buf_take(&out, out_len);
}

char* v6_opt_strip_json_whitespace(const char* src, size_t len,
                                   int strip_whitespace, size_t* out_len) {
  if (!strip_whitespace) {
    *out_len = len;
    return dup_range(src, len);
  }

  v6_opt_buf out;
  v6_opt_buf_init(&out);

  size_t i = 0;
  while (i < len) {
    char c = src[i];
    if (c == '"') {
      v6_opt_buf_append_char(&out, c);
      i++;
      while (i < len && src[i] != '"') {
        if (src[i] == '\\' && i + 1 < len) {
          v6_opt_buf_append_char(&out, src[i]);
          i++;
        }
        v6_opt_buf_append_char(&out, src[i]);
        i++;
      }
      if (i < len) {
        v6_opt_buf_append_char(&out, src[i]);
        i++;
      }
      continue;
    }
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      i++;
      continue;
    }
    v6_opt_buf_append_char(&out, c);
    i++;
  }

  return v6_opt_buf_take(&out, out_len);
}
