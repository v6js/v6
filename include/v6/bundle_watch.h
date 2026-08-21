#pragma once

typedef struct bundle_watcher bundle_watcher;

bundle_watcher* bundle_watcher_create(void);
int bundle_watcher_add_dir(bundle_watcher* w, const char* dir);
int bundle_watcher_wait(bundle_watcher* w, int timeout_ms);
void bundle_watcher_free(bundle_watcher* w);
