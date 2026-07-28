#pragma once

#include <stddef.h>

typedef struct v6_jvm v6_jvm;

int v6_jvm_available(void);
v6_jvm* v6_jvm_create(void);
int v6_jvm_load_runtime(v6_jvm* jvm);
int v6_jvm_run(v6_jvm* jvm, const unsigned char* class_bytes, size_t len);
void v6_jvm_destroy(v6_jvm* jvm);
