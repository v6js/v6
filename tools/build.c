#ifndef _WIN32
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#endif

#include "../include/v6/version.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#else
#include <dirent.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

typedef struct {
  char** items;
  int count;
  int cap;
} strlist;

static char* dup_str(const char* s) {
#ifdef _WIN32
  return _strdup(s);
#else
  return strdup(s);
#endif
}

static void strlist_init(strlist* l) {
  l->items = NULL;
  l->count = 0;
  l->cap = 0;
}

static void strlist_push(strlist* l, const char* s) {
  if (l->count == l->cap) {
    l->cap = l->cap ? l->cap * 2 : 16;
    l->items = realloc(l->items, sizeof(char*) * (size_t)l->cap);
  }
  l->items[l->count++] = dup_str(s);
}

static void strlist_free(strlist* l) {
  for (int i = 0; i < l->count; i++)
    free(l->items[i]);
  free(l->items);
  l->items = NULL;
  l->count = 0;
  l->cap = 0;
}

typedef struct {
  uint32_t state[8];
  uint64_t bitlen;
  unsigned char buf[64];
  size_t buflen;
} sha256_ctx;

static const uint32_t SHA256_K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

static uint32_t rotr(uint32_t x, uint32_t n) {
  return (x >> n) | (x << (32 - n));
}

static void sha256_init(sha256_ctx* c) {
  static const uint32_t iv[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372,
                                 0xa54ff53a, 0x510e527f, 0x9b05688c,
                                 0x1f83d9ab, 0x5be0cd19};
  memcpy(c->state, iv, sizeof(iv));
  c->bitlen = 0;
  c->buflen = 0;
}

static void sha256_transform(sha256_ctx* c, const unsigned char* data) {
  uint32_t w[64];
  for (int i = 0; i < 16; i++)
    w[i] = ((uint32_t)data[i * 4] << 24) | ((uint32_t)data[i * 4 + 1] << 16) |
           ((uint32_t)data[i * 4 + 2] << 8) | ((uint32_t)data[i * 4 + 3]);
  for (int i = 16; i < 64; i++) {
    uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
    uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }
  uint32_t a = c->state[0], b = c->state[1], cc = c->state[2], d = c->state[3];
  uint32_t e = c->state[4], f = c->state[5], g = c->state[6], h = c->state[7];
  for (int i = 0; i < 64; i++) {
    uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
    uint32_t ch = (e & f) ^ ((~e) & g);
    uint32_t temp1 = h + S1 + ch + SHA256_K[i] + w[i];
    uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
    uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
    uint32_t temp2 = S0 + maj;
    h = g;
    g = f;
    f = e;
    e = d + temp1;
    d = cc;
    cc = b;
    b = a;
    a = temp1 + temp2;
  }
  c->state[0] += a;
  c->state[1] += b;
  c->state[2] += cc;
  c->state[3] += d;
  c->state[4] += e;
  c->state[5] += f;
  c->state[6] += g;
  c->state[7] += h;
}

static void sha256_update(sha256_ctx* c, const unsigned char* data,
                          size_t len) {
  c->bitlen += (uint64_t)len * 8;
  while (len > 0) {
    size_t take = 64 - c->buflen;
    if (take > len)
      take = len;
    memcpy(c->buf + c->buflen, data, take);
    c->buflen += take;
    data += take;
    len -= take;
    if (c->buflen == 64) {
      sha256_transform(c, c->buf);
      c->buflen = 0;
    }
  }
}

static void sha256_final(sha256_ctx* c, unsigned char out[32]) {
  uint64_t bitlen = c->bitlen;
  size_t i = c->buflen;
  c->buf[i++] = 0x80;
  if (i > 56) {
    while (i < 64)
      c->buf[i++] = 0;
    sha256_transform(c, c->buf);
    i = 0;
  }
  while (i < 56)
    c->buf[i++] = 0;
  for (int j = 7; j >= 0; j--)
    c->buf[i++] = (unsigned char)(bitlen >> (j * 8));
  sha256_transform(c, c->buf);
  for (int j = 0; j < 8; j++) {
    out[j * 4 + 0] = (unsigned char)(c->state[j] >> 24);
    out[j * 4 + 1] = (unsigned char)(c->state[j] >> 16);
    out[j * 4 + 2] = (unsigned char)(c->state[j] >> 8);
    out[j * 4 + 3] = (unsigned char)(c->state[j]);
  }
}

static int sha256_file(const char* path, char out_hex[65]) {
  FILE* f = fopen(path, "rb");
  if (!f)
    return -1;
  sha256_ctx ctx;
  sha256_init(&ctx);
  unsigned char buf[65536];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
    sha256_update(&ctx, buf, n);
  fclose(f);
  unsigned char digest[32];
  sha256_final(&ctx, digest);
  static const char* hexch = "0123456789abcdef";
  for (int i = 0; i < 32; i++) {
    out_hex[i * 2] = hexch[(digest[i] >> 4) & 0xF];
    out_hex[i * 2 + 1] = hexch[digest[i] & 0xF];
  }
  out_hex[64] = '\0';
  return 0;
}

typedef struct {
  const char* zig_target;
  const char* jni_dir;
  const char* exe_suffix;
  int is_windows;
  int needs_ldl;
  const char* jdk_url;
  const char* jdk_sha256;
  const char* jdk_archive_ext;
  const char* jdk_root_suffix;
} target_spec;

static const target_spec TARGETS[] = {
    {
        "x86_64-linux-gnu",
        "unix",
        "",
        0,
        1,
        "https://github.com/adoptium/temurin21-binaries/releases/download/"
        "jdk-21.0.12%2B8/OpenJDK21U-jdk_x64_linux_hotspot_21.0.12_8.tar.gz",
        "e4446ff06a276155697597cc0f1b15da004ff083f4964a35271ecee567177370",
        "tar.gz",
        "",
    },
    {
        "aarch64-linux-gnu",
        "unix",
        "",
        0,
        1,
        "https://github.com/adoptium/temurin21-binaries/releases/download/"
        "jdk-21.0.12%2B8/"
        "OpenJDK21U-jdk_aarch64_linux_hotspot_21.0.12_8.tar.gz",
        "eba38e871b02d407897bfe017ea35352dfc1420ef6d2112425b0c67325ca509d",
        "tar.gz",
        "",
    },
    {
        "x86_64-windows-gnu",
        "win32",
        ".exe",
        1,
        0,
        "https://github.com/adoptium/temurin21-binaries/releases/download/"
        "jdk-21.0.12%2B8/OpenJDK21U-jdk_x64_windows_hotspot_21.0.12_8.zip",
        "9ba963ee2371874a74185d18bc7bb2ab9407df7683300855ed7606e0662321d0",
        "zip",
        "",
    },
    {
        "x86_64-macos",
        "unix",
        "",
        0,
        0,
        "https://github.com/adoptium/temurin21-binaries/releases/download/"
        "jdk-21.0.12%2B8/OpenJDK21U-jdk_x64_mac_hotspot_21.0.12_8.tar.gz",
        "6b85c260eea574a995eacd0b3ee23c8042aa93b23a08e6478edafca0a0333d7f",
        "tar.gz",
        "/Contents/Home",
    },
    {
        "aarch64-macos",
        "unix",
        "",
        0,
        0,
        "https://github.com/adoptium/temurin21-binaries/releases/download/"
        "jdk-21.0.12%2B8/"
        "OpenJDK21U-jdk_aarch64_mac_hotspot_21.0.12_8.tar.gz",
        "021d629349ebc12a409faa517b837ec80ceee8f58a5ac85c788ecad07ca6881c",
        "tar.gz",
        "/Contents/Home",
    },
};

static const int TARGET_COUNT = (int)(sizeof(TARGETS) / sizeof(TARGETS[0]));

static int run_cmd(const char* fmt, ...) {
  char buf[16384];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  return system(buf);
}

static int run_cmd_capture(char* out, size_t out_size, const char* fmt, ...) {
  char buf[16384];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

#ifdef _WIN32
  FILE* p = _popen(buf, "r");
#else
  FILE* p = popen(buf, "r");
#endif
  if (!p)
    return -1;
  size_t n = fread(out, 1, out_size - 1, p);
  out[n] = '\0';
#ifdef _WIN32
  int rc = _pclose(p);
#else
  int rc = pclose(p);
#endif
  while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r'))
    out[--n] = '\0';
  return rc;
}

static int get_cpu_count(void) {
#ifdef _WIN32
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  int n = (int)si.dwNumberOfProcessors;
#else
  int n = (int)sysconf(_SC_NPROCESSORS_ONLN);
#endif
  return n > 0 ? n : 4;
}

typedef struct {
#ifdef _WIN32
  intptr_t handle;
#else
  pid_t pid;
#endif
} proc_handle;

static int spawn_async(char* const argv[], proc_handle* out) {
#ifdef _WIN32
  out->handle = _spawnvp(_P_NOWAIT, argv[0], (const char* const*)argv);
  return out->handle == -1 ? -1 : 0;
#else
  int rc = posix_spawnp(&out->pid, argv[0], NULL, NULL, argv, environ);
  return rc == 0 ? 0 : -1;
#endif
}

static int wait_proc(proc_handle* h) {
#ifdef _WIN32
  int status = -1;
  if (_cwait(&status, h->handle, 0) == -1)
    return -1;
  return status;
#else
  int status = -1;
  if (waitpid(h->pid, &status, 0) == -1)
    return -1;
  if (WIFEXITED(status))
    return WEXITSTATUS(status);
  return -1;
#endif
}

#ifdef _WIN32
static void to_win_path(const char* path, char* out, size_t out_size) {
  size_t i = 0;
  for (; path[i] && i < out_size - 1; i++)
    out[i] = path[i] == '/' ? '\\' : path[i];
  out[i] = '\0';
}
#endif

static void ensure_dir(const char* path) {
#ifdef _WIN32
  char wpath[1024];
  to_win_path(path, wpath, sizeof(wpath));
  run_cmd("if not exist \"%s\" mkdir \"%s\" >nul 2>nul", wpath, wpath);
#else
  run_cmd("mkdir -p \"%s\"", path);
#endif
}

static void remove_dir(const char* path) {
#ifdef _WIN32
  char wpath[1024];
  to_win_path(path, wpath, sizeof(wpath));
  run_cmd("if exist \"%s\" rmdir /S /Q \"%s\" >nul 2>nul", wpath, wpath);
#else
  run_cmd("rm -rf \"%s\"", path);
#endif
}

static void copy_file(const char* src, const char* dst) {
#ifdef _WIN32
  char wsrc[1024], wdst[1024];
  to_win_path(src, wsrc, sizeof(wsrc));
  to_win_path(dst, wdst, sizeof(wdst));
  run_cmd("copy /Y \"%s\" \"%s\" >nul", wsrc, wdst);
#else
  run_cmd("cp \"%s\" \"%s\"", src, dst);
#endif
}

static int copy_tree_contents(const char* srcdir, const char* dstdir) {
#ifdef _WIN32
  return run_cmd("powershell -NoProfile -Command \"Copy-Item -Path "
                 "'%s\\*' -Destination '%s' -Recurse -Force\"",
                 srcdir, dstdir);
#else
  return run_cmd("cp -R \"%s\"/. \"%s\"/", srcdir, dstdir);
#endif
}

static int path_exists(const char* path) {
#ifdef _WIN32
  DWORD attrs = GetFileAttributesA(path);
  return attrs != INVALID_FILE_ATTRIBUTES;
#else
  return access(path, F_OK) == 0;
#endif
}

static void to_abs_path(const char* path, char* out, size_t out_size) {
#ifdef _WIN32
  GetFullPathNameA(path, (DWORD)out_size, out, NULL);
#else
  if (path[0] == '/') {
    snprintf(out, out_size, "%s", path);
    return;
  }
  char cwd[1024];
  if (!getcwd(cwd, sizeof(cwd)))
    cwd[0] = '\0';
  snprintf(out, out_size, "%s/%s", cwd, path);
#endif
}

static void list_files(const char* dir, const char* ext, strlist* out) {
#ifdef _WIN32
  char pattern[1024];
  snprintf(pattern, sizeof(pattern), "%s\\*%s", dir, ext);
  WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA(pattern, &fd);
  if (h == INVALID_HANDLE_VALUE)
    return;
  do {
    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
      char full[1024];
      snprintf(full, sizeof(full), "%s/%s", dir, fd.cFileName);
      strlist_push(out, full);
    }
  } while (FindNextFileA(h, &fd));
  FindClose(h);
#else
  DIR* d = opendir(dir);
  if (!d)
    return;
  size_t ext_len = strlen(ext);
  struct dirent* ent;
  while ((ent = readdir(d)) != NULL) {
    size_t name_len = strlen(ent->d_name);
    if (name_len < ext_len)
      continue;
    if (strcmp(ent->d_name + (name_len - ext_len), ext) != 0)
      continue;
    char full[1024];
    snprintf(full, sizeof(full), "%s/%s", dir, ent->d_name);
    struct stat st;
    if (stat(full, &st) == 0 && S_ISREG(st.st_mode))
      strlist_push(out, full);
  }
  closedir(d);
#endif
}

static int find_single_subdir(const char* parent, char* out, size_t out_size) {
#ifdef _WIN32
  char pattern[1024];
  snprintf(pattern, sizeof(pattern), "%s\\*", parent);
  WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA(pattern, &fd);
  if (h == INVALID_HANDLE_VALUE)
    return -1;
  int found = 0;
  do {
    if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
        strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0) {
      snprintf(out, out_size, "%s/%s", parent, fd.cFileName);
      found = 1;
      break;
    }
  } while (FindNextFileA(h, &fd));
  FindClose(h);
  return found ? 0 : -1;
#else
  DIR* d = opendir(parent);
  if (!d)
    return -1;
  struct dirent* ent;
  int found = 0;
  while ((ent = readdir(d)) != NULL) {
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
      continue;
    char full[1024];
    snprintf(full, sizeof(full), "%s/%s", parent, ent->d_name);
    struct stat st;
    if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) {
      snprintf(out, out_size, "%s", full);
      found = 1;
      break;
    }
  }
  closedir(d);
  return found ? 0 : -1;
#endif
}

static int extract_targz(const char* archive, const char* destdir) {
  return run_cmd("tar -xzf \"%s\" -C \"%s\"", archive, destdir);
}

static int extract_zip(const char* archive, const char* destdir) {
#ifdef _WIN32
  return run_cmd("powershell -NoProfile -Command \"Expand-Archive -Path "
                 "'%s' -DestinationPath '%s' -Force\"",
                 archive, destdir);
#else
  return run_cmd("unzip -q -o \"%s\" -d \"%s\"", archive, destdir);
#endif
}

static int create_zip(const char* srcdir, const char* zippath) {
  char abs_zip[1024];
  to_abs_path(zippath, abs_zip, sizeof(abs_zip));
#ifdef _WIN32
  run_cmd("if exist \"%s\" del /Q \"%s\" >nul 2>nul", abs_zip, abs_zip);
#else
  run_cmd("rm -f \"%s\"", zippath);
#endif
#ifdef _WIN32
  return run_cmd("powershell -NoProfile -Command \"Compress-Archive -Path "
                 "'%s\\*' -DestinationPath '%s' -Force\"",
                 srcdir, abs_zip);
#else
  return run_cmd("(cd \"%s\" && zip -qr \"%s\" .)", srcdir, abs_zip);
#endif
}

static int check_zig_version(void) {
  char out[256];
  if (run_cmd_capture(out, sizeof(out), "zig version") != 0) {
    fprintf(stderr, "error: failed to run `zig version` (is zig on PATH?)\n");
    return -1;
  }
  if (strcmp(out, V6_ZIG_VERSION) == 0)
    return 0;
  if (getenv("V6_BUILD_ALLOW_ZIG_MISMATCH")) {
    fprintf(stderr,
            "warning: installed zig %s != pinned %s (allowed via "
            "V6_BUILD_ALLOW_ZIG_MISMATCH)\n",
            out, V6_ZIG_VERSION);
    return 0;
  }
  fprintf(stderr,
          "error: installed zig version %s does not match the pinned "
          "version %s\n"
          "       set V6_BUILD_ALLOW_ZIG_MISMATCH=1 to override\n",
          out, V6_ZIG_VERSION);
  return -1;
}

static int ensure_jdk_downloaded(const target_spec* t, char* archive_path,
                                 size_t archive_path_size) {
  ensure_dir("build/target/cache");
  const char* base = strrchr(t->jdk_url, '/');
  base = base ? base + 1 : t->jdk_url;
  char clean_name[256];
  int j = 0;
  for (int i = 0; base[i] && j < (int)sizeof(clean_name) - 1; i++) {
    if (base[i] != '%')
      clean_name[j++] = base[i];
  }
  clean_name[j] = '\0';
  snprintf(archive_path, archive_path_size, "build/target/cache/%s-%s",
           t->zig_target, clean_name);

  if (path_exists(archive_path)) {
    char hash[65];
    if (sha256_file(archive_path, hash) == 0 &&
        strcmp(hash, t->jdk_sha256) == 0) {
      printf("  [%s] jdk archive cached: %s\n", t->zig_target, archive_path);
      return 0;
    }
    printf("  [%s] cached jdk archive missing/mismatched, re-downloading\n",
           t->zig_target);
  }

  printf("  [%s] downloading JDK: %s\n", t->zig_target, t->jdk_url);
  if (run_cmd("curl -sL --fail -o \"%s\" \"%s\"", archive_path, t->jdk_url) !=
      0) {
    fprintf(stderr, "  [%s] error: JDK download failed\n", t->zig_target);
    return -1;
  }

  char hash[65];
  if (sha256_file(archive_path, hash) != 0 ||
      strcmp(hash, t->jdk_sha256) != 0) {
    fprintf(stderr,
            "  [%s] error: downloaded JDK checksum mismatch\n"
            "    expected %s\n    got      %s\n",
            t->zig_target, t->jdk_sha256, hash);
    return -1;
  }
  return 0;
}

static int stage_portable_jdk(const target_spec* t, const char* stage_dir) {
  char archive_path[512];
  if (ensure_jdk_downloaded(t, archive_path, sizeof(archive_path)) != 0)
    return -1;

  char extract_dir[512];
  snprintf(extract_dir, sizeof(extract_dir), "build/target/jdk-extract/%s",
           t->zig_target);
  remove_dir(extract_dir);
  ensure_dir(extract_dir);

  int rc = strcmp(t->jdk_archive_ext, "zip") == 0
               ? extract_zip(archive_path, extract_dir)
               : extract_targz(archive_path, extract_dir);
  if (rc != 0) {
    fprintf(stderr, "  [%s] error: failed to extract JDK archive\n",
            t->zig_target);
    return -1;
  }

  char jdk_root[512];
  if (find_single_subdir(extract_dir, jdk_root, sizeof(jdk_root)) != 0) {
    fprintf(stderr, "  [%s] error: unexpected JDK archive layout\n",
            t->zig_target);
    return -1;
  }
  strcat(jdk_root, t->jdk_root_suffix);

  char dest[512];
  snprintf(dest, sizeof(dest), "%s/jdk", stage_dir);
  ensure_dir(dest);
  if (copy_tree_contents(jdk_root, dest) != 0) {
    fprintf(stderr, "  [%s] error: failed to copy JDK into staging dir\n",
            t->zig_target);
    return -1;
  }
  return 0;
}

#define MAX_PARALLEL 16

static int compile_sources_parallel(const target_spec* t,
                                    const strlist* sources, const char* obj_dir,
                                    FILE* rsp) {
  int njobs = get_cpu_count();
  if (njobs < 1)
    njobs = 1;
  if (njobs > MAX_PARALLEL)
    njobs = MAX_PARALLEL;

  int n = sources->count;
  int i = 0;
  while (i < n) {
    int batch = n - i < njobs ? n - i : njobs;
    proc_handle handles[MAX_PARALLEL];
    char obj_paths[MAX_PARALLEL][512];
    char inc_flags[MAX_PARALLEL][256];
    char* argvs[MAX_PARALLEL][20];

    for (int k = 0; k < batch; k++) {
      const char* src = sources->items[i + k];
      const char* base = strrchr(src, '/');
      base = base ? base + 1 : src;
      snprintf(obj_paths[k], sizeof(obj_paths[k]), "%s/%s.o", obj_dir, base);
      snprintf(inc_flags[k], sizeof(inc_flags[k]), "-Iinclude/jni/%s",
               t->jni_dir);

      int ac = 0;
      argvs[k][ac++] = "zig";
      argvs[k][ac++] = "cc";
      argvs[k][ac++] = "-std=c11";
      argvs[k][ac++] = "-Wall";
      argvs[k][ac++] = "-Wextra";
      argvs[k][ac++] = "-Iinclude";
      argvs[k][ac++] = "-Iinclude/jni";
      argvs[k][ac++] = inc_flags[k];
      argvs[k][ac++] = "-DV6_HAVE_JNI";
      if (t->is_windows)
        argvs[k][ac++] = "-D_CRT_SECURE_NO_WARNINGS";
      argvs[k][ac++] = "-O3";
      argvs[k][ac++] = "-DNDEBUG";
      argvs[k][ac++] = "-target";
      argvs[k][ac++] = (char*)t->zig_target;
      argvs[k][ac++] = "-c";
      argvs[k][ac++] = (char*)src;
      argvs[k][ac++] = "-o";
      argvs[k][ac++] = obj_paths[k];
      argvs[k][ac] = NULL;

      if (spawn_async(argvs[k], &handles[k]) != 0) {
        fprintf(stderr, "  [%s] error: failed to spawn compiler for %s\n",
                t->zig_target, src);
        return -1;
      }
    }

    int had_error = 0;
    for (int k = 0; k < batch; k++) {
      int rc = wait_proc(&handles[k]);
      if (rc != 0) {
        fprintf(stderr, "  [%s] error: failed to compile %s\n", t->zig_target,
                sources->items[i + k]);
        had_error = 1;
      } else {
        fprintf(rsp, "\"%s\"\n", obj_paths[k]);
      }
    }
    if (had_error)
      return -1;

    i += batch;
  }
  return 0;
}

static int compile_and_link(const target_spec* t, const strlist* sources,
                            const char* bin_path) {
  char obj_dir[256];
  snprintf(obj_dir, sizeof(obj_dir), "build/target/obj/%s", t->zig_target);
  ensure_dir(obj_dir);

  char rsp_path[256];
  snprintf(rsp_path, sizeof(rsp_path), "build/target/obj/%s/link.rsp",
           t->zig_target);
  FILE* rsp = fopen(rsp_path, "w");
  if (!rsp)
    return -1;

  int rc = compile_sources_parallel(t, sources, obj_dir, rsp);
  fclose(rsp);
  if (rc != 0)
    return -1;

  ensure_dir("build/target/bin");
  char bin_dir[256];
  snprintf(bin_dir, sizeof(bin_dir), "build/target/bin/%s", t->zig_target);
  ensure_dir(bin_dir);

  int link_rc = run_cmd("zig cc -target %s @%s -o \"%s\" %s %s", t->zig_target,
                        rsp_path, bin_path, t->needs_ldl ? "-ldl" : "",
                        t->is_windows ? "-Wl,/STACK:8388608" : "");
  if (link_rc != 0) {
    fprintf(stderr, "  [%s] error: link failed\n", t->zig_target);
    return -1;
  }
  return 0;
}

static int package_zip(const char* stage_dir, const char* zip_path) {
  return create_zip(stage_dir, zip_path);
}

static int build_target(const target_spec* t, const strlist* sources) {
  printf("== %s ==\n", t->zig_target);

  char bin_name[64];
  snprintf(bin_name, sizeof(bin_name), "v6%s", t->exe_suffix);
  char bin_path[256];
  snprintf(bin_path, sizeof(bin_path), "build/target/bin/%s/%s", t->zig_target,
           bin_name);

  if (compile_and_link(t, sources, bin_path) != 0)
    return -1;
  printf("  [%s] compiled %s\n", t->zig_target, bin_path);

  char dev_stage[256];
  snprintf(dev_stage, sizeof(dev_stage), "build/target/stage/%s-developer",
           t->zig_target);
  remove_dir(dev_stage);
  ensure_dir(dev_stage);
  char dev_bin[256];
  snprintf(dev_bin, sizeof(dev_bin), "%s/%s", dev_stage, bin_name);
  copy_file(bin_path, dev_bin);

  char dev_zip[256];
  snprintf(dev_zip, sizeof(dev_zip), "build/target/v6-%s-developer.zip",
           t->zig_target);
  if (package_zip(dev_stage, dev_zip) != 0) {
    fprintf(stderr, "  [%s] error: failed to create %s\n", t->zig_target,
            dev_zip);
    return -1;
  }
  printf("  [%s] wrote %s\n", t->zig_target, dev_zip);

  char port_stage[256];
  snprintf(port_stage, sizeof(port_stage), "build/target/stage/%s-portable",
           t->zig_target);
  remove_dir(port_stage);
  ensure_dir(port_stage);
  char port_bin[256];
  snprintf(port_bin, sizeof(port_bin), "%s/%s", port_stage, bin_name);
  copy_file(bin_path, port_bin);

  if (stage_portable_jdk(t, port_stage) != 0)
    return -1;

  char port_zip[256];
  snprintf(port_zip, sizeof(port_zip), "build/target/v6-%s-portable.zip",
           t->zig_target);
  if (package_zip(port_stage, port_zip) != 0) {
    fprintf(stderr, "  [%s] error: failed to create %s\n", t->zig_target,
            port_zip);
    return -1;
  }
  printf("  [%s] wrote %s\n", t->zig_target, port_zip);
  return 0;
}

int main(int argc, char** argv) {
  setvbuf(stdout, NULL, _IONBF, 0);
  printf("v6 cross-build (pinned zig %s)\n", V6_ZIG_VERSION);

  if (check_zig_version() != 0)
    return 1;

  printf("generating java runtime classes...\n");
  if (run_cmd("make BUILD_TYPE=release javaclasses") != 0) {
    fprintf(stderr, "error: failed to generate java runtime classes\n");
    return 1;
  }

  strlist sources;
  strlist_init(&sources);
  list_files("src", ".c", &sources);
  list_files("build/release/lib/rt", ".c", &sources);
  strlist_push(&sources, "build/release/lib/runtime_classes.c");
  printf("compiling %d source files per target\n", sources.count);

  ensure_dir("build/target");

  int selected[32];
  int selected_count = 0;
  if (argc > 1) {
    for (int i = 1; i < argc; i++) {
      for (int j = 0; j < TARGET_COUNT; j++) {
        if (strcmp(argv[i], TARGETS[j].zig_target) == 0)
          selected[selected_count++] = j;
      }
    }
  } else {
    for (int j = 0; j < TARGET_COUNT; j++)
      selected[selected_count++] = j;
  }

  int ok_count = 0;
  int fail_count = 0;
  const char* failed_names[32];

  for (int i = 0; i < selected_count; i++) {
    const target_spec* t = &TARGETS[selected[i]];
    if (build_target(t, &sources) == 0) {
      ok_count++;
    } else {
      failed_names[fail_count++] = t->zig_target;
    }
  }

  strlist_free(&sources);

  printf("\n%d/%d targets succeeded (2 zip(s) each)\n", ok_count,
         selected_count);
  if (fail_count > 0) {
    printf("failed targets:\n");
    for (int i = 0; i < fail_count; i++)
      printf("  %s\n", failed_names[i]);
    return 1;
  }
  return 0;
}
