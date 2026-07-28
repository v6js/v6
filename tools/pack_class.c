#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
  if (argc != 4) {
    fprintf(stderr, "usage: %s <in.class> <symbol> <out.c>\n", argv[0]);
    return 1;
  }

  const char* in_path = argv[1];
  const char* sym = argv[2];
  const char* out_path = argv[3];

  FILE* in = fopen(in_path, "rb");
  if (!in) {
    fprintf(stderr, "error: cannot read %s\n", in_path);
    return 1;
  }

  fseek(in, 0, SEEK_END);
  long n = ftell(in);
  fseek(in, 0, SEEK_SET);

  unsigned char* data = malloc((size_t)n);
  fread(data, 1, (size_t)n, in);
  fclose(in);

  FILE* out = fopen(out_path, "w");
  if (!out) {
    fprintf(stderr, "error: cannot write %s\n", out_path);
    free(data);
    return 1;
  }

  fprintf(out, "#include <stddef.h>\n\n");
  fprintf(out, "const unsigned char %s[] = {\n", sym);
  for (long i = 0; i < n; i++) {
    fprintf(out, "%s0x%02x,%s", (i % 12 == 0) ? "  " : "", data[i],
            (i % 12 == 11) ? "\n" : "");
  }
  fprintf(out, "\n};\n\n");
  fprintf(out, "const size_t %s_len = sizeof(%s);\n", sym, sym);

  fclose(out);
  free(data);
  return 0;
}
