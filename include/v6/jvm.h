#pragma once

typedef struct v6_jvm v6_jvm;

int v6_jvm_available(void);
v6_jvm* v6_jvm_create(void);
int v6_jvm_load_runtime(v6_jvm* jvm);
void v6_jvm_destroy(v6_jvm* jvm);
