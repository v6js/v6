#pragma once

typedef struct v6_jvm v6_jvm;

int v6_jvm_available(void);
v6_jvm* v6_jvm_create(void);
void v6_jvm_destroy(v6_jvm* jvm);
