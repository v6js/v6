#pragma once

#include "v6/internal.h"
#include "v6/module.h"

char* decode_string(tok t);
char* dup_tok(tok t);
void emit_string_value(compiler* c, const char* s);
void emit_throw_reference_error(compiler* c, const char* name, size_t len);
