#pragma once

#include "v6/bytecode.h"
#include "v6/lexer.h"
#include "v6/module.h"
#include "v6/parser.h"

typedef enum { var_local, var_upvalue, var_not_found } var_kind;

typedef struct {
  var_kind kind;
  uint16_t index;
} var_ref;

void advance(parser* p);
int check(parser* p, tok_kind k);
void emit_aload(method* m, uint16_t slot);
void emit_astore(method* m, uint16_t slot);
void emit_box_tag(compiler* c, uint8_t tag_op);
void emit_dconst_val(class_file* cf, method* m, double v);
void emit_dload(method* m, uint16_t slot);
void emit_dstore(method* m, uint16_t slot);
void emit_iconst(method* m, int n);
void emit_to_int32_raw(class_file* cf, method* m);
void emit_to_number(compiler* c);
void emit_var_read_ref(compiler* c, var_ref vr);
void emit_var_write_ref(compiler* c, var_ref vr);
local* find_local_entry(compiler* c, const char* name, size_t len);
int find_slot(compiler* c, const char* name, size_t len, uint16_t* out);
uint16_t value_class(class_file* cf);

void emit_ref_push(compiler* c, int is_upvalue, uint16_t index);
void emit_var_declare(compiler* c, uint16_t slot);
uint16_t next_declared_slot(compiler* c);
void emit_box_bool(compiler* c);
void emit_truthy(compiler* c);
void emit_box_object_ref(compiler* c);
void emit_box_ref_computed(compiler* c, int tag_val);

void error_at(parser* p, const char* msg);
int match(parser* p, tok_kind k);
int expect(parser* p, tok_kind k);
int expect_semi(parser* p);
int is_contextual_ident(tok_kind k);
int match_property_name(parser* p);
void skip_balanced(parser* p, tok_kind open, tok_kind close);
compile_result compile_module_impl(class_file* cf, const char* this_class_name,
                                   const char* user_src, const char* module_dir,
                                   module_ctx* modctx, int is_entry,
                                   int is_cjs);
