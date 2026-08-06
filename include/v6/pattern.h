#pragma once

#include "v6/internal.h"
#include "v6/module.h"

void parse_one_declarator_named(parser* p, compiler* c, tok_kind kind,
                                tok name);
void skip_field_init(parser* p);
void parse_array_pattern(parser* p, compiler* c, tok_kind kind,
                         uint16_t src_slot);
void parse_object_pattern(parser* p, compiler* c, tok_kind kind,
                          uint16_t src_slot);
void parse_one_declarator(parser* p, compiler* c, tok_kind kind);
void parse_var_decl(parser* p, compiler* c, tok_kind kind);
