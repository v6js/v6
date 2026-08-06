#include "v6/cli.h"

#include "v6/bytecode.h"
#include "v6/daemon.h"
#include "v6/jar.h"
#include "v6/jvm.h"
#include "v6/module.h"
#include "v6/parser.h"
#include "v6/runtime.h"
#include "v6/version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define V6_DEFAULT_DAEMON_IDLE_MS (30L * 60L * 1000L)
#define V6_DEFAULT_DAEMON_EXEC_TIMEOUT_MS (5L * 60L * 1000L)

static long v6_daemon_idle_ms(void) {
  const char* env = getenv("V6_DAEMON_IDLE_MS");
  if (env && env[0])
    return atol(env);
  return V6_DEFAULT_DAEMON_IDLE_MS;
}

static long v6_daemon_exec_timeout_ms(void) {
  const char* env = getenv("V6_DAEMON_EXEC_TIMEOUT_MS");
  if (env && env[0])
    return atol(env);
  return V6_DEFAULT_DAEMON_EXEC_TIMEOUT_MS;
}

int v6_cli_serve_daemon(const char* lock_path) {
  if (!v6_jvm_available()) {
    fprintf(stderr, "error: no JVM available (built without JAVA_HOME?)\n");
    return 1;
  }

  char exe_path[1024];
  if (v6_get_own_exe_path(exe_path, sizeof(exe_path)) != 0) {
    fprintf(stderr, "error: cannot resolve own executable path\n");
    return 1;
  }

  long long mtime = 0, size = 0;
  v6_stat_file(exe_path, &mtime, &size);

  v6_jvm* jvm = v6_jvm_create(NULL, 1);
  if (!jvm) {
    fprintf(stderr, "error: failed to create JVM\n");
    return 1;
  }

  if (v6_jvm_load_runtime(jvm) != 0) {
    fprintf(stderr, "error: failed to load runtime\n");
    v6_jvm_destroy(jvm);
    return 1;
  }

  v6_jvm_serve_daemon(jvm, lock_path, v6_daemon_idle_ms(), mtime, size,
                      v6_daemon_exec_timeout_ms());
  v6_jvm_destroy(jvm);
  return 0;
}

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

static const char* basename_of(const char* path) {
  const char* slash = strrchr(path, '/');
  const char* bslash = strrchr(path, '\\');
  const char* last = slash;
  if (bslash && (!last || bslash > last))
    last = bslash;
  return last ? last + 1 : path;
}

void v6_cli_print_usage(const char* prog_path) {
  const char* prog = basename_of(prog_path);
  fprintf(stderr,
          "usage: %s [options] <script.js> [script args...]\n"
          "       %s [options] -e \"<code>\"\n"
          "       %s [options]\n"
          "       %s --version | -v\n"
          "       %s --help | -h\n",
          prog, prog, prog, prog, prog);
}

void v6_cli_print_help(const char* prog_path) {
  v6_cli_print_usage(prog_path);
  fprintf(stderr,
          "\n"
          "options:\n"
          "  -e, --eval <code>       evaluate <code> and exit\n"
          "  -o <output.jar>         compile to a standalone AOT jar\n"
          "  -cp, --classpath <cp>   extra Java classpath for java: imports\n"
          "  --no-daemon             skip the persistent-daemon fast path\n"
          "  --color                 force colored output\n"
          "  --no-color              disable colored output\n"
          "  -v, --version           print the version and exit\n"
          "  -h, --help              print this help and exit\n"
          "\n"
          "running with no script starts an interactive REPL.\n");
}

void v6_cli_print_version(void) {
  printf("v6 %s (zig %s)\n", V6_VERSION, V6_ZIG_VERSION);
  char jdk_version[64];
  if (v6_jvm_detect_version(jdk_version, sizeof(jdk_version)) == 0)
    printf("jdk %s\n", jdk_version);
  else
    printf("jdk not found (JAVA_HOME unset, no bundled JDK)\n");
}

v6_cli_action v6_cli_parse(int argc, char** argv, v6_cli_options* opts) {
  memset(opts, 0, sizeof(*opts));
  opts->prog = argv[0];
  opts->color_mode = v6_color_auto;

  if (argc > 1 &&
      (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0))
    return v6_action_version;

  if (argc > 1 &&
      (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0))
    return v6_action_help;

  if (argc > 2 && strcmp(argv[1], "--__v6_daemon_serve__") == 0) {
    opts->daemon_lock_path = argv[2];
    return v6_action_daemon_serve;
  }

  opts->script_args = argc > 1 ? malloc(sizeof(char*) * (size_t)argc) : NULL;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
      opts->out_path = argv[++i];
    } else if ((strcmp(argv[i], "-cp") == 0 ||
                strcmp(argv[i], "--classpath") == 0) &&
               i + 1 < argc) {
      opts->classpath = argv[++i];
    } else if ((strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--eval") == 0) &&
               i + 1 < argc) {
      opts->eval_code = argv[++i];
    } else if (strcmp(argv[i], "--no-daemon") == 0) {
      opts->no_daemon = 1;
    } else if (strcmp(argv[i], "--color") == 0) {
      opts->color_mode = v6_color_always;
    } else if (strcmp(argv[i], "--no-color") == 0) {
      opts->color_mode = v6_color_never;
    } else if (!opts->script_path && !opts->eval_code) {
      opts->script_path = argv[i];
    } else {
      opts->script_args[opts->script_argc++] = argv[i];
    }
  }

  if (opts->eval_code)
    return v6_action_eval;
  if (opts->script_path)
    return v6_action_run_script;
  return v6_action_repl;
}

int v6_cli_run_source(const char* src, const char* in_path,
                      v6_cli_options* opts) {
  class_file cf;
  cf_init(&cf, "Main", "java/lang/Object");

  module_ctx modctx;
  compile_result rc = compile_program(src, &cf, in_path, &modctx);

  if (!rc.ok) {
    int c = v6_color_enabled_err();
    fprintf(stderr, "%s%serror%s: %s:%d: %s\n", v6_c_bold(c), v6_c_red(c),
            v6_c_reset(c), in_path, rc.line, rc.message);
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

  if (opts->out_path) {
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

    FILE* outf = fopen(opts->out_path, "wb");
    if (!outf) {
      fprintf(stderr, "error: cannot write %s\n", opts->out_path);
      buf_free(&jar);
      return 1;
    }
    fwrite(jar.data, 1, jar.len, outf);
    fclose(outf);
    printf("wrote %s (%zu bytes)\n", opts->out_path, jar.len);
    buf_free(&jar);
    return 0;
  }

  int uses_stdin =
      strstr(src, "stdin") != NULL || strstr(src, "readline") != NULL;

  if (!opts->classpath && !opts->no_daemon && !uses_stdin) {
    int num_classes = 1 + modctx.count;
    v6_daemon_class_entry* classes =
        malloc(sizeof(v6_daemon_class_entry) * (size_t)num_classes);
    classes[0].name = "Main";
    classes[0].data = out.data;
    classes[0].len = out.len;
    for (int i = 0; i < modctx.count; i++) {
      classes[i + 1].name = modctx.modules[i].class_name;
      classes[i + 1].data = mod_bufs[i].data;
      classes[i + 1].len = mod_bufs[i].len;
    }

    int exit_code = 1;
    int handled = v6_daemon_run(opts->prog, classes, num_classes, in_path,
                                opts->script_args, opts->script_argc,
                                v6_color_enabled_out(), &exit_code);
    free(classes);
    if (handled) {
      buf_free(&out);
      for (int i = 0; i < modctx.count; i++)
        buf_free(&mod_bufs[i]);
      free(mod_bufs);
      return exit_code;
    }
  }

  if (!v6_jvm_available()) {
    fprintf(stderr, "error: no JVM available (built without JAVA_HOME?)\n");
    buf_free(&out);
    for (int i = 0; i < modctx.count; i++)
      buf_free(&mod_bufs[i]);
    free(mod_bufs);
    return 1;
  }

  v6_jvm* jvm = v6_jvm_create(opts->classpath, 0);
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

  int run_rc =
      v6_jvm_run(jvm, out.data, out.len, opts->script_args, opts->script_argc);
  buf_free(&out);
  v6_jvm_destroy(jvm);

  return run_rc < 0 ? 1 : run_rc;
}

int v6_cli_run_script(v6_cli_options* opts) {
  char* src = read_file(opts->script_path);
  if (!src) {
    fprintf(stderr, "error: cannot read %s\n", opts->script_path);
    return 1;
  }
  int rc = v6_cli_run_source(src, opts->script_path, opts);
  free(src);
  return rc;
}

int v6_cli_eval(v6_cli_options* opts) {
  return v6_cli_run_source(opts->eval_code, "[eval]", opts);
}
