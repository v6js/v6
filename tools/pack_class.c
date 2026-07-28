#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int main(int argc, char** argv) {
  if (argc < 3) {
    fprintf(stderr, "usage: %s <out.c> <class1.class> [class2.class ...]\n",
            argv[0]);
    return 1;
  }

  const char* out_path = argv[1];
  int count = argc - 2;

  FILE* out = fopen(out_path, "w");
  if (!out) {
    fprintf(stderr, "error: cannot write %s\n", out_path);
    return 1;
  }

  fprintf(out, "#include <stddef.h>\n\n");

  char** names = malloc((size_t)count * sizeof(char*));

  for (int i = 0; i < count; i++) {
    const char* path = argv[2 + i];
    long len;
    unsigned char* data = read_file(path, &len);
    names[i] = base_name(path);

    fprintf(out, "static const unsigned char v6_data_%s[] = {\n", names[i]);
    for (long j = 0; j < len; j++) {
      fprintf(out, "%s0x%02x,%s", (j % 12 == 0) ? "  " : "", data[j],
              (j % 12 == 11) ? "\n" : "");
    }
    fprintf(out, "\n};\n\n");
    free(data);
  }

  fprintf(out, "typedef struct {\n"
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
  return 0;
}
