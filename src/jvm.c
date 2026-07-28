#include "v6/jvm.h"

#ifdef V6_HAVE_JNI

#include <jni.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
typedef HMODULE v6_lib;
#define v6_lib_open(p) LoadLibraryA(p)
#define v6_lib_sym(h, n) GetProcAddress(h, n)
static const char* v6_jvm_lib_name = "jvm.dll";
#else
#include <dlfcn.h>
typedef void* v6_lib;
#define v6_lib_open(p) dlopen(p, RTLD_NOW)
#define v6_lib_sym(h, n) dlsym(h, n)
static const char* v6_jvm_lib_name = "libjvm.so";
#endif

typedef jint(JNICALL* v6_create_vm_fn)(JavaVM**, void**, void*);

struct v6_jvm {
  JavaVM* vm;
  JNIEnv* env;
  v6_lib lib;
};

int v6_jvm_available(void) {
  return 1;
}

v6_jvm* v6_jvm_create(void) {
  v6_lib lib = v6_lib_open(v6_jvm_lib_name);
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

void v6_jvm_destroy(v6_jvm* jvm) {
  if (!jvm)
    return;
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

void v6_jvm_destroy(v6_jvm* jvm) {
  (void)jvm;
}

#endif
