#include "v6/module.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

void module_ctx_init(module_ctx* mc) {
  mc->count = 0;
}

int path_is_regular_file(const char* path) {
#ifdef _WIN32
  struct _stat st;
  if (_stat(path, &st) != 0)
    return 0;
#else
  struct stat st;
  if (stat(path, &st) != 0)
    return 0;
#endif
  return (st.st_mode & S_IFREG) != 0;
}

static int path_is_directory(const char* path) {
#ifdef _WIN32
  struct _stat st;
  if (_stat(path, &st) != 0)
    return 0;
  return (st.st_mode & _S_IFDIR) != 0;
#else
  struct stat st;
  if (stat(path, &st) != 0)
    return 0;
  return (st.st_mode & S_IFDIR) != 0;
#endif
}

void path_normalize(const char* path, char* out, size_t out_size) {
  char buf[2048];
  size_t n = 0;
  for (const char* p = path; *p && n + 1 < sizeof(buf); p++)
    buf[n++] = (*p == '\\') ? '/' : *p;
  buf[n] = '\0';

  char prefix[4] = "";
  char* rest = buf;
  if (isalpha((unsigned char)buf[0]) && buf[1] == ':') {
    prefix[0] = buf[0];
    prefix[1] = ':';
    prefix[2] = '\0';
    rest = buf + 2;
  }
  int is_abs = rest[0] == '/';

  char* segs[256];
  int seg_count = 0;
  char* tok = strtok(rest, "/");
  while (tok) {
    if (strcmp(tok, ".") == 0) {
    } else if (strcmp(tok, "..") == 0) {
      if (seg_count > 0 && strcmp(segs[seg_count - 1], "..") != 0) {
        seg_count--;
      } else if (!is_abs) {
        if (seg_count < 256)
          segs[seg_count++] = tok;
      }
    } else if (tok[0] != '\0') {
      if (seg_count < 256)
        segs[seg_count++] = tok;
    }
    tok = strtok(NULL, "/");
  }

  out[0] = '\0';
  if (prefix[0])
    strncat(out, prefix, out_size - strlen(out) - 1);
  if (is_abs)
    strncat(out, "/", out_size - strlen(out) - 1);
  for (int i = 0; i < seg_count; i++) {
    if (i > 0)
      strncat(out, "/", out_size - strlen(out) - 1);
    strncat(out, segs[i], out_size - strlen(out) - 1);
  }
  if (out[0] == '\0')
    strncat(out, ".", out_size - 1);
}

void path_dirname(const char* path, char* out, size_t out_size) {
  char norm[2048];
  path_normalize(path, norm, sizeof(norm));
  char* last_slash = strrchr(norm, '/');
  if (!last_slash) {
    snprintf(out, out_size, ".");
    return;
  }
  size_t len = (size_t)(last_slash - norm);
  if (len == 0) {
    snprintf(out, out_size, "/");
    return;
  }
  if (len == 2 && norm[1] == ':') {
    snprintf(out, out_size, "%.*s/", (int)len, norm);
    return;
  }
  snprintf(out, out_size, "%.*s", (int)len, norm);
}

static void path_join(const char* base_dir, const char* rel, char* out,
                      size_t out_size) {
  char combined[2048];
  if (rel[0] == '/' || (isalpha((unsigned char)rel[0]) && rel[1] == ':')) {
    snprintf(combined, sizeof(combined), "%s", rel);
  } else {
    snprintf(combined, sizeof(combined), "%s/%s", base_dir, rel);
  }
  path_normalize(combined, out, out_size);
}

static int read_package_json_field(const char* pkg_path, const char* field,
                                   char* out, size_t out_size) {
  FILE* f = fopen(pkg_path, "rb");
  if (!f)
    return -1;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (n <= 0) {
    fclose(f);
    return -1;
  }
  char* text = malloc((size_t)n + 1);
  fread(text, 1, (size_t)n, f);
  text[n] = '\0';
  fclose(f);

  char needle[128];
  snprintf(needle, sizeof(needle), "\"%s\"", field);
  char* p = strstr(text, needle);
  int found = 0;
  if (p) {
    p += strlen(needle);
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
      p++;
    if (*p == ':') {
      p++;
      while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
      if (*p == '"') {
        p++;
        size_t i = 0;
        while (*p && *p != '"' && i + 1 < out_size) {
          if (*p == '\\' && p[1])
            p++;
          out[i++] = *p++;
        }
        out[i] = '\0';
        found = 1;
      }
    }
  }
  free(text);
  return found ? 0 : -1;
}

static int try_as_file(const char* base, char* out, size_t out_size) {
  if (path_is_regular_file(base)) {
    snprintf(out, out_size, "%s", base);
    return 1;
  }
  char with_ext[v6_max_path];
  snprintf(with_ext, sizeof(with_ext), "%s.js", base);
  if (path_is_regular_file(with_ext)) {
    snprintf(out, out_size, "%s", with_ext);
    return 1;
  }
  return 0;
}

static int try_as_directory(const char* dir, char* out, size_t out_size) {
  char pkg_path[v6_max_path];
  snprintf(pkg_path, sizeof(pkg_path), "%s/package.json", dir);
  char field_val[512];
  if (read_package_json_field(pkg_path, "main", field_val, sizeof(field_val)) ==
          0 ||
      read_package_json_field(pkg_path, "exports", field_val,
                              sizeof(field_val)) == 0) {
    char joined[v6_max_path];
    path_join(dir, field_val, joined, sizeof(joined));
    if (try_as_file(joined, out, out_size))
      return 1;
    char index_joined[v6_max_path];
    snprintf(index_joined, sizeof(index_joined), "%s/index.js", joined);
    if (path_is_regular_file(index_joined)) {
      snprintf(out, out_size, "%s", index_joined);
      return 1;
    }
  }
  char index_path[v6_max_path];
  snprintf(index_path, sizeof(index_path), "%s/index.js", dir);
  if (path_is_regular_file(index_path)) {
    snprintf(out, out_size, "%s", index_path);
    return 1;
  }
  return 0;
}

int resolve_module_specifier(const char* importer_dir, const char* specifier,
                             char* out_path, size_t out_size, char* err,
                             size_t err_size) {
  int is_relative =
      specifier[0] == '.' || specifier[0] == '/' ||
      (isalpha((unsigned char)specifier[0]) && specifier[1] == ':');

  if (is_relative) {
    char joined[v6_max_path];
    path_join(importer_dir, specifier, joined, sizeof(joined));
    if (try_as_file(joined, out_path, out_size))
      return 0;
    if (path_is_directory(joined) &&
        try_as_directory(joined, out_path, out_size))
      return 0;
    snprintf(err, err_size, "Cannot find module '%s'", specifier);
    return -1;
  }

  char dir[v6_max_path];
  snprintf(dir, sizeof(dir), "%s", importer_dir);
  for (;;) {
    char candidate[v6_max_path];
    snprintf(candidate, sizeof(candidate), "%s/node_modules/%s", dir,
             specifier);
    char normalized[v6_max_path];
    path_normalize(candidate, normalized, sizeof(normalized));
    if (try_as_file(normalized, out_path, out_size))
      return 0;
    if (path_is_directory(normalized) &&
        try_as_directory(normalized, out_path, out_size))
      return 0;

    char parent[v6_max_path];
    path_dirname(dir, parent, sizeof(parent));
    if (strcmp(parent, dir) == 0)
      break;
    snprintf(dir, sizeof(dir), "%s", parent);
  }
  snprintf(err, err_size, "Cannot find module '%s'", specifier);
  return -1;
}
