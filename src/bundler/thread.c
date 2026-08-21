#ifndef _WIN32
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#endif

#include "v6/bundle_thread.h"

#include <stdlib.h>

#ifdef _WIN32

#include <windows.h>

struct bundle_thread {
  HANDLE handle;
};

struct bundle_mutex {
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

bundle_thread* bundle_thread_start(void (*fn)(void* arg), void* arg) {
  bundle_thread* t = malloc(sizeof(bundle_thread));
  thread_trampoline_arg* ta = malloc(sizeof(thread_trampoline_arg));
  ta->fn = fn;
  ta->arg = arg;
  t->handle = CreateThread(NULL, 0, thread_trampoline, ta, 0, NULL);
  return t;
}

void bundle_thread_join(bundle_thread* t) {
  if (!t)
    return;
  WaitForSingleObject(t->handle, INFINITE);
  CloseHandle(t->handle);
  free(t);
}

bundle_mutex* bundle_mutex_create(void) {
  bundle_mutex* m = malloc(sizeof(bundle_mutex));
  InitializeCriticalSection(&m->cs);
  return m;
}

void bundle_mutex_lock(bundle_mutex* m) {
  EnterCriticalSection(&m->cs);
}

void bundle_mutex_unlock(bundle_mutex* m) {
  LeaveCriticalSection(&m->cs);
}

void bundle_mutex_free(bundle_mutex* m) {
  if (!m)
    return;
  DeleteCriticalSection(&m->cs);
  free(m);
}

#else

#include <pthread.h>

struct bundle_thread {
  pthread_t handle;
};

struct bundle_mutex {
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

bundle_thread* bundle_thread_start(void (*fn)(void* arg), void* arg) {
  bundle_thread* t = malloc(sizeof(bundle_thread));
  thread_trampoline_arg* ta = malloc(sizeof(thread_trampoline_arg));
  ta->fn = fn;
  ta->arg = arg;
  pthread_create(&t->handle, NULL, thread_trampoline, ta);
  return t;
}

void bundle_thread_join(bundle_thread* t) {
  if (!t)
    return;
  pthread_join(t->handle, NULL);
  free(t);
}

bundle_mutex* bundle_mutex_create(void) {
  bundle_mutex* m = malloc(sizeof(bundle_mutex));
  pthread_mutex_init(&m->mu, NULL);
  return m;
}

void bundle_mutex_lock(bundle_mutex* m) {
  pthread_mutex_lock(&m->mu);
}

void bundle_mutex_unlock(bundle_mutex* m) {
  pthread_mutex_unlock(&m->mu);
}

void bundle_mutex_free(bundle_mutex* m) {
  if (!m)
    return;
  pthread_mutex_destroy(&m->mu);
  free(m);
}

#endif
