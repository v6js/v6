#include "v6/value.h"

#include <stdint.h>
#include <string.h>

#define v6_qnan 0x7ffc000000000000ULL
#define v6_sign 0x8000000000000000ULL
#define v6_ptr_mask 0x0000fffffffffffful
#define v6_nan_canon 0x7ff8000000000000ULL

enum {
  v6_tag_null = 1,
  v6_tag_undef = 2,
  v6_tag_false = 3,
  v6_tag_true = 4,
};

v6_val v6_num(double n) {
  v6_val v;
  memcpy(&v, &n, sizeof(v));
  if ((v & v6_qnan) == v6_qnan)
    v = v6_nan_canon;
  return v;
}

double v6_as_num(v6_val v) {
  double n;
  memcpy(&n, &v, sizeof(n));
  return n;
}

int v6_is_num(v6_val v) {
  return (v & v6_qnan) != v6_qnan;
}

v6_val v6_bool(int b) {
  return v6_qnan | (uint64_t)(b ? v6_tag_true : v6_tag_false);
}

int v6_is_bool(v6_val v) {
  return v == v6_bool(0) || v == v6_bool(1);
}

int v6_as_bool(v6_val v) {
  return v == v6_bool(1);
}

v6_val v6_null(void) {
  return v6_qnan | v6_tag_null;
}

int v6_is_null(v6_val v) {
  return v == v6_null();
}

v6_val v6_undef(void) {
  return v6_qnan | v6_tag_undef;
}

int v6_is_undef(v6_val v) {
  return v == v6_undef();
}

v6_val v6_obj(void* p) {
  return v6_sign | v6_qnan | ((uint64_t)(uintptr_t)p & v6_ptr_mask);
}

int v6_is_obj(v6_val v) {
  return (v & (v6_sign | v6_qnan)) == (v6_sign | v6_qnan);
}

void* v6_as_obj(v6_val v) {
  return (void*)(uintptr_t)(v & v6_ptr_mask);
}
