#pragma once

#include <stddef.h>

int v6_bundler_mkdir_p(const char* path);
int v6_bundler_write_file(const char* path, const char* data, size_t len);
int v6_bundler_copy_file(const char* src_path, const char* dst_path);
int v6_bundler_copy_dir_recursive(const char* src_dir, const char* dst_dir);
