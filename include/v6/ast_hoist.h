#pragma once

#include "v6/ast.h"
#include "v6/internal.h"

void ast_hoist_scope(compiler* c, ast_list* body);
