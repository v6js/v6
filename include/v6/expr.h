#pragma once

#include "v6/internal.h"
#include "v6/module.h"

int is_logical_assign_op(tok_kind k);
void emit_logical_assign_ident(parser* p, compiler* c, var_ref vr, tok_kind op);
void emit_logical_assign_member(parser* p, compiler* c, tok_kind op);
void parse_unary(parser* p, compiler* c);
void parse_expr(parser* p, compiler* c);
void parse_seq_expr(parser* p, compiler* c);
