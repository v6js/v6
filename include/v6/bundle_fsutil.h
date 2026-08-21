#pragma once

#include <stddef.h>

int bundle_mkdir_p(const char* path);
int bundle_write_file(const char* path, const char* data, size_t len);
int bundle_copy_file(const char* src_path, const char* dst_path);
