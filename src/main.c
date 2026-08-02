#include "v6/bytecode.h"
#include "v6/jar.h"
#include "v6/jvm.h"
#include "v6/module.h"
#include "v6/parser.h"
#include "v6/runtime.h"

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
  fprintf(stderr, "usage: %s <script.js> [-o <output.jar>]\n", prog);
}

int main(int argc, char** argv) {
  const char* in_path = NULL;
  const char* out_path = NULL;

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

  module_ctx modctx;
  compile_result rc = compile_program(src, &cf, in_path, &modctx);
  free(src);

  if (!rc.ok) {
    fprintf(stderr, "error: %s:%d: %s\n", in_path, rc.line, rc.message);
    cf_free(&cf);
    return 1;
  }

  buf out;
  buf_init(&out);
  cf_emit(&cf, &out);
  cf_free(&cf);

  buf* mod_bufs =
      modctx.count > 0 ? malloc(sizeof(buf) * (size_t)modctx.count) : NULL;
  for (int i = 0; i < modctx.count; i++) {
    buf_init(&mod_bufs[i]);
    cf_emit(modctx.modules[i].cf, &mod_bufs[i]);
    cf_free(modctx.modules[i].cf);
    free(modctx.modules[i].cf);
  }

  if (out_path) {
    size_t n = v6_runtime_class_count;
    size_t total = n + 1 + (size_t)modctx.count;
    jar_entry* entries = malloc(total * sizeof(jar_entry));
    entries[0].name = "Main.class";
    entries[0].data = out.data;
    entries[0].len = out.len;
    for (size_t i = 0; i < n; i++) {
      char* name = malloc(strlen(v6_runtime_classes[i].name) + 7);
      sprintf(name, "%s.class", v6_runtime_classes[i].name);
      entries[i + 1].name = name;
      entries[i + 1].data = v6_runtime_classes[i].data;
      entries[i + 1].len = v6_runtime_classes[i].len;
    }
    for (int i = 0; i < modctx.count; i++) {
      char* name = malloc(strlen(modctx.modules[i].class_name) + 7);
      sprintf(name, "%s.class", modctx.modules[i].class_name);
      entries[n + 1 + (size_t)i].name = name;
      entries[n + 1 + (size_t)i].data = mod_bufs[i].data;
      entries[n + 1 + (size_t)i].len = mod_bufs[i].len;
    }

    buf jar;
    buf_init(&jar);
    jar_write(&jar, entries, total, "Main");
    buf_free(&out);
    for (int i = 0; i < modctx.count; i++)
      buf_free(&mod_bufs[i]);
    free(mod_bufs);

    FILE* outf = fopen(out_path, "wb");
    if (!outf) {
      fprintf(stderr, "error: cannot write %s\n", out_path);
      buf_free(&jar);
      return 1;
    }
    fwrite(jar.data, 1, jar.len, outf);
    fclose(outf);
    printf("wrote %s (%zu bytes)\n", out_path, jar.len);
    buf_free(&jar);
    return 0;
  }

  if (!v6_jvm_available()) {
    fprintf(stderr, "error: no JVM available (built without JAVA_HOME?)\n");
    buf_free(&out);
    for (int i = 0; i < modctx.count; i++)
      buf_free(&mod_bufs[i]);
    free(mod_bufs);
    return 1;
  }

  v6_jvm* jvm = v6_jvm_create();
  if (!jvm) {
    fprintf(stderr, "error: failed to create JVM\n");
    buf_free(&out);
    for (int i = 0; i < modctx.count; i++)
      buf_free(&mod_bufs[i]);
    free(mod_bufs);
    return 1;
  }

  if (v6_jvm_load_runtime(jvm) != 0) {
    fprintf(stderr, "error: failed to load runtime\n");
    v6_jvm_destroy(jvm);
    buf_free(&out);
    for (int i = 0; i < modctx.count; i++)
      buf_free(&mod_bufs[i]);
    free(mod_bufs);
    return 1;
  }

  int mods_ok = 1;
  for (int i = 0; i < modctx.count; i++) {
    if (v6_jvm_define_extra(jvm, modctx.modules[i].class_name, mod_bufs[i].data,
                            mod_bufs[i].len) != 0) {
      mods_ok = 0;
    }
    buf_free(&mod_bufs[i]);
  }
  free(mod_bufs);

  if (!mods_ok) {
    fprintf(stderr, "error: failed to load module classes\n");
    v6_jvm_destroy(jvm);
    buf_free(&out);
    return 1;
  }

  int run_rc = v6_jvm_run(jvm, out.data, out.len);
  buf_free(&out);
  v6_jvm_destroy(jvm);

  if (run_rc != 0) {
    fprintf(stderr, "error: program failed\n");
    return 1;
  }

  return 0;
}
