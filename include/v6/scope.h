#pragma once

#include "v6/internal.h"
#include "v6/module.h"

local* find_local_entry(compiler* c, const char* name, size_t len);
void add_local(compiler* c, tok name, uint16_t slot, int is_var, int is_const);
var_ref resolve_var(compiler* c, const char* name, size_t len);
