#include "v6/jvm.h"
#include "v6/runtime.h"

#ifdef V6_HAVE_JNI

#include <jni.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
typedef HMODULE v6_lib;
#define v6_lib_open(p) LoadLibraryA(p)
#define v6_lib_sym(h, n) GetProcAddress(h, n)
static const char* v6_jvm_lib_name = "jvm.dll";
static const char* v6_jvm_lib_rel = "bin/server/jvm.dll";
#elif defined(__APPLE__)
#include <dlfcn.h>
typedef void* v6_lib;
#define v6_lib_open(p) dlopen(p, RTLD_NOW)
#define v6_lib_sym(h, n) dlsym(h, n)
static const char* v6_jvm_lib_name = "libjvm.dylib";
static const char* v6_jvm_lib_rel = "lib/server/libjvm.dylib";
#else
#include <dlfcn.h>
typedef void* v6_lib;
#define v6_lib_open(p) dlopen(p, RTLD_NOW)
#define v6_lib_sym(h, n) dlsym(h, n)
static const char* v6_jvm_lib_name = "libjvm.so";
static const char* v6_jvm_lib_rel = "lib/server/libjvm.so";
#endif

static v6_lib v6_open_jvm_lib(void) {
  v6_lib lib = v6_lib_open(v6_jvm_lib_name);
  if (lib)
    return lib;

  const char* home = getenv("JAVA_HOME");
  if (!home)
    return NULL;

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
};

int v6_jvm_available(void) {
  return 1;
}

v6_jvm* v6_jvm_create(void) {
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

  JavaVMInitArgs args;
  args.version = JNI_VERSION_1_8;
  args.nOptions = 0;
  args.options = NULL;
  args.ignoreUnrecognized = JNI_FALSE;

  jint rc = sym.fn(&jvm->vm, (void**)&jvm->env, &args);
  if (rc != JNI_OK) {
    free(jvm);
    return NULL;
  }
  return jvm;
}

int v6_jvm_load_runtime(v6_jvm* jvm) {
  JNIEnv* env = jvm->env;

  jclass cls =
      (*env)->DefineClass(env, "V6Value", NULL, (const jbyte*)v6_runtime_class,
                          (jsize)v6_runtime_class_len);
  if (!cls)
    return -1;

  jmethodID ctor =
      (*env)->GetMethodID(env, cls, "<init>", "(IDLjava/lang/Object;)V");
  if (!ctor)
    return -1;

  jobject probe = (*env)->NewObject(env, cls, ctor, 0, 0.0, NULL);
  if (!probe)
    return -1;

  jmethodID tag_m = (*env)->GetMethodID(env, cls, "tag", "()I");
  if (!tag_m)
    return -1;

  if ((*env)->CallIntMethod(env, probe, tag_m) != 0)
    return -1;

  jvm->value_class = (jclass)(*env)->NewGlobalRef(env, cls);
  return jvm->value_class ? 0 : -1;
}

void v6_jvm_destroy(v6_jvm* jvm) {
  if (!jvm)
    return;
  if (jvm->value_class)
    (*jvm->env)->DeleteGlobalRef(jvm->env, jvm->value_class);
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

v6_jvm* v6_jvm_create(void) {
  return NULL;
}

int v6_jvm_load_runtime(v6_jvm* jvm) {
  (void)jvm;
  return -1;
}

void v6_jvm_destroy(v6_jvm* jvm) {
  (void)jvm;
}

#endif
