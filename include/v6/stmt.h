#pragma once

#include "v6/internal.h"
#include "v6/module.h"

void parse_block(parser* p, compiler* c);
void parse_stmt(parser* p, compiler* c);
void parse_program(parser* p, compiler* c);
