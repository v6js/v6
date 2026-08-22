#include "v6/cli.h"

#include "v6/optimizer.h"
#include "v6/color.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* read_whole_file(const char* path, size_t* out_len) {
  FILE* f = fopen(path, "rb");
  if (!f)
    return NULL;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (n < 0) {
    fclose(f);
    return NULL;
  }
  char* buf = malloc((size_t)n + 1);
  size_t got = fread(buf, 1, (size_t)n, f);
  buf[got] = '\0';
  fclose(f);
  if (out_len)
    *out_len = got;
  return buf;
}

static int has_suffix(const char* s, const char* suffix) {
  size_t ls = strlen(s), lsuf = strlen(suffix);
  if (lsuf > ls)
    return 0;
  return strcmp(s + ls - lsuf, suffix) == 0;
}

static int write_output(const char* outfile, const char* data, size_t len) {
  if (!outfile) {
    fwrite(data, 1, len, stdout);
    return 0;
  }
  FILE* f = fopen(outfile, "wb");
  if (!f)
    return -1;
  size_t written = fwrite(data, 1, len, f);
  fclose(f);
  return written == len ? 0 : -1;
}

int v6_cli_run_optimize(v6_cli_options* opts) {
  if (!opts->optimize_entry) {
    fprintf(stderr, "error: --optimize requires an input file\n");
    return 1;
  }

  size_t src_len = 0;
  char* src = read_whole_file(opts->optimize_entry, &src_len);
  if (!src) {
    fprintf(stderr, "error: cannot read %s\n", opts->optimize_entry);
    return 1;
  }

  char* out = NULL;
  size_t out_len = 0;

  if (has_suffix(opts->optimize_entry, ".css")) {
    out = v6_optimizer_run_css(src, src_len, &opts->optimizer, &out_len);
  } else if (has_suffix(opts->optimize_entry, ".json")) {
    out = v6_optimizer_run_json(src, src_len, &opts->optimizer, &out_len);
  } else if (has_suffix(opts->optimize_entry, ".js") ||
             has_suffix(opts->optimize_entry, ".mjs") ||
             has_suffix(opts->optimize_entry, ".cjs")) {
    char err[1024];
    err[0] = '\0';
    int err_line = 0;
    out = v6_optimizer_run_js(src, src_len, &opts->optimizer, &out_len, err,
                              sizeof(err), &err_line);
    if (!out) {
      fprintf(stderr, "error: %s:%d: %s\n", opts->optimize_entry, err_line,
              err);
      free(src);
      return 1;
    }
  } else {
    fprintf(stderr,
            "error: %s: unsupported file type (expected .js, .mjs, .cjs, "
            ".css, or .json)\n",
            opts->optimize_entry);
    free(src);
    return 1;
  }

  free(src);

  if (write_output(opts->optimize_outfile, out, out_len) != 0) {
    fprintf(stderr, "error: cannot write %s\n", opts->optimize_outfile);
    free(out);
    return 1;
  }

  if (opts->optimize_outfile) {
    int c = v6_color_enabled_out();
    printf("%s%s%10s%s %s -> %s\n", v6_c_bold(c), v6_c_green(c), "Optimized",
           v6_c_reset(c), opts->optimize_entry, opts->optimize_outfile);
  }

  free(out);
  return 0;
}
