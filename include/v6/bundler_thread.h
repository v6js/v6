#pragma once

typedef struct v6_bundler_thread v6_bundler_thread;
typedef struct v6_bundler_mutex v6_bundler_mutex;

v6_bundler_thread* v6_bundler_thread_start(void (*fn)(void* arg), void* arg);
void v6_bundler_thread_join(v6_bundler_thread* t);

v6_bundler_mutex* v6_bundler_mutex_create(void);
void v6_bundler_mutex_lock(v6_bundler_mutex* m);
void v6_bundler_mutex_unlock(v6_bundler_mutex* m);
void v6_bundler_mutex_free(v6_bundler_mutex* m);
