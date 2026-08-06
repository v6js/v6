#pragma once

#include "v6/internal.h"
#include "v6/module.h"

void emit_wrap_generator(compiler* c);
void emit_wrap_async(compiler* c);
void emit_wrap_async_generator(compiler* c);
void compile_closure_value(parser* p, compiler* c, int is_arrow,
                           int parens_params, char* out_lambda_name);
void parse_function_decl(parser* p, compiler* c);
