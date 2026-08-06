#pragma once

#include <stddef.h>

typedef struct v6_jvm v6_jvm;

int v6_jvm_available(void);
int v6_jvm_detect_version(char* out, size_t out_cap);
v6_jvm* v6_jvm_create(const char* classpath, int is_daemon);
int v6_jvm_load_runtime(v6_jvm* jvm);
int v6_jvm_define_extra(v6_jvm* jvm, const char* name,
                        const unsigned char* class_bytes, size_t len);
int v6_jvm_run(v6_jvm* jvm, const unsigned char* class_bytes, size_t len,
               char** script_args, int script_argc);
int v6_jvm_serve_daemon(v6_jvm* jvm, const char* lock_file_path,
                        long idle_timeout_ms, long long binary_mtime,
                        long long binary_size, long long execution_timeout_ms);
void v6_jvm_destroy(v6_jvm* jvm);
