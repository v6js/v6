#pragma once

#include <stddef.h>

enum {
  rt_tag_num = 0,
  rt_tag_bool = 1,
  rt_tag_null = 2,
  rt_tag_undef = 3,
  rt_tag_obj = 4,
  rt_tag_str = 5,
};

typedef struct {
  const char* name;
  const unsigned char* data;
  size_t len;
} v6_rt_class;

extern const v6_rt_class v6_runtime_classes[];
extern const size_t v6_runtime_class_count;
