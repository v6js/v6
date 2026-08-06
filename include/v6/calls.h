#pragma once

#include "v6/internal.h"
#include "v6/module.h"

void emit_args_array(parser* p, compiler* c);
void emit_call_args_and_invoke(parser* p, compiler* c);
void compile_direct_call(parser* p, compiler* c, var_ref vr,
                         const char* lambda_name);
void compile_direct_new(parser* p, compiler* c, var_ref vr,
                        const char* lambda_name);
void parse_postfix(parser* p, compiler* c);
