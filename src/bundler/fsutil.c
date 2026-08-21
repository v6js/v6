#include "v6/bundle_fsutil.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define bundle_mkdir_raw(p) _mkdir(p)
#else
#include <sys/stat.h>
#define bundle_mkdir_raw(p) mkdir(p, 0755)
#endif

int bundle_mkdir_p(const char* path) {
  char buf[1024];
  size_t len = strlen(path);
  if (len >= sizeof(buf))
    return -1;
  memcpy(buf, path, len + 1);

  for (size_t i = 1; i < len; i++) {
    if (buf[i] == '/' || buf[i] == '\\') {
      char save = buf[i];
      buf[i] = '\0';
      bundle_mkdir_raw(buf);
      buf[i] = save;
    }
  }
  bundle_mkdir_raw(buf);
  return 0;
}

int bundle_write_file(const char* path, const char* data, size_t len) {
  FILE* f = fopen(path, "wb");
  if (!f)
    return -1;
  size_t written = fwrite(data, 1, len, f);
  fclose(f);
  return written == len ? 0 : -1;
}

int bundle_copy_file(const char* src_path, const char* dst_path) {
  FILE* in = fopen(src_path, "rb");
  if (!in)
    return -1;
  FILE* out = fopen(dst_path, "wb");
  if (!out) {
    fclose(in);
    return -1;
  }
  char buf[65536];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
    if (fwrite(buf, 1, n, out) != n) {
      fclose(in);
      fclose(out);
      return -1;
    }
  }
  fclose(in);
  fclose(out);
  return 0;
}
