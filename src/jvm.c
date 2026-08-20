#if !defined(_WIN32) && !defined(__APPLE__) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE
#endif

#include "v6/color.h"
#include "v6/jvm.h"
#include "v6/runtime.h"
#include "v6/wasm.h"

#ifdef V6_HAVE_JNI

#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
typedef HMODULE v6_lib;
#define v6_lib_open(p) LoadLibraryA(p)
#define v6_lib_sym(h, n) GetProcAddress(h, n)
static const char* v6_jvm_lib_name = "jvm.dll";
static const char* v6_jvm_lib_rel = "bin/server/jvm.dll";
#elif defined(__APPLE__)
#include <dlfcn.h>
#include <mach-o/dyld.h>
typedef void* v6_lib;
#define v6_lib_open(p) dlopen(p, RTLD_NOW)
#define v6_lib_sym(h, n) dlsym(h, n)
static const char* v6_jvm_lib_name = "libjvm.dylib";
static const char* v6_jvm_lib_rel = "lib/server/libjvm.dylib";
#else
#include <dlfcn.h>
#include <unistd.h>
typedef void* v6_lib;
#define v6_lib_open(p) dlopen(p, RTLD_NOW)
#define v6_lib_sym(h, n) dlsym(h, n)
static const char* v6_jvm_lib_name = "libjvm.so";
static const char* v6_jvm_lib_rel = "lib/server/libjvm.so";
#endif

static int v6_get_exe_dir(char* out, size_t out_size) {
  char path[1024];
#ifdef _WIN32
  DWORD n = GetModuleFileNameA(NULL, path, sizeof(path));
  if (n == 0 || n >= sizeof(path))
    return -1;
#elif defined(__APPLE__)
  uint32_t size = sizeof(path);
  if (_NSGetExecutablePath(path, &size) != 0)
    return -1;
#else
  ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
  if (n < 0)
    return -1;
  path[n] = '\0';
#endif

  char* last_slash = strrchr(path, '/');
  char* last_backslash = strrchr(path, '\\');
  char* cut = last_slash;
  if (last_backslash && (!cut || last_backslash > cut))
    cut = last_backslash;
  if (!cut)
    return -1;

  size_t len = (size_t)(cut - path);
  if (len >= out_size)
    return -1;
  memcpy(out, path, len);
  out[len] = '\0';
  return 0;
}

#ifdef _WIN32
static void v6_prep_dll_search(const char* root) {
  char bin_dir[1024];
  snprintf(bin_dir, sizeof(bin_dir), "%s/bin", root);
  SetDllDirectoryA(bin_dir);
}
#else
static void v6_prep_dll_search(const char* root) {
  (void)root;
}
#endif

static v6_lib v6_open_jvm_lib(void) {
  v6_lib lib = v6_lib_open(v6_jvm_lib_name);
  if (lib)
    return lib;

  char exe_dir[1024];
  if (v6_get_exe_dir(exe_dir, sizeof(exe_dir)) == 0) {
    char root[1024];
    snprintf(root, sizeof(root), "%s/jdk", exe_dir);
    v6_prep_dll_search(root);
    char path[1200];
    snprintf(path, sizeof(path), "%s/%s", root, v6_jvm_lib_rel);
    lib = v6_lib_open(path);
    if (lib)
      return lib;
  }

  const char* home = getenv("JAVA_HOME");
  if (!home)
    return NULL;

  v6_prep_dll_search(home);
  char path[1024];
  snprintf(path, sizeof(path), "%s/%s", home, v6_jvm_lib_rel);
  return v6_lib_open(path);
}

static int v6_parse_release_file(const char* root, char* out, size_t out_cap) {
  char path[1200];
  snprintf(path, sizeof(path), "%s/release", root);
  FILE* f = fopen(path, "rb");
  if (!f)
    return -1;
  char line[256];
  int found = -1;
  while (fgets(line, sizeof(line), f)) {
    if (strncmp(line, "JAVA_VERSION=", 13) != 0)
      continue;
    char* start = strchr(line, '"');
    if (!start)
      break;
    start++;
    char* end = strchr(start, '"');
    if (!end)
      break;
    size_t n = (size_t)(end - start);
    if (n >= out_cap)
      n = out_cap - 1;
    memcpy(out, start, n);
    out[n] = '\0';
    found = 0;
    break;
  }
  fclose(f);
  return found;
}

int v6_jvm_detect_version(char* out, size_t out_cap) {
  char exe_dir[1024];
  if (v6_get_exe_dir(exe_dir, sizeof(exe_dir)) == 0) {
    char root[1024];
    snprintf(root, sizeof(root), "%s/jdk", exe_dir);
    if (v6_parse_release_file(root, out, out_cap) == 0)
      return 0;
  }
  const char* home = getenv("JAVA_HOME");
  if (home && v6_parse_release_file(home, out, out_cap) == 0)
    return 0;
  return -1;
}

typedef jint(JNICALL* v6_create_vm_fn)(JavaVM**, void**, void*);

struct v6_jvm {
  JavaVM* vm;
  JNIEnv* env;
  v6_lib lib;
  jclass value_class;
  jobject loader;
};

int v6_jvm_available(void) {
  return 1;
}

v6_jvm* v6_jvm_create(const char* classpath, int is_daemon) {
  v6_lib lib = v6_open_jvm_lib();
  if (!lib)
    return NULL;

  union {
    void* obj;
    v6_create_vm_fn fn;
  } sym;

  sym.obj = (void*)v6_lib_sym(lib, "JNI_CreateJavaVM");
  if (!sym.fn)
    return NULL;

  v6_jvm* jvm = malloc(sizeof(v6_jvm));
  if (!jvm)
    return NULL;
  jvm->lib = lib;
  jvm->value_class = NULL;
  jvm->loader = NULL;

  JavaVMInitArgs args;
  args.version = JNI_VERSION_1_8;
  args.ignoreUnrecognized = JNI_TRUE;

  JavaVMOption opts[8];
  int nopts = 0;
  char* cp_opt = NULL;
  if (classpath && classpath[0] != '\0') {
    size_t n = strlen(classpath) + 32;
    cp_opt = malloc(n);
    snprintf(cp_opt, n, "-Djava.class.path=%s", classpath);
    opts[nopts].optionString = cp_opt;
    opts[nopts].extraInfo = NULL;
    nopts++;
  }
  opts[nopts].optionString = "-Dsun.java2d.dpiaware=true";
  opts[nopts].extraInfo = NULL;
  nopts++;
  if (is_daemon) {
    opts[nopts].optionString = "-XX:+UseG1GC";
    opts[nopts].extraInfo = NULL;
    nopts++;
  } else {
    opts[nopts].optionString = "-XX:+UseSerialGC";
    opts[nopts].extraInfo = NULL;
    nopts++;
    opts[nopts].optionString = "-XX:TieredStopAtLevel=1";
    opts[nopts].extraInfo = NULL;
    nopts++;
    opts[nopts].optionString = "-XX:CICompilerCount=1";
    opts[nopts].extraInfo = NULL;
    nopts++;
  }
  opts[nopts].optionString = "-Xshare:auto";
  opts[nopts].extraInfo = NULL;
  nopts++;
  args.nOptions = nopts;
  args.options = opts;

  jint rc = sym.fn(&jvm->vm, (void**)&jvm->env, &args);
  free(cp_opt);
  if (rc != JNI_OK) {
    free(jvm);
    return NULL;
  }

  JNIEnv* env = jvm->env;
  jclass loader_cls = (*env)->FindClass(env, "java/lang/ClassLoader");
  if (loader_cls) {
    jmethodID get_sys = (*env)->GetStaticMethodID(
        env, loader_cls, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
    if (get_sys) {
      jobject sys_loader =
          (*env)->CallStaticObjectMethod(env, loader_cls, get_sys);
      if (sys_loader)
        jvm->loader = (*env)->NewGlobalRef(env, sys_loader);
    }
  }

  return jvm;
}

static jbyteArray JNICALL v6_wasm_compile_native(JNIEnv* env, jclass cls,
                                                 jbyteArray wasm_bytes,
                                                 jstring class_name) {
  (void)cls;
  jsize len = (*env)->GetArrayLength(env, wasm_bytes);
  jbyte* bytes = (*env)->GetByteArrayElements(env, wasm_bytes, NULL);

  wasm_module m;
  int rc = wasm_parse_module((const uint8_t*)bytes, (size_t)len, &m);
  if (rc != 0) {
    char msg[300];
    snprintf(msg, sizeof(msg), "%s", m.err_msg);
    (*env)->ReleaseByteArrayElements(env, wasm_bytes, bytes, JNI_ABORT);
    (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/RuntimeException"),
                     msg);
    return NULL;
  }

  const char* cname = (*env)->GetStringUTFChars(env, class_name, NULL);
  class_file cf;
  cf_init(&cf, cname, "java/lang/Object");
  compile_result cr = wasm_compile_module(&m, &cf, cname);
  (*env)->ReleaseStringUTFChars(env, class_name, cname);
  (*env)->ReleaseByteArrayElements(env, wasm_bytes, bytes, JNI_ABORT);

  if (!cr.ok) {
    char msg[1024];
    snprintf(msg, sizeof(msg), "%s", cr.message);
    wasm_module_free(&m);
    cf_free(&cf);
    (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/RuntimeException"),
                     msg);
    return NULL;
  }

  buf out;
  buf_init(&out);
  cf_emit(&cf, &out);

  jbyteArray result = (*env)->NewByteArray(env, (jsize)out.len);
  (*env)->SetByteArrayRegion(env, result, 0, (jsize)out.len,
                             (const jbyte*)out.data);

  buf_free(&out);
  cf_free(&cf);
  wasm_module_free(&m);
  return result;
}

static jstring JNICALL v6_wasm_describe_exports_native(JNIEnv* env, jclass cls,
                                                       jbyteArray wasm_bytes) {
  (void)cls;
  jsize len = (*env)->GetArrayLength(env, wasm_bytes);
  jbyte* bytes = (*env)->GetByteArrayElements(env, wasm_bytes, NULL);

  wasm_module m;
  int rc = wasm_parse_module((const uint8_t*)bytes, (size_t)len, &m);
  (*env)->ReleaseByteArrayElements(env, wasm_bytes, bytes, JNI_ABORT);

  if (rc != 0) {
    char msg[300];
    snprintf(msg, sizeof(msg), "%s", m.err_msg);
    (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/RuntimeException"),
                     msg);
    return NULL;
  }

  buf text;
  buf_init(&text);
  for (uint32_t i = 0; i < m.export_count; i++) {
    if (m.exports[i].kind != wasm_import_func)
      continue;
    uint32_t fidx = m.exports[i].index;
    const wasm_functype* ft = wasm_func_type(&m, fidx);
    if (!ft)
      continue;
    char desc[512];
    wasm_build_func_desc(ft, desc, sizeof(desc));
    char line[600];
    int n = snprintf(line, sizeof(line), "%s\t%u\t%s\n", m.exports[i].name,
                     fidx, desc);
    if (n > 0)
      buf_bytes(&text, (const uint8_t*)line, (size_t)n);
  }
  buf_u8(&text, 0);

  jstring result = (*env)->NewStringUTF(env, (const char*)text.data);
  buf_free(&text);
  wasm_module_free(&m);
  return result;
}

static jstring JNICALL v6_wasm_describe_imports_native(JNIEnv* env, jclass cls,
                                                       jbyteArray wasm_bytes) {
  (void)cls;
  jsize len = (*env)->GetArrayLength(env, wasm_bytes);
  jbyte* bytes = (*env)->GetByteArrayElements(env, wasm_bytes, NULL);

  wasm_module m;
  int rc = wasm_parse_module((const uint8_t*)bytes, (size_t)len, &m);
  (*env)->ReleaseByteArrayElements(env, wasm_bytes, bytes, JNI_ABORT);

  if (rc != 0) {
    char msg[300];
    snprintf(msg, sizeof(msg), "%s", m.err_msg);
    (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/RuntimeException"),
                     msg);
    return NULL;
  }

  buf text;
  buf_init(&text);
  uint32_t fidx = 0;
  for (uint32_t i = 0; i < m.import_count; i++) {
    if (m.imports[i].kind != wasm_import_func)
      continue;
    char line[600];
    int n = snprintf(line, sizeof(line), "%s\t%s\t%u\n",
                     m.imports[i].module_name, m.imports[i].field_name, fidx);
    if (n > 0)
      buf_bytes(&text, (const uint8_t*)line, (size_t)n);
    fidx++;
  }
  buf_u8(&text, 0);

  jstring result = (*env)->NewStringUTF(env, (const char*)text.data);
  buf_free(&text);
  wasm_module_free(&m);
  return result;
}

int v6_jvm_load_runtime(v6_jvm* jvm) {
  JNIEnv* env = jvm->env;
  jclass value_cls = NULL;
  jclass wasm_compiler_cls = NULL;

  for (size_t i = 0; i < v6_runtime_class_count; i++) {
    jclass cls =
        (*env)->DefineClass(env, v6_runtime_classes[i].name, jvm->loader,
                            (const jbyte*)v6_runtime_classes[i].data,
                            (jsize)v6_runtime_classes[i].len);
    if (!cls)
      return -1;
    if (strcmp(v6_runtime_classes[i].name, "V6Value") == 0)
      value_cls = cls;
    if (strcmp(v6_runtime_classes[i].name, "V6WasmCompiler") == 0)
      wasm_compiler_cls = cls;
  }

  if (!value_cls)
    return -1;

  if (wasm_compiler_cls) {
    JNINativeMethod methods[3];
    methods[0].name = "compile";
    methods[0].signature = "([B"
                           "Ljava/lang/String;"
                           ")[B";
    methods[0].fnPtr = (void*)v6_wasm_compile_native;
    methods[1].name = "describeExports";
    methods[1].signature = "([B)Ljava/lang/String;";
    methods[1].fnPtr = (void*)v6_wasm_describe_exports_native;
    methods[2].name = "describeImports";
    methods[2].signature = "([B)Ljava/lang/String;";
    methods[2].fnPtr = (void*)v6_wasm_describe_imports_native;
    if ((*env)->RegisterNatives(env, wasm_compiler_cls, methods, 3) != 0)
      return -1;
  }

  jmethodID ctor =
      (*env)->GetMethodID(env, value_cls, "<init>", "(IDLjava/lang/Object;)V");
  if (!ctor)
    return -1;

  jobject probe = (*env)->NewObject(env, value_cls, ctor, 0, 0.0, NULL);
  if (!probe)
    return -1;

  jmethodID tag_m = (*env)->GetMethodID(env, value_cls, "tag", "()I");
  if (!tag_m)
    return -1;

  if ((*env)->CallIntMethod(env, probe, tag_m) != 0)
    return -1;

  jvm->value_class = (jclass)(*env)->NewGlobalRef(env, value_cls);
  return jvm->value_class ? 0 : -1;
}

int v6_jvm_define_extra(v6_jvm* jvm, const char* name,
                        const unsigned char* class_bytes, size_t len) {
  JNIEnv* env = jvm->env;
  jclass cls = (*env)->DefineClass(env, name, jvm->loader,
                                   (const jbyte*)class_bytes, (jsize)len);
  if (!cls) {
    if ((*env)->ExceptionCheck(env)) {
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
    }
    return -1;
  }
  return 0;
}

int v6_jvm_call_static_i_ii(v6_jvm* jvm, const char* class_name,
                            const char* method_name, int a, int b, int* out) {
  JNIEnv* env = jvm->env;
  jclass cls = (*env)->FindClass(env, class_name);
  if (!cls) {
    if ((*env)->ExceptionCheck(env)) {
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
    }
    return -1;
  }
  jmethodID m = (*env)->GetStaticMethodID(env, cls, method_name, "(II)I");
  if (!m) {
    if ((*env)->ExceptionCheck(env)) {
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
    }
    return -1;
  }
  jint result = (*env)->CallStaticIntMethod(env, cls, m, (jint)a, (jint)b);
  if ((*env)->ExceptionCheck(env)) {
    (*env)->ExceptionDescribe(env);
    (*env)->ExceptionClear(env);
    return -1;
  }
  *out = (int)result;
  return 0;
}

int v6_jvm_call_static_i(v6_jvm* jvm, const char* class_name,
                         const char* method_name, int* out) {
  JNIEnv* env = jvm->env;
  jclass cls = (*env)->FindClass(env, class_name);
  if (!cls) {
    if ((*env)->ExceptionCheck(env)) {
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
    }
    return -1;
  }
  jmethodID m = (*env)->GetStaticMethodID(env, cls, method_name, "()I");
  if (!m) {
    if ((*env)->ExceptionCheck(env)) {
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
    }
    return -1;
  }
  jint result = (*env)->CallStaticIntMethod(env, cls, m);
  if ((*env)->ExceptionCheck(env)) {
    (*env)->ExceptionDescribe(env);
    (*env)->ExceptionClear(env);
    return -1;
  }
  *out = (int)result;
  return 0;
}

int v6_jvm_call_static_i_iii(v6_jvm* jvm, const char* class_name,
                             const char* method_name, int a, int b, int c,
                             int* out) {
  JNIEnv* env = jvm->env;
  jclass cls = (*env)->FindClass(env, class_name);
  if (!cls) {
    if ((*env)->ExceptionCheck(env)) {
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
    }
    return -1;
  }
  jmethodID m = (*env)->GetStaticMethodID(env, cls, method_name, "(III)I");
  if (!m) {
    if ((*env)->ExceptionCheck(env)) {
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
    }
    return -1;
  }
  jint result =
      (*env)->CallStaticIntMethod(env, cls, m, (jint)a, (jint)b, (jint)c);
  if ((*env)->ExceptionCheck(env)) {
    (*env)->ExceptionDescribe(env);
    (*env)->ExceptionClear(env);
    return -1;
  }
  *out = (int)result;
  return 0;
}

int v6_jvm_wasm_compile(v6_jvm* jvm, const unsigned char* wasm_bytes,
                        size_t wasm_len, const char* class_name,
                        unsigned char** out_bytes, size_t* out_len) {
  JNIEnv* env = jvm->env;
  jclass cls = (*env)->FindClass(env, "V6WasmCompiler");
  if (!cls) {
    if ((*env)->ExceptionCheck(env)) {
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
    }
    return -1;
  }
  jmethodID m = (*env)->GetStaticMethodID(env, cls, "compile",
                                          "([BLjava/lang/String;)[B");
  if (!m) {
    if ((*env)->ExceptionCheck(env)) {
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
    }
    return -1;
  }

  jbyteArray jbytes = (*env)->NewByteArray(env, (jsize)wasm_len);
  (*env)->SetByteArrayRegion(env, jbytes, 0, (jsize)wasm_len,
                             (const jbyte*)wasm_bytes);
  jstring jname = (*env)->NewStringUTF(env, class_name);

  jbyteArray result =
      (jbyteArray)(*env)->CallStaticObjectMethod(env, cls, m, jbytes, jname);
  if ((*env)->ExceptionCheck(env)) {
    (*env)->ExceptionDescribe(env);
    (*env)->ExceptionClear(env);
    return -1;
  }
  if (!result)
    return -1;

  jsize rlen = (*env)->GetArrayLength(env, result);
  jbyte* rbytes = (*env)->GetByteArrayElements(env, result, NULL);
  unsigned char* copy = malloc((size_t)rlen);
  memcpy(copy, rbytes, (size_t)rlen);
  (*env)->ReleaseByteArrayElements(env, result, rbytes, JNI_ABORT);

  *out_bytes = copy;
  *out_len = (size_t)rlen;
  return 0;
}

int v6_jvm_run(v6_jvm* jvm, const unsigned char* class_bytes, size_t len,
               char** script_args, int script_argc) {
  JNIEnv* env = jvm->env;

  jclass cls = (*env)->DefineClass(env, "Main", jvm->loader,
                                   (const jbyte*)class_bytes, (jsize)len);
  if (!cls) {
    fprintf(stderr, "error: failed to load compiled program\n");
    return -1;
  }

  jmethodID main_m =
      (*env)->GetStaticMethodID(env, cls, "main", "([Ljava/lang/String;)V");
  if (!main_m) {
    if ((*env)->ExceptionCheck(env)) {
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
    }
    fprintf(stderr, "error: compiled program has no entry point\n");
    return -1;
  }

  jclass str_cls = (*env)->FindClass(env, "java/lang/String");
  if (!str_cls) {
    fprintf(stderr, "error: internal JVM setup failure\n");
    return -1;
  }

  jobjectArray args =
      (*env)->NewObjectArray(env, (jsize)script_argc, str_cls, NULL);
  if (!args) {
    fprintf(stderr, "error: internal JVM setup failure\n");
    return -1;
  }

  for (int i = 0; i < script_argc; i++) {
    jstring s = (*env)->NewStringUTF(env, script_args[i]);
    (*env)->SetObjectArrayElement(env, args, i, s);
  }

  jclass builtins_cls = (*env)->FindClass(env, "V6Builtins");
  if ((*env)->ExceptionCheck(env))
    (*env)->ExceptionClear(env);
  if (builtins_cls) {
    jmethodID set_color =
        (*env)->GetStaticMethodID(env, builtins_cls, "setColorEnabled", "(Z)V");
    if (set_color)
      (*env)->CallStaticVoidMethod(env, builtins_cls, set_color,
                                   (jboolean)v6_color_enabled_out());
    if ((*env)->ExceptionCheck(env))
      (*env)->ExceptionClear(env);
  }

  (*env)->CallStaticVoidMethod(env, cls, main_m, args);
  if ((*env)->ExceptionCheck(env)) {
    jthrowable exc = (*env)->ExceptionOccurred(env);
    (*env)->ExceptionClear(env);

    jclass exit_cls = (*env)->FindClass(env, "V6ProcessExit");
    if ((*env)->ExceptionCheck(env))
      (*env)->ExceptionClear(env);
    if (exit_cls && (*env)->IsInstanceOf(env, exc, exit_cls)) {
      jfieldID code_fld = (*env)->GetFieldID(env, exit_cls, "code", "I");
      jint code = code_fld ? (*env)->GetIntField(env, exc, code_fld) : 0;
      return (int)code;
    }
    if ((*env)->ExceptionCheck(env))
      (*env)->ExceptionClear(env);

    jclass throw_cls = (*env)->FindClass(env, "V6Throw");
    if ((*env)->ExceptionCheck(env))
      (*env)->ExceptionClear(env);
    if (throw_cls && (*env)->IsInstanceOf(env, exc, throw_cls)) {
      jfieldID value_fld =
          (*env)->GetFieldID(env, throw_cls, "value", "LV6Value;");
      jobject value =
          value_fld ? (*env)->GetObjectField(env, exc, value_fld) : NULL;
      jmethodID format_m =
          value ? (*env)->GetStaticMethodID(env, throw_cls, "formatUncaught",
                                            "(LV6Value;)Ljava/lang/String;")
                : NULL;
      jstring msg = format_m ? (jstring)(*env)->CallStaticObjectMethod(
                                   env, throw_cls, format_m, value)
                             : NULL;
      int c = v6_color_enabled_err();
      if (msg) {
        const char* msg_c = (*env)->GetStringUTFChars(env, msg, NULL);
        fprintf(stderr, "%s%sUncaught%s %s\n", v6_c_bold(c), v6_c_red(c),
                v6_c_reset(c), msg_c);
        (*env)->ReleaseStringUTFChars(env, msg, msg_c);
      } else {
        fprintf(stderr, "%s%sUncaught exception (V6Throw)%s\n", v6_c_bold(c),
                v6_c_red(c), v6_c_reset(c));
      }
      (*env)->ExceptionClear(env);
    } else {
      (*env)->Throw(env, exc);
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
    }
    return -1;
  }

  return 0;
}

int v6_jvm_serve_daemon(v6_jvm* jvm, const char* lock_file_path,
                        long idle_timeout_ms, long long binary_mtime,
                        long long binary_size, long long execution_timeout_ms) {
  JNIEnv* env = jvm->env;
  jclass daemon_cls = (*env)->FindClass(env, "V6Daemon");
  if (!daemon_cls) {
    if ((*env)->ExceptionCheck(env)) {
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
    }
    return -1;
  }
  jmethodID serve_m = (*env)->GetStaticMethodID(env, daemon_cls, "serve",
                                                "(Ljava/lang/String;JJJJ)V");
  if (!serve_m) {
    if ((*env)->ExceptionCheck(env)) {
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
    }
    return -1;
  }
  jstring lock_str = (*env)->NewStringUTF(env, lock_file_path);
  (*env)->CallStaticVoidMethod(env, daemon_cls, serve_m, lock_str,
                               (jlong)idle_timeout_ms, (jlong)binary_mtime,
                               (jlong)binary_size, (jlong)execution_timeout_ms);
  if ((*env)->ExceptionCheck(env)) {
    (*env)->ExceptionDescribe(env);
    (*env)->ExceptionClear(env);
    return -1;
  }
  return 0;
}

void v6_jvm_destroy(v6_jvm* jvm) {
  if (!jvm)
    return;
  if (jvm->value_class)
    (*jvm->env)->DeleteGlobalRef(jvm->env, jvm->value_class);
  if (jvm->loader)
    (*jvm->env)->DeleteGlobalRef(jvm->env, jvm->loader);
  (*jvm->vm)->DestroyJavaVM(jvm->vm);
  free(jvm);
}

#else

#include <stddef.h>

struct v6_jvm {
  int unused;
};

int v6_jvm_available(void) {
  return 0;
}

int v6_jvm_detect_version(char* out, size_t out_cap) {
  (void)out;
  (void)out_cap;
  return -1;
}

v6_jvm* v6_jvm_create(const char* classpath, int is_daemon) {
  (void)classpath;
  (void)is_daemon;
  return NULL;
}

int v6_jvm_load_runtime(v6_jvm* jvm) {
  (void)jvm;
  return -1;
}

int v6_jvm_define_extra(v6_jvm* jvm, const char* name,
                        const unsigned char* class_bytes, size_t len) {
  (void)jvm;
  (void)name;
  (void)class_bytes;
  (void)len;
  return -1;
}

int v6_jvm_run(v6_jvm* jvm, const unsigned char* class_bytes, size_t len,
               char** script_args, int script_argc) {
  (void)jvm;
  (void)class_bytes;
  (void)len;
  (void)script_args;
  (void)script_argc;
  return -1;
}

int v6_jvm_call_static_i_ii(v6_jvm* jvm, const char* class_name,
                            const char* method_name, int a, int b, int* out) {
  (void)jvm;
  (void)class_name;
  (void)method_name;
  (void)a;
  (void)b;
  (void)out;
  return -1;
}

int v6_jvm_call_static_i(v6_jvm* jvm, const char* class_name,
                         const char* method_name, int* out) {
  (void)jvm;
  (void)class_name;
  (void)method_name;
  (void)out;
  return -1;
}

int v6_jvm_call_static_i_iii(v6_jvm* jvm, const char* class_name,
                             const char* method_name, int a, int b, int c,
                             int* out) {
  (void)jvm;
  (void)class_name;
  (void)method_name;
  (void)a;
  (void)b;
  (void)c;
  (void)out;
  return -1;
}

int v6_jvm_wasm_compile(v6_jvm* jvm, const unsigned char* wasm_bytes,
                        size_t wasm_len, const char* class_name,
                        unsigned char** out_bytes, size_t* out_len) {
  (void)jvm;
  (void)wasm_bytes;
  (void)wasm_len;
  (void)class_name;
  (void)out_bytes;
  (void)out_len;
  return -1;
}

int v6_jvm_serve_daemon(v6_jvm* jvm, const char* lock_file_path,
                        long idle_timeout_ms, long long binary_mtime,
                        long long binary_size, long long execution_timeout_ms) {
  (void)jvm;
  (void)lock_file_path;
  (void)idle_timeout_ms;
  (void)binary_mtime;
  (void)binary_size;
  (void)execution_timeout_ms;
  return -1;
}

void v6_jvm_destroy(v6_jvm* jvm) {
  (void)jvm;
}

#endif
