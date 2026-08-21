#pragma once

typedef struct v6_bundler_watcher v6_bundler_watcher;

v6_bundler_watcher* v6_bundler_watcher_create(void);
int v6_bundler_watcher_add_dir(v6_bundler_watcher* w, const char* dir);
int v6_bundler_watcher_wait(v6_bundler_watcher* w, int timeout_ms);
void v6_bundler_watcher_free(v6_bundler_watcher* w);
