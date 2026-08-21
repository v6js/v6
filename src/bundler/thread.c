#ifndef _WIN32
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#endif

#include "v6/bundler_thread.h"

#include <stdlib.h>

#ifdef _WIN32

#include <windows.h>

struct v6_bundler_thread {
  HANDLE handle;
};

struct v6_bundler_mutex {
  CRITICAL_SECTION cs;
};

typedef struct thread_trampoline_arg {
  void (*fn)(void*);
  void* arg;
} thread_trampoline_arg;

static DWORD WINAPI thread_trampoline(LPVOID p) {
  thread_trampoline_arg* t = (thread_trampoline_arg*)p;
  t->fn(t->arg);
  free(t);
  return 0;
}

v6_bundler_thread* v6_bundler_thread_start(void (*fn)(void* arg), void* arg) {
  v6_bundler_thread* t = malloc(sizeof(v6_bundler_thread));
  thread_trampoline_arg* ta = malloc(sizeof(thread_trampoline_arg));
  ta->fn = fn;
  ta->arg = arg;
  t->handle = CreateThread(NULL, 0, thread_trampoline, ta, 0, NULL);
  return t;
}

void v6_bundler_thread_join(v6_bundler_thread* t) {
  if (!t)
    return;
  WaitForSingleObject(t->handle, INFINITE);
  CloseHandle(t->handle);
  free(t);
}

v6_bundler_mutex* v6_bundler_mutex_create(void) {
  v6_bundler_mutex* m = malloc(sizeof(v6_bundler_mutex));
  InitializeCriticalSection(&m->cs);
  return m;
}

void v6_bundler_mutex_lock(v6_bundler_mutex* m) {
  EnterCriticalSection(&m->cs);
}

void v6_bundler_mutex_unlock(v6_bundler_mutex* m) {
  LeaveCriticalSection(&m->cs);
}

void v6_bundler_mutex_free(v6_bundler_mutex* m) {
  if (!m)
    return;
  DeleteCriticalSection(&m->cs);
  free(m);
}

#else

#include <pthread.h>

struct v6_bundler_thread {
  pthread_t handle;
};

struct v6_bundler_mutex {
  pthread_mutex_t mu;
};

typedef struct thread_trampoline_arg {
  void (*fn)(void*);
  void* arg;
} thread_trampoline_arg;

static void* thread_trampoline(void* p) {
  thread_trampoline_arg* t = (thread_trampoline_arg*)p;
  t->fn(t->arg);
  free(t);
  return NULL;
}

v6_bundler_thread* v6_bundler_thread_start(void (*fn)(void* arg), void* arg) {
  v6_bundler_thread* t = malloc(sizeof(v6_bundler_thread));
  thread_trampoline_arg* ta = malloc(sizeof(thread_trampoline_arg));
  ta->fn = fn;
  ta->arg = arg;
  pthread_create(&t->handle, NULL, thread_trampoline, ta);
  return t;
}

void v6_bundler_thread_join(v6_bundler_thread* t) {
  if (!t)
    return;
  pthread_join(t->handle, NULL);
  free(t);
}

v6_bundler_mutex* v6_bundler_mutex_create(void) {
  v6_bundler_mutex* m = malloc(sizeof(v6_bundler_mutex));
  pthread_mutex_init(&m->mu, NULL);
  return m;
}

void v6_bundler_mutex_lock(v6_bundler_mutex* m) {
  pthread_mutex_lock(&m->mu);
}

void v6_bundler_mutex_unlock(v6_bundler_mutex* m) {
  pthread_mutex_unlock(&m->mu);
}

void v6_bundler_mutex_free(v6_bundler_mutex* m) {
  if (!m)
    return;
  pthread_mutex_destroy(&m->mu);
  free(m);
}

#endif
