#include "v6/cli.h"
#include "v6/bundle_graph.h"
#include "v6/bundle_emit.h"
#include "v6/bundle_fsutil.h"
#include "v6/module.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_format(const char* s, bundle_format* out) {
  if (!s || strcmp(s, "esm") == 0) {
    *out = bundle_fmt_esm;
    return 0;
  }
  if (strcmp(s, "cjs") == 0) {
    *out = bundle_fmt_cjs;
    return 0;
  }
  if (strcmp(s, "iife") == 0) {
    *out = bundle_fmt_iife;
    return 0;
  }
  return -1;
}

static const char* pick_default_entry(void) {
  static const char* candidates[] = {
      "index.js", "src/index.js", "index.mjs", "src/index.mjs", NULL,
  };
  for (int i = 0; candidates[i]; i++) {
    if (path_is_regular_file(candidates[i]))
      return candidates[i];
  }
  return NULL;
}

int v6_cli_run_bundle(v6_cli_options* opts) {
  const char* entry = opts->bundle_entry;
  if (!entry)
    entry = pick_default_entry();
  if (!entry) {
    fprintf(stderr, "error: no entry file given and none of index.js, "
                    "src/index.js, index.mjs, src/index.mjs exist\n");
    return 1;
  }

  bundle_format fmt;
  if (parse_format(opts->bundle_format, &fmt) != 0) {
    fprintf(stderr,
            "error: unknown --format \"%s\" (expected esm, cjs, or iife)\n",
            opts->bundle_format);
    return 1;
  }

  if (opts->bundle_watch || opts->bundle_serve) {
    fprintf(stderr, "error: --watch/--serve are not implemented yet\n");
    return 1;
  }

  bundle_graph g;
  bundle_graph_init(&g);
  int rc = bundle_graph_build(&g, entry);
  if (rc != 0) {
    for (int i = 0; i < g.error_count; i++)
      fprintf(stderr, "error: %s\n", g.errors[i]);
    bundle_graph_free(&g);
    return 1;
  }

  int had_asset = 0;
  for (int i = 0; i < g.count; i++) {
    if (g.modules[i]->kind == bundle_mod_css ||
        g.modules[i]->kind == bundle_mod_asset) {
      fprintf(stderr,
              "error: %s: asset imports (css/images/etc) are not yet "
              "supported by the bundler\n",
              g.modules[i]->abs_path);
      had_asset = 1;
    }
  }
  if (had_asset) {
    bundle_graph_free(&g);
    return 1;
  }

  bundle_emit_options eopts;
  eopts.format = fmt;
  eopts.global_name = opts->bundle_global_name;

  size_t out_len = 0;
  char* output = bundle_emit(&g, &eopts, &out_len);

  const char* outfile =
      opts->bundle_outfile ? opts->bundle_outfile : "dist/bundle.js";
  char dir[1024];
  path_dirname(outfile, dir, sizeof(dir));
  bundle_mkdir_p(dir);

  if (bundle_write_file(outfile, output, out_len) != 0) {
    fprintf(stderr, "error: cannot write %s\n", outfile);
    free(output);
    bundle_graph_free(&g);
    return 1;
  }

  printf("bundled %d module%s -> %s (%zu bytes)\n", g.count,
         g.count == 1 ? "" : "s", outfile, out_len);

  free(output);
  bundle_graph_free(&g);
  return 0;
}
