#pragma once

#include <stdint.h>

typedef uint64_t v6_val;

v6_val v6_num(double n);
double v6_as_num(v6_val v);
int v6_is_num(v6_val v);

v6_val v6_bool(int b);
int v6_as_bool(v6_val v);
int v6_is_bool(v6_val v);

v6_val v6_null(void);
int v6_is_null(v6_val v);

v6_val v6_undef(void);
int v6_is_undef(v6_val v);

v6_val v6_obj(void* p);
void* v6_as_obj(v6_val v);
int v6_is_obj(v6_val v);
