#pragma once

typedef struct bundle_thread bundle_thread;
typedef struct bundle_mutex bundle_mutex;

bundle_thread* bundle_thread_start(void (*fn)(void* arg), void* arg);
void bundle_thread_join(bundle_thread* t);

bundle_mutex* bundle_mutex_create(void);
void bundle_mutex_lock(bundle_mutex* m);
void bundle_mutex_unlock(bundle_mutex* m);
void bundle_mutex_free(bundle_mutex* m);
