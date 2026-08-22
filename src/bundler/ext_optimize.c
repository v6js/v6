#include "v6/bundler_ext_optimize.h"
#include "v6/optimizer.h"

#include <stdlib.h>
#include <string.h>

static int has_suffix(const char* s, const char* suffix) {
  size_t ls = strlen(s), lsuf = strlen(suffix);
  if (lsuf > ls)
    return 0;
  return strcmp(s + ls - lsuf, suffix) == 0;
}

static char* optimize_transform(void* state_v, const char* path, char* source,
                                size_t source_len, size_t* out_len) {
  const v6_optimizer_options* opts = (const v6_optimizer_options*)state_v;
  if (!v6_optimizer_any_css_pass_enabled(opts)) {
    *out_len = source_len;
    return source;
  }
  if (has_suffix(path, ".css"))
    return v6_optimizer_run_css(source, source_len, opts, out_len);
  if (has_suffix(path, ".json"))
    return v6_optimizer_run_json(source, source_len, opts, out_len);
  *out_len = source_len;
  return source;
}

static int is_export_line(const char* s, size_t len) {
  size_t i = 0;
  while (i < len && (s[i] == ' ' || s[i] == '\t'))
    i++;
  return len - i >= 7 && memcmp(s + i, "export ", 7) == 0;
}

static size_t find_export_tail_start(const char* s, size_t len) {
  size_t end = len;
  while (end > 0 && (s[end - 1] == '\n' || s[end - 1] == '\r'))
    end--;
  for (;;) {
    if (end == 0)
      return 0;
    size_t line_start = end;
    while (line_start > 0 && s[line_start - 1] != '\n')
      line_start--;
    if (!is_export_line(s + line_start, end - line_start)) {
      size_t split = end;
      while (split < len && (s[split] == '\n' || s[split] == '\r'))
        split++;
      return split;
    }
    end = line_start;
    while (end > 0 && (s[end - 1] == '\n' || s[end - 1] == '\r'))
      end--;
  }
}

static char* optimize_finalize(void* state_v, char* output, size_t output_len,
                               size_t* out_len) {
  const v6_optimizer_options* opts = (const v6_optimizer_options*)state_v;
  if (!v6_optimizer_any_js_pass_enabled(opts)) {
    *out_len = output_len;
    return output;
  }
  size_t split = find_export_tail_start(output, output_len);
  size_t tail_len = output_len - split;

  char* body_src = malloc(split + 1);
  memcpy(body_src, output, split);
  body_src[split] = '\0';

  char err_buf[256];
  int err_line = 0;
  size_t body_out_len = 0;
  char* body_result = v6_optimizer_run_js(body_src, split, opts, &body_out_len,
                                          err_buf, sizeof(err_buf), &err_line);
  free(body_src);
  if (!body_result) {
    *out_len = output_len;
    return output;
  }
  if (tail_len == 0) {
    *out_len = body_out_len;
    return body_result;
  }
  char* combined = malloc(body_out_len + tail_len + 1);
  memcpy(combined, body_result, body_out_len);
  memcpy(combined + body_out_len, output + split, tail_len);
  combined[body_out_len + tail_len] = '\0';
  free(body_result);
  *out_len = body_out_len + tail_len;
  return combined;
}

v6_bundler_extension
v6_bundler_optimize_extension(const v6_optimizer_options* opts) {
  v6_bundler_extension ext;
  ext.name = "optimize";
  ext.state = (void*)opts;
  ext.resolve = NULL;
  ext.transform = optimize_transform;
  ext.finalize = optimize_finalize;
  ext.emit = NULL;
  return ext;
}
