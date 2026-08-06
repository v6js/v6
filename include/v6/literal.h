#pragma once

#include "v6/internal.h"
#include "v6/module.h"

char* decode_string(tok t);
char* dup_tok(tok t);
void parse_object_literal(parser* p, compiler* c);
void parse_array_literal(parser* p, compiler* c);
void emit_string_value(compiler* c, const char* s);
void emit_throw_reference_error(compiler* c, const char* name, size_t len);
void parse_template_literal(parser* p, compiler* c);
void emit_tagged_template_call(parser* p, compiler* c);
void emit_regex_literal(parser* p, compiler* c, tok t);
