#pragma once

#include <stddef.h>

typedef struct {
  const char* name;
  const unsigned char* data;
  size_t len;
} v6_daemon_class_entry;

int v6_daemon_run(const char* exe_path, v6_daemon_class_entry* classes,
                  int num_classes, const char* script_path,
                  char** script_args, int script_argc, int* exit_code);

int v6_get_own_exe_path(char* out, size_t out_size);
int v6_stat_file(const char* path, long long* mtime, long long* size);
