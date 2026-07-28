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

extern const unsigned char v6_runtime_class[];
extern const size_t v6_runtime_class_len;

extern const unsigned char v6_object_class[];
extern const size_t v6_object_class_len;
