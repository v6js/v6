#include "v6/cache.h"

#include "v6/daemon.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#define v6_cache_magic 0x36434356u
#define v6_cache_version 1u

static unsigned long long v6_cache_hash(const char* s) {
  unsigned long long h = 1469598103934665603ULL;
  for (; *s; s++) {
    h ^= (unsigned char)*s;
    h *= 1099511628211ULL;
  }
  return h;
}

static int v6_cache_temp_dir(char* out, size_t out_size) {
#ifdef _WIN32
  DWORD n = GetTempPathA((DWORD)out_size, out);
  if (n == 0 || n >= out_size)
    return -1;
  if (n > 0 && out[n - 1] == '\\')
    out[n - 1] = '\0';
  return 0;
#else
  const char* t = getenv("TMPDIR");
  if (!t)
    t = "/tmp";
  snprintf(out, out_size, "%s", t);
  return 0;
#endif
}

static int v6_cache_abs_path(const char* path, char* out, size_t out_size) {
#ifdef _WIN32
  DWORD n = GetFullPathNameA(path, (DWORD)out_size, out, NULL);
  if (n == 0 || n >= out_size)
    return -1;
  return 0;
#else
  if (path[0] == '/') {
    snprintf(out, out_size, "%s", path);
    return 0;
  }
  char cwd[1024];
  if (!getcwd(cwd, sizeof(cwd)))
    return -1;
  snprintf(out, out_size, "%s/%s", cwd, path);
  return 0;
#endif
}

static int v6_cache_path_for(const char* entry_path, char* out,
                             size_t out_size) {
  char abs[1200];
  if (v6_cache_abs_path(entry_path, abs, sizeof(abs)) != 0)
    return -1;
  char tmp[1024];
  if (v6_cache_temp_dir(tmp, sizeof(tmp)) != 0)
    return -1;
  unsigned long long h = v6_cache_hash(abs);
#ifdef _WIN32
  snprintf(out, out_size, "%s\\v6-compile-cache", tmp);
  CreateDirectoryA(out, NULL);
#else
  snprintf(out, out_size, "%s/v6-compile-cache", tmp);
  mkdir(out, 0755);
#endif
  size_t base_len = strlen(out);
  snprintf(out + base_len, out_size - base_len, "/%016llx.v6c", h);
  return 0;
}

static int v6_write_u32(FILE* f, uint32_t v) {
  return fwrite(&v, sizeof(v), 1, f) == 1 ? 0 : -1;
}

static int v6_write_i64(FILE* f, int64_t v) {
  return fwrite(&v, sizeof(v), 1, f) == 1 ? 0 : -1;
}

static int v6_write_str(FILE* f, const char* s) {
  uint32_t len = (uint32_t)strlen(s);
  if (v6_write_u32(f, len) != 0)
    return -1;
  return fwrite(s, 1, len, f) == len ? 0 : -1;
}

static int v6_write_bytes(FILE* f, const unsigned char* data, size_t len) {
  if (v6_write_u32(f, (uint32_t)len) != 0)
    return -1;
  if (len == 0)
    return 0;
  return fwrite(data, 1, len, f) == len ? 0 : -1;
}

static int v6_read_u32(FILE* f, uint32_t* v) {
  return fread(v, sizeof(*v), 1, f) == 1 ? 0 : -1;
}

static int v6_read_i64(FILE* f, int64_t* v) {
  return fread(v, sizeof(*v), 1, f) == 1 ? 0 : -1;
}

static char* v6_read_str(FILE* f) {
  uint32_t len;
  if (v6_read_u32(f, &len) != 0)
    return NULL;
  char* s = malloc((size_t)len + 1);
  if (len > 0 && fread(s, 1, len, f) != len) {
    free(s);
    return NULL;
  }
  s[len] = '\0';
  return s;
}

static unsigned char* v6_read_bytes(FILE* f, size_t* out_len) {
  uint32_t len;
  if (v6_read_u32(f, &len) != 0)
    return NULL;
  unsigned char* d = malloc(len > 0 ? len : 1);
  if (len > 0 && fread(d, 1, len, f) != len) {
    free(d);
    return NULL;
  }
  *out_len = len;
  return d;
}

int v6_cache_try_load(const char* entry_path, v6_cache_result* out) {
  out->entries = NULL;
  out->count = 0;

  char cache_path[1300];
  if (v6_cache_path_for(entry_path, cache_path, sizeof(cache_path)) != 0)
    return -1;

  FILE* f = fopen(cache_path, "rb");
  if (!f)
    return -1;

  int ok = 0;
  char* stored_entry_path = NULL;
  char** tracked_names = NULL;
  int tracked_count = 0;
  v6_cache_entry* entries = NULL;
  int entry_count = 0;

  uint32_t magic, version;
  if (v6_read_u32(f, &magic) != 0 || magic != v6_cache_magic)
    goto done;
  if (v6_read_u32(f, &version) != 0 || version != v6_cache_version)
    goto done;

  stored_entry_path = v6_read_str(f);
  if (!stored_entry_path)
    goto done;
  char abs_entry[1200];
  if (v6_cache_abs_path(entry_path, abs_entry, sizeof(abs_entry)) != 0)
    goto done;
  if (strcmp(stored_entry_path, abs_entry) != 0)
    goto done;

  uint32_t tc;
  if (v6_read_u32(f, &tc) != 0)
    goto done;
  tracked_count = (int)tc;
  tracked_names =
      malloc(sizeof(char*) * (tracked_count > 0 ? tracked_count : 1));
  for (int i = 0; i < tracked_count; i++)
    tracked_names[i] = NULL;

  for (int i = 0; i < tracked_count; i++) {
    char* path = v6_read_str(f);
    int64_t stored_mtime, stored_size;
    if (!path || v6_read_i64(f, &stored_mtime) != 0 ||
        v6_read_i64(f, &stored_size) != 0) {
      free(path);
      goto done;
    }
    tracked_names[i] = path;
    long long cur_mtime, cur_size;
    if (v6_stat_file(path, &cur_mtime, &cur_size) != 0 ||
        cur_mtime != stored_mtime || cur_size != stored_size)
      goto done;
  }

  uint32_t ec;
  if (v6_read_u32(f, &ec) != 0)
    goto done;
  entry_count = (int)ec;
  entries =
      malloc(sizeof(v6_cache_entry) * (entry_count > 0 ? entry_count : 1));
  for (int i = 0; i < entry_count; i++) {
    entries[i].name = NULL;
    entries[i].data = NULL;
    entries[i].len = 0;
  }
  for (int i = 0; i < entry_count; i++) {
    char* name = v6_read_str(f);
    if (!name)
      goto done;
    size_t len;
    unsigned char* data = v6_read_bytes(f, &len);
    if (!data) {
      free(name);
      goto done;
    }
    entries[i].name = name;
    entries[i].data = data;
    entries[i].len = len;
  }

  ok = 1;

done:
  fclose(f);
  free(stored_entry_path);
  if (tracked_names) {
    for (int i = 0; i < tracked_count; i++)
      free(tracked_names[i]);
    free(tracked_names);
  }
  if (ok) {
    out->entries = entries;
    out->count = entry_count;
    return 0;
  }
  if (entries) {
    for (int i = 0; i < entry_count; i++) {
      free(entries[i].name);
      free(entries[i].data);
    }
    free(entries);
  }
  return -1;
}

void v6_cache_free_result(v6_cache_result* r) {
  for (int i = 0; i < r->count; i++) {
    free(r->entries[i].name);
    free(r->entries[i].data);
  }
  free(r->entries);
  r->entries = NULL;
  r->count = 0;
}

void v6_cache_store(const char* entry_path, const char** tracked_paths,
                    int tracked_count, const char** entry_names,
                    const unsigned char** entry_datas, const size_t* entry_lens,
                    int entry_count) {
  char cache_path[1300];
  if (v6_cache_path_for(entry_path, cache_path, sizeof(cache_path)) != 0)
    return;
  char tmp_path[1320];
  snprintf(tmp_path, sizeof(tmp_path), "%s.tmp%lu", cache_path,
           (unsigned long)v6_cache_hash(entry_path));

  FILE* f = fopen(tmp_path, "wb");
  if (!f)
    return;

  int ok = 1;
  ok &= v6_write_u32(f, v6_cache_magic) == 0;
  ok &= v6_write_u32(f, v6_cache_version) == 0;

  char abs_entry[1200];
  if (v6_cache_abs_path(entry_path, abs_entry, sizeof(abs_entry)) != 0) {
    fclose(f);
    remove(tmp_path);
    return;
  }
  ok &= v6_write_str(f, abs_entry) == 0;

  ok &= v6_write_u32(f, (uint32_t)tracked_count) == 0;
  for (int i = 0; i < tracked_count && ok; i++) {
    long long mtime, size;
    if (v6_stat_file(tracked_paths[i], &mtime, &size) != 0) {
      ok = 0;
      break;
    }
    ok &= v6_write_str(f, tracked_paths[i]) == 0;
    ok &= v6_write_i64(f, (int64_t)mtime) == 0;
    ok &= v6_write_i64(f, (int64_t)size) == 0;
  }

  ok &= v6_write_u32(f, (uint32_t)entry_count) == 0;
  for (int i = 0; i < entry_count && ok; i++) {
    ok &= v6_write_str(f, entry_names[i]) == 0;
    ok &= v6_write_bytes(f, entry_datas[i], entry_lens[i]) == 0;
  }

  fclose(f);

  if (!ok) {
    remove(tmp_path);
    return;
  }

#ifdef _WIN32
  MoveFileExA(tmp_path, cache_path, MOVEFILE_REPLACE_EXISTING);
#else
  rename(tmp_path, cache_path);
#endif
}
