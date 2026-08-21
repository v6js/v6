#include "v6/bundler_fsutil.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define v6_bundler_mkdir_raw(p) _mkdir(p)
#else
#include <dirent.h>
#include <sys/stat.h>
#define v6_bundler_mkdir_raw(p) mkdir(p, 0755)
#endif

int v6_bundler_mkdir_p(const char* path) {
  char buf[1024];
  size_t len = strlen(path);
  if (len >= sizeof(buf))
    return -1;
  memcpy(buf, path, len + 1);

  for (size_t i = 1; i < len; i++) {
    if (buf[i] == '/' || buf[i] == '\\') {
      char save = buf[i];
      buf[i] = '\0';
      v6_bundler_mkdir_raw(buf);
      buf[i] = save;
    }
  }
  v6_bundler_mkdir_raw(buf);
  return 0;
}

int v6_bundler_write_file(const char* path, const char* data, size_t len) {
  FILE* f = fopen(path, "wb");
  if (!f)
    return -1;
  size_t written = fwrite(data, 1, len, f);
  fclose(f);
  return written == len ? 0 : -1;
}

int v6_bundler_copy_file(const char* src_path, const char* dst_path) {
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

#ifdef _WIN32
int v6_bundler_copy_dir_recursive(const char* src_dir, const char* dst_dir) {
  v6_bundler_mkdir_p(dst_dir);

  char pattern[1024];
  snprintf(pattern, sizeof(pattern), "%s\\*", src_dir);

  WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA(pattern, &fd);
  if (h == INVALID_HANDLE_VALUE)
    return -1;

  int rc = 0;
  do {
    if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
      continue;

    char src_path[1024];
    char dst_path[1024];
    snprintf(src_path, sizeof(src_path), "%s/%s", src_dir, fd.cFileName);
    snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_dir, fd.cFileName);

    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      if (v6_bundler_copy_dir_recursive(src_path, dst_path) != 0)
        rc = -1;
    } else {
      if (v6_bundler_copy_file(src_path, dst_path) != 0)
        rc = -1;
    }
  } while (FindNextFileA(h, &fd));

  FindClose(h);
  return rc;
}
#else
int v6_bundler_copy_dir_recursive(const char* src_dir, const char* dst_dir) {
  v6_bundler_mkdir_p(dst_dir);

  DIR* d = opendir(src_dir);
  if (!d)
    return -1;

  int rc = 0;
  struct dirent* ent;
  while ((ent = readdir(d)) != NULL) {
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
      continue;

    char src_path[1024];
    char dst_path[1024];
    snprintf(src_path, sizeof(src_path), "%s/%s", src_dir, ent->d_name);
    snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_dir, ent->d_name);

    struct stat st;
    if (stat(src_path, &st) != 0) {
      rc = -1;
      continue;
    }

    if (S_ISDIR(st.st_mode)) {
      if (v6_bundler_copy_dir_recursive(src_path, dst_path) != 0)
        rc = -1;
    } else {
      if (v6_bundler_copy_file(src_path, dst_path) != 0)
        rc = -1;
    }
  }

  closedir(d);
  return rc;
}
#endif
