#pragma once

#include "v6/ast.h"
#include "v6/parser.h"

ast_node* ast_parse_program_from(ast_arena* arena, parser* p);
