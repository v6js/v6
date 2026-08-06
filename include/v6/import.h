#pragma once

#include "v6/internal.h"
#include "v6/module.h"

void emit_require_expr(parser* p, compiler* c);
void parse_import_stmt(parser* p, compiler* c);
