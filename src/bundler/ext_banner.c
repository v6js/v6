#include "v6/bundler_ext_banner.h"

#include <stdlib.h>
#include <string.h>

static char* banner_finalize(void* state_v, char* output, size_t output_len,
                             size_t* out_len) {
  const char* banner = (const char*)state_v;
  size_t blen = strlen(banner);
  size_t total = blen + 1 + output_len;
  char* buf = malloc(total + 1);
  memcpy(buf, banner, blen);
  buf[blen] = '\n';
  memcpy(buf + blen + 1, output, output_len);
  buf[total] = '\0';
  *out_len = total;
  return buf;
}

v6_bundler_extension v6_bundler_banner_extension(const char* banner_text) {
  v6_bundler_extension ext;
  ext.name = "banner";
  ext.state = (void*)banner_text;
  ext.resolve = NULL;
  ext.transform = NULL;
  ext.finalize = banner_finalize;
  ext.emit = NULL;
  return ext;
}
