#include "v6/cli.h"

#include "v6/bytecode.h"
#include "v6/cache.h"
#include "v6/daemon.h"
#include "v6/internal.h"
#include "v6/jar.h"
#include "v6/jvm.h"
#include "v6/module.h"
#include "v6/parser.h"
#include "v6/runtime.h"
#include "v6/version.h"
#include "v6/wasm.h"

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
          "wasi sandboxing (running a .wasm file directly enables WASI fully "
          "by default):\n"
          "  --no-wasi-args          don't pass CLI args through as WASI "
          "argv\n"
          "  --no-wasi-env           don't pass host environment variables "
          "through\n"
          "  --no-wasi-random        deny the random_get syscall\n"
          "  --no-wasi-clock         deny the clock_time_get syscall\n"
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
    } else if (strcmp(argv[i], "--no-wasi-args") == 0) {
      opts->wasi_deny_args = 1;
    } else if (strcmp(argv[i], "--no-wasi-env") == 0) {
      opts->wasi_deny_env = 1;
    } else if (strcmp(argv[i], "--no-wasi-random") == 0) {
      opts->wasi_deny_random = 1;
    } else if (strcmp(argv[i], "--no-wasi-clock") == 0) {
      opts->wasi_deny_clock = 1;
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
  int cacheable = strcmp(in_path, "[eval]") != 0;

  module_ctx modctx;
  buf out;
  buf* mod_bufs = NULL;

  v6_cache_result cached;
  int cache_hit = cacheable && v6_cache_try_load(in_path, &cached) == 0;

  if (cache_hit) {
    module_ctx_init(&modctx);
    modctx.count = cached.count - 1;
    buf_init(&out);
    buf_bytes(&out, cached.entries[0].data, cached.entries[0].len);
    mod_bufs =
        modctx.count > 0 ? malloc(sizeof(buf) * (size_t)modctx.count) : NULL;
    for (int i = 0; i < modctx.count; i++) {
      buf_init(&mod_bufs[i]);
      buf_bytes(&mod_bufs[i], cached.entries[i + 1].data,
                cached.entries[i + 1].len);
      snprintf(modctx.modules[i].class_name,
               sizeof(modctx.modules[i].class_name), "%s",
               cached.entries[i + 1].name);
    }
    v6_cache_free_result(&cached);
  } else {
    class_file cf;
    cf_init(&cf, "Main", "java/lang/Object");

    compile_result rc = compile_program(src, &cf, in_path, &modctx);

    if (!rc.ok) {
      int c = v6_color_enabled_err();
      fprintf(stderr, "%s%serror%s: %s:%d: %s\n", v6_c_bold(c), v6_c_red(c),
              v6_c_reset(c), in_path, rc.line, rc.message);
      cf_free(&cf);
      return 1;
    }

    buf_init(&out);
    cf_emit(&cf, &out);
    cf_free(&cf);

    mod_bufs =
        modctx.count > 0 ? malloc(sizeof(buf) * (size_t)modctx.count) : NULL;
    for (int i = 0; i < modctx.count; i++) {
      buf_init(&mod_bufs[i]);
      cf_emit(modctx.modules[i].cf, &mod_bufs[i]);
      cf_free(modctx.modules[i].cf);
      free(modctx.modules[i].cf);
    }

    if (cacheable) {
      char exe_path[1024];
      int have_exe_path = v6_get_own_exe_path(exe_path, sizeof(exe_path)) == 0;

      int tracked_count = 1 + modctx.count + (have_exe_path ? 1 : 0);
      const char** tracked_paths = malloc(sizeof(char*) * tracked_count);
      tracked_paths[0] = in_path;
      for (int i = 0; i < modctx.count; i++)
        tracked_paths[i + 1] = modctx.modules[i].abs_path;
      if (have_exe_path)
        tracked_paths[1 + modctx.count] = exe_path;

      int entry_count = 1 + modctx.count;
      const char** entry_names = malloc(sizeof(char*) * entry_count);
      const unsigned char** entry_datas =
          malloc(sizeof(unsigned char*) * entry_count);
      size_t* entry_lens = malloc(sizeof(size_t) * entry_count);
      entry_names[0] = "Main";
      entry_datas[0] = out.data;
      entry_lens[0] = out.len;
      for (int i = 0; i < modctx.count; i++) {
        entry_names[i + 1] = modctx.modules[i].class_name;
        entry_datas[i + 1] = mod_bufs[i].data;
        entry_lens[i + 1] = mod_bufs[i].len;
      }

      v6_cache_store(in_path, tracked_paths, tracked_count, entry_names,
                     entry_datas, entry_lens, entry_count);

      free(tracked_paths);
      free(entry_names);
      free(entry_datas);
      free(entry_lens);
    }
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

static int has_suffix(const char* s, const char* suffix) {
  size_t ls = strlen(s), lsuf = strlen(suffix);
  if (lsuf > ls)
    return 0;
  return strcmp(s + ls - lsuf, suffix) == 0;
}

static unsigned char* read_file_bytes(const char* path, size_t* out_len) {
  FILE* f = fopen(path, "rb");
  if (!f)
    return NULL;

  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);

  unsigned char* data = malloc((size_t)n);
  if (!data) {
    fclose(f);
    return NULL;
  }

  fread(data, 1, (size_t)n, f);
  fclose(f);
  *out_len = (size_t)n;
  return data;
}

static const char v6_b64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char* v6_base64_encode(const unsigned char* data, size_t len,
                              size_t* out_len) {
  size_t olen = ((len + 2) / 3) * 4;
  char* out = malloc(olen + 1);
  size_t i = 0, j = 0;
  while (i + 3 <= len) {
    uint32_t n =
        ((uint32_t)data[i] << 16) | ((uint32_t)data[i + 1] << 8) | data[i + 2];
    out[j++] = v6_b64_chars[(n >> 18) & 0x3F];
    out[j++] = v6_b64_chars[(n >> 12) & 0x3F];
    out[j++] = v6_b64_chars[(n >> 6) & 0x3F];
    out[j++] = v6_b64_chars[n & 0x3F];
    i += 3;
  }
  size_t rem = len - i;
  if (rem == 1) {
    uint32_t n = (uint32_t)data[i] << 16;
    out[j++] = v6_b64_chars[(n >> 18) & 0x3F];
    out[j++] = v6_b64_chars[(n >> 12) & 0x3F];
    out[j++] = '=';
    out[j++] = '=';
  } else if (rem == 2) {
    uint32_t n = ((uint32_t)data[i] << 16) | ((uint32_t)data[i + 1] << 8);
    out[j++] = v6_b64_chars[(n >> 18) & 0x3F];
    out[j++] = v6_b64_chars[(n >> 12) & 0x3F];
    out[j++] = v6_b64_chars[(n >> 6) & 0x3F];
    out[j++] = '=';
  }
  out[j] = '\0';
  *out_len = j;
  return out;
}

static void compile_wasi_cli_bootstrap(class_file* cf,
                                       const unsigned char* wasm_bytes,
                                       size_t wasm_len, int wasi_flags) {
  method* m =
      cf_method(cf, acc_public | acc_static, "main", "([Ljava/lang/String;)V");
  m->max_stack = 3;
  m->max_locals = 2;

  size_t b64_len;
  char* b64 = v6_base64_encode(wasm_bytes, wasm_len, &b64_len);

  op_emit2(m, op_invokestatic,
           cf_methodref(cf, "java/util/Base64", "getDecoder",
                        "()Ljava/util/Base64$Decoder;"));

  const size_t chunk_limit = 60000;
  size_t pos = 0;
  int first = 1;
  uint16_t concat_idx = cf_methodref(cf, "java/lang/String", "concat",
                                     "(Ljava/lang/String;)Ljava/lang/String;");
  while (pos < b64_len || first) {
    size_t take = b64_len - pos;
    if (take > chunk_limit)
      take = chunk_limit;
    char save = b64[pos + take];
    b64[pos + take] = '\0';
    uint16_t str_idx = cf_string(cf, b64 + pos);
    b64[pos + take] = save;
    op_emit2(m, op_ldc_w, str_idx);
    if (!first)
      op_emit2(m, op_invokevirtual, concat_idx);
    first = 0;
    pos += take;
  }
  free(b64);

  op_emit2(m, op_invokevirtual,
           cf_methodref(cf, "java/util/Base64$Decoder", "decode",
                        "(Ljava/lang/String;)[B"));
  emit_astore(m, 1);

  emit_aload(m, 1);
  emit_aload(m, 0);
  emit_iconst(m, wasi_flags);
  op_emit2(
      m, op_invokestatic,
      cf_methodref(cf, "V6WasiCliRunner", "run", "([B[Ljava/lang/String;I)V"));
  op_emit(m, op_return);
}

int v6_cli_run_wasm(v6_cli_options* opts) {
  size_t len = 0;
  unsigned char* bytes = read_file_bytes(opts->script_path, &len);
  if (!bytes) {
    fprintf(stderr, "error: cannot read %s\n", opts->script_path);
    return 1;
  }

  wasm_module m;
  int prc = wasm_parse_module(bytes, len, &m);
  if (prc != 0) {
    fprintf(stderr, "error: %s: %s\n", opts->script_path, m.err_msg);
    free(bytes);
    return 1;
  }
  wasm_module_free(&m);

  int wasi_flags =
      (opts->wasi_deny_args ? 0 : 1) | (opts->wasi_deny_env ? 0 : 2) |
      (opts->wasi_deny_random ? 0 : 4) | (opts->wasi_deny_clock ? 0 : 8);

  class_file cf;
  cf_init(&cf, "Main", "java/lang/Object");
  compile_wasi_cli_bootstrap(&cf, bytes, len, wasi_flags);
  free(bytes);

  buf out;
  buf_init(&out);
  cf_emit(&cf, &out);
  cf_free(&cf);

  if (!opts->classpath && !opts->no_daemon) {
    v6_daemon_class_entry classes[1];
    classes[0].name = "Main";
    classes[0].data = out.data;
    classes[0].len = out.len;

    int exit_code = 1;
    int handled =
        v6_daemon_run(opts->prog, classes, 1, opts->script_path,
                      opts->script_args, opts->script_argc,
                      v6_color_enabled_out(), &exit_code);
    if (handled) {
      buf_free(&out);
      return exit_code;
    }
  }

  if (!v6_jvm_available()) {
    fprintf(stderr, "error: no JVM available (built without JAVA_HOME?)\n");
    buf_free(&out);
    return 1;
  }

  v6_jvm* jvm = v6_jvm_create(opts->classpath, 0);
  if (!jvm) {
    fprintf(stderr, "error: failed to create JVM\n");
    buf_free(&out);
    return 1;
  }

  if (v6_jvm_load_runtime(jvm) != 0) {
    fprintf(stderr, "error: failed to load runtime\n");
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
  if (has_suffix(opts->script_path, ".wasm"))
    return v6_cli_run_wasm(opts);

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
