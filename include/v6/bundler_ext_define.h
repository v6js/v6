#pragma once

#include "v6/bundler_extension.h"

typedef struct v6_bundler_define_state v6_bundler_define_state;

v6_bundler_define_state* v6_bundler_define_state_create(void);
void v6_bundler_define_state_add(v6_bundler_define_state* state,
                                 const char* key, const char* value);
void v6_bundler_define_state_free(v6_bundler_define_state* state);
v6_bundler_extension
v6_bundler_define_extension(v6_bundler_define_state* state);
