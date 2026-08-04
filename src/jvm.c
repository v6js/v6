#if !defined(_WIN32) && !defined(__APPLE__) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE
#endif

#include "v6/jvm.h"
#include "v6/runtime.h"

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

v6_jvm* v6_jvm_create(const char* classpath) {
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

  JavaVMOption opts[2];
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

int v6_jvm_load_runtime(v6_jvm* jvm) {
  JNIEnv* env = jvm->env;
  jclass value_cls = NULL;

  for (size_t i = 0; i < v6_runtime_class_count; i++) {
    jclass cls =
        (*env)->DefineClass(env, v6_runtime_classes[i].name, jvm->loader,
                            (const jbyte*)v6_runtime_classes[i].data,
                            (jsize)v6_runtime_classes[i].len);
    if (!cls)
      return -1;
    if (strcmp(v6_runtime_classes[i].name, "V6Value") == 0)
      value_cls = cls;
  }

  if (!value_cls)
    return -1;

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

int v6_jvm_run(v6_jvm* jvm, const unsigned char* class_bytes, size_t len,
               char** script_args, int script_argc) {
  JNIEnv* env = jvm->env;

  jclass cls = (*env)->DefineClass(env, "Main", jvm->loader,
                                   (const jbyte*)class_bytes, (jsize)len);
  if (!cls)
    return -1;

  jmethodID main_m =
      (*env)->GetStaticMethodID(env, cls, "main", "([Ljava/lang/String;)V");
  if (!main_m)
    return -1;

  jclass str_cls = (*env)->FindClass(env, "java/lang/String");
  if (!str_cls)
    return -1;

  jobjectArray args =
      (*env)->NewObjectArray(env, (jsize)script_argc, str_cls, NULL);
  if (!args)
    return -1;

  for (int i = 0; i < script_argc; i++) {
    jstring s = (*env)->NewStringUTF(env, script_args[i]);
    (*env)->SetObjectArrayElement(env, args, i, s);
  }

  (*env)->CallStaticVoidMethod(env, cls, main_m, args);
  if ((*env)->ExceptionCheck(env)) {
    jthrowable exc = (*env)->ExceptionOccurred(env);
    (*env)->ExceptionClear(env);
    jclass throw_cls = (*env)->FindClass(env, "V6Throw");
    if (throw_cls && (*env)->IsInstanceOf(env, exc, throw_cls)) {
      jfieldID value_fld =
          (*env)->GetFieldID(env, throw_cls, "value", "LV6Value;");
      jobject value =
          value_fld ? (*env)->GetObjectField(env, exc, value_fld) : NULL;
      jclass value_cls = value ? (*env)->GetObjectClass(env, value) : NULL;
      jmethodID to_string =
          value_cls ? (*env)->GetMethodID(env, value_cls, "toString",
                                          "()Ljava/lang/String;")
                    : NULL;
      jstring msg =
          to_string ? (jstring)(*env)->CallObjectMethod(env, value, to_string)
                    : NULL;
      if (msg) {
        const char* msg_c = (*env)->GetStringUTFChars(env, msg, NULL);
        fprintf(stderr, "Uncaught %s\n", msg_c);
        (*env)->ReleaseStringUTFChars(env, msg, msg_c);
      } else {
        fprintf(stderr, "Uncaught exception (V6Throw)\n");
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

v6_jvm* v6_jvm_create(const char* classpath) {
  (void)classpath;
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

void v6_jvm_destroy(v6_jvm* jvm) {
  (void)jvm;
}

#endif
