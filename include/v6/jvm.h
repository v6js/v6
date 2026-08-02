#pragma once

#include <stddef.h>

typedef struct v6_jvm v6_jvm;

int v6_jvm_available(void);
v6_jvm* v6_jvm_create(void);
int v6_jvm_load_runtime(v6_jvm* jvm);
int v6_jvm_define_extra(v6_jvm* jvm, const char* name,
                        const unsigned char* class_bytes, size_t len);
int v6_jvm_run(v6_jvm* jvm, const unsigned char* class_bytes, size_t len,
              char** script_args, int script_argc);
void v6_jvm_destroy(v6_jvm* jvm);
