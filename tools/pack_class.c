#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static char* base_name(const char* path) {
  const char* slash = strrchr(path, '/');
  const char* bslash = strrchr(path, '\\');
  const char* start = path;
  if (slash && slash + 1 > start)
    start = slash + 1;
  if (bslash && bslash + 1 > start)
    start = bslash + 1;
  const char* dot = strrchr(start, '.');
  size_t len = dot ? (size_t)(dot - start) : strlen(start);
  char* name = malloc(len + 1);
  memcpy(name, start, len);
  name[len] = '\0';
  return name;
}

static unsigned char* read_file(const char* path, long* out_len) {
  FILE* f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "error: cannot read %s\n", path);
    exit(1);
  }
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  unsigned char* data = malloc((size_t)n);
  fread(data, 1, (size_t)n, f);
  fclose(f);
  *out_len = n;
  return data;
}

static int get_mtime(const char* path, long long* out_mtime) {
  struct stat st;
  if (stat(path, &st) != 0)
    return -1;
  *out_mtime = (long long)st.st_mtime;
  return 0;
}

static int class_is_up_to_date(const char* class_path, const char* out_c_path) {
  long long class_mtime, out_mtime;
  if (get_mtime(out_c_path, &out_mtime) != 0)
    return 0;
  if (get_mtime(class_path, &class_mtime) != 0)
    return 0;
  return class_mtime <= out_mtime;
}

static long file_len(const char* path) {
  struct stat st;
  if (stat(path, &st) != 0)
    return -1;
  return (long)st.st_size;
}

static void write_class_c(const char* out_c_path, const char* name,
                          const char* class_path) {
  long len;
  unsigned char* data = read_file(class_path, &len);

  FILE* out = fopen(out_c_path, "w");
  if (!out) {
    fprintf(stderr, "error: cannot write %s\n", out_c_path);
    exit(1);
  }

  fprintf(out, "const unsigned char v6_data_%s[] = {\n", name);
  for (long j = 0; j < len; j++) {
    fprintf(out, "%s0x%02x,%s", (j % 12 == 0) ? "  " : "", data[j],
            (j % 12 == 11) ? "\n" : "");
  }
  fprintf(out, "\n};\n");

  fclose(out);
  free(data);
}

static void join_path(char* out, size_t out_size, const char* dir,
                      const char* file) {
  size_t dlen = strlen(dir);
  int need_sep = dlen > 0 && dir[dlen - 1] != '/' && dir[dlen - 1] != '\\';
  snprintf(out, out_size, "%s%s%s", dir, need_sep ? "/" : "", file);
}

int main(int argc, char** argv) {
  if (argc < 4) {
    fprintf(stderr,
            "usage: %s <gen-dir> <index.c> <class1.class> [class2.class ...]\n",
            argv[0]);
    return 1;
  }

  const char* gen_dir = argv[1];
  const char* index_path = argv[2];
  int count = argc - 3;
  char** class_paths = argv + 3;

  char** names = malloc((size_t)count * sizeof(char*));
  int regenerated = 0;

  char lengths_path[4096];
  join_path(lengths_path, sizeof(lengths_path), gen_dir, "lengths.h");
  FILE* lengths = fopen(lengths_path, "w");
  if (!lengths) {
    fprintf(stderr, "error: cannot write %s\n", lengths_path);
    return 1;
  }

  for (int i = 0; i < count; i++) {
    names[i] = base_name(class_paths[i]);
    char out_c_path[4096];
    char c_file_name[512];
    snprintf(c_file_name, sizeof(c_file_name), "%s.c", names[i]);
    join_path(out_c_path, sizeof(out_c_path), gen_dir, c_file_name);

    if (!class_is_up_to_date(class_paths[i], out_c_path)) {
      write_class_c(out_c_path, names[i], class_paths[i]);
      regenerated++;
    }

    fprintf(lengths, "#define V6_LEN_%s %ld\n", names[i],
            file_len(class_paths[i]));
  }
  fclose(lengths);

  FILE* out = fopen(index_path, "w");
  if (!out) {
    fprintf(stderr, "error: cannot write %s\n", index_path);
    return 1;
  }

  fprintf(out, "#include <stddef.h>\n");
  fprintf(out, "#include \"rt/lengths.h\"\n\n");
  for (int i = 0; i < count; i++)
    fprintf(out, "extern const unsigned char v6_data_%s[V6_LEN_%s];\n",
            names[i], names[i]);

  fprintf(out, "\ntypedef struct {\n"
               "  const char* name;\n"
               "  const unsigned char* data;\n"
               "  size_t len;\n"
               "} v6_rt_class;\n\n");
  fprintf(out, "const v6_rt_class v6_runtime_classes[] = {\n");
  for (int i = 0; i < count; i++) {
    fprintf(out, "  {\"%s\", v6_data_%s, sizeof(v6_data_%s)},\n", names[i],
            names[i], names[i]);
    free(names[i]);
  }
  fprintf(out, "};\n\n");
  fprintf(out, "const size_t v6_runtime_class_count = %d;\n", count);

  free(names);
  fclose(out);

  if (regenerated > 0)
    fprintf(stderr, "pack_class: regenerated %d/%d class table(s)\n",
            regenerated, count);

  return 0;
}
