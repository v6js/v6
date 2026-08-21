#pragma once

#include "v6/bundler_extension.h"

typedef struct v6_bundler_alias_state v6_bundler_alias_state;

v6_bundler_alias_state* v6_bundler_alias_state_create(const char* base_dir);
void v6_bundler_alias_state_add(v6_bundler_alias_state* state, const char* from,
                                const char* to);
void v6_bundler_alias_state_free(v6_bundler_alias_state* state);
v6_bundler_extension v6_bundler_alias_extension(v6_bundler_alias_state* state);
