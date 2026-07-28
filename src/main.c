#include "v6/bytecode.h"
#include "v6/parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* read_file(const char* path) {
  FILE* f = fopen(path, "rb");
  if (!f)
    return NULL;

  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);

  char* src = malloc((size_t)n + 1);
  if (!src) {
    fclose(f);
    return NULL;
  }

  fread(src, 1, (size_t)n, f);
  src[n] = '\0';
  fclose(f);
  return src;
}

static void usage(const char* prog) {
  fprintf(stderr, "usage: %s <script.js> [-o <output.class>]\n", prog);
}

int main(int argc, char** argv) {
  const char* in_path = NULL;
  const char* out_path = "Main.class";

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
      out_path = argv[++i];
    } else if (!in_path) {
      in_path = argv[i];
    } else {
      usage(argv[0]);
      return 1;
    }
  }

  if (!in_path) {
    usage(argv[0]);
    return 1;
  }

  char* src = read_file(in_path);
  if (!src) {
    fprintf(stderr, "error: cannot read %s\n", in_path);
    return 1;
  }

  class_file cf;
  cf_init(&cf, "Main", "java/lang/Object");

  int rc = compile_program(src, &cf);
  free(src);

  if (rc != 0) {
    fprintf(stderr, "error: failed to parse %s\n", in_path);
    cf_free(&cf);
    return 1;
  }

  buf out;
  buf_init(&out);
  cf_emit(&cf, &out);

  FILE* outf = fopen(out_path, "wb");
  if (!outf) {
    fprintf(stderr, "error: cannot write %s\n", out_path);
    buf_free(&out);
    cf_free(&cf);
    return 1;
  }
  fwrite(out.data, 1, out.len, outf);
  fclose(outf);

  printf("wrote %s (%zu bytes)\n", out_path, out.len);

  buf_free(&out);
  cf_free(&cf);
  return 0;
}
