#include "v6/parser.h"

#include "v6/module.h"
#include "v6/internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v6/class.h"
#include "v6/closures.h"
#include "v6/expr.h"
#include "v6/literal.h"
#include "v6/pattern.h"
#include "v6/scope.h"

void parse_class_decl(parser* p, compiler* c, int is_expr) {
  tok name;
  if (is_expr && !check(p, tok_ident)) {
    char* synth = malloc(24);
    snprintf(synth, 24, "$anonclass%d", (*c->lambda_counter)++);
    name.kind = tok_ident;
    name.start = synth;
    name.len = strlen(synth);
    name.line = p->cur.line;
    name.num = 0;
    name.is_bigint = 0;
  } else {
    if (!expect(p, tok_ident))
      return;
    name = p->prev;
  }

  tok base_name;
  int has_base = 0;
  if (match(p, tok_kw_extends)) {
    int simple_ident = 0;
    if (check(p, tok_ident)) {
      lexer save_lex = p->lex;
      tok save_cur = p->cur;
      tok save_prev = p->prev;
      tok ident_tok = p->cur;
      advance(p);
      if (check(p, tok_lbrace)) {
        simple_ident = 1;
        base_name = ident_tok;
      } else {
        p->lex = save_lex;
        p->cur = save_cur;
        p->prev = save_prev;
      }
    }
    if (!simple_ident) {
      parse_unary(p, c);
      uint16_t base_slot = c->next_local_slot++;
      emit_var_declare(c, base_slot);
      char* synth = malloc(24);
      snprintf(synth, 24, "$super%d", (*c->lambda_counter)++);
      base_name.kind = tok_ident;
      base_name.start = synth;
      base_name.len = strlen(synth);
      base_name.line = 0;
      base_name.num = 0;
      add_local(c, base_name, base_slot, 0, 0);
    }
    has_base = 1;
  }

  if (!expect(p, tok_lbrace))
    return;

  uint16_t cls_tmp = c->next_local_slot++;
  uint16_t proto_tmp = c->next_local_slot++;

  uint16_t cls_cls = cf_class(c->cf, "V6Class");
  uint16_t cls_ctor_idx = cf_methodref(c->cf, "V6Class", "<init>", "()V");
  op_emit2(c->m, op_new, cls_cls);
  op_emit(c->m, op_dup);
  op_emit2(c->m, op_invokespecial, cls_ctor_idx);
  emit_astore(c->m, cls_tmp);

  uint16_t obj_ctor_idx = cf_methodref(c->cf, "V6Object", "<init>", "()V");
  op_emit2(c->m, op_new, object_class(c->cf));
  op_emit(c->m, op_dup);
  op_emit2(c->m, op_invokespecial, obj_ctor_idx);
  emit_astore(c->m, proto_tmp);

  const char* saved_super_name = c->super_name;
  size_t saved_super_len = c->super_len;
  const char* saved_class_name = c->class_name;
  size_t saved_class_name_len = c->class_name_len;
  c->class_name = name.start;
  c->class_name_len = name.len;
  char* ctor_lambda_name = NULL;

  var_ref base_vr;
  base_vr.kind = var_not_found;
  base_vr.index = 0;
  if (has_base) {
    base_vr = resolve_var(c, base_name.start, base_name.len);
    if (base_vr.kind == var_not_found) {
      error_at(p, "undeclared variable");
      return;
    }
    c->super_name = base_name.start;
    c->super_len = base_name.len;

    uint16_t setproto_idx =
        cf_methodref(c->cf, "V6Object", "setProtoFromValue", "(LV6Value;)V");
    uint16_t proto_str = cf_string(c->cf, "prototype");
    uint16_t getprop_idx = cf_methodref(c->cf, "V6Value", "getProp",
                                        "(Ljava/lang/String;)LV6Value;");
    emit_aload(c->m, proto_tmp);
    emit_var_read_ref(c, base_vr);
    op_emit2(c->m, op_ldc_w, proto_str);
    op_emit2(c->m, op_invokevirtual, getprop_idx);
    op_emit2(c->m, op_invokevirtual, setproto_idx);
  }

  uint16_t set_idx =
      cf_methodref(c->cf, "V6Object", "set", "(Ljava/lang/String;LV6Value;)V");
  uint16_t proto_prop_str = cf_string(c->cf, "prototype");
  emit_aload(c->m, cls_tmp);
  op_emit2(c->m, op_ldc_w, proto_prop_str);
  emit_aload(c->m, proto_tmp);
  emit_box_object_ref(c);
  op_emit2(c->m, op_invokevirtual, set_idx);

  field_init pending_instance_fields[v6_max_fields];
  int pending_instance_field_count = 0;

  while (!check(p, tok_rbrace) && !check(p, tok_eof) && !p->had_error) {
    int is_static = 0;
    if (check(p, tok_kw_static)) {
      lexer static_save_lex = p->lex;
      tok static_save_cur = p->cur;
      tok static_save_prev = p->prev;
      advance(p);
      if (check(p, tok_lparen) || check(p, tok_assign) || check(p, tok_semi) ||
          check(p, tok_rbrace)) {
        p->lex = static_save_lex;
        p->cur = static_save_cur;
        p->prev = static_save_prev;
      } else {
        is_static = 1;
      }
    }

    int is_async = 0;
    if (check(p, tok_kw_async)) {
      lexer save_lex = p->lex;
      tok save_cur = p->cur;
      tok save_prev = p->prev;
      advance(p);
      if (check(p, tok_lparen)) {
        p->lex = save_lex;
        p->cur = save_cur;
        p->prev = save_prev;
      } else {
        is_async = 1;
      }
    }

    int is_gen = match(p, tok_star);

    int is_getter = 0, is_setter = 0;
    if (!is_async && (check(p, tok_kw_get) || check(p, tok_kw_set))) {
      tok_kind modifier = p->cur.kind;
      lexer save_lex = p->lex;
      tok save_cur = p->cur;
      tok save_prev = p->prev;
      advance(p);
      if (check(p, tok_lparen)) {
        p->lex = save_lex;
        p->cur = save_cur;
        p->prev = save_prev;
      } else {
        is_getter = modifier == tok_kw_get;
        is_setter = modifier == tok_kw_set;
      }
    }

    int computed = 0;
    uint16_t computed_key_slot = 0;
    tok member_name;
    member_name.start = NULL;
    member_name.len = 0;
    if (match(p, tok_lbracket)) {
      computed = 1;
      parse_expr(p, c);
      uint16_t tostring_idx =
          cf_methodref(c->cf, "V6Value", "toString", "()Ljava/lang/String;");
      op_emit2(c->m, op_invokevirtual, tostring_idx);
      if (!expect(p, tok_rbracket))
        return;
      computed_key_slot = c->next_local_slot++;
      emit_astore(c->m, computed_key_slot);
    } else if (!match_property_name(p)) {
      break;
    } else {
      member_name = p->prev;
    }
    int is_ctor = !computed && !is_static && !is_getter && !is_setter &&
                  member_name.len == 11 &&
                  memcmp(member_name.start, "constructor", 11) == 0;

    if (is_getter || is_setter) {
      uint16_t target_slot = is_static ? cls_tmp : proto_tmp;
      emit_aload(c->m, target_slot);
      if (computed) {
        emit_aload(c->m, computed_key_slot);
      } else {
        char* mkey = dup_tok(member_name);
        uint16_t mkey_idx = cf_string(c->cf, mkey);
        free(mkey);
        op_emit2(c->m, op_ldc_w, mkey_idx);
      }
      compile_closure_value(p, c, 0, 1, NULL);
      uint16_t ascall_idx =
          cf_methodref(c->cf, "V6Value", "asCallable", "()LV6Callable;");
      op_emit2(c->m, op_invokevirtual, ascall_idx);
      uint16_t def_idx = cf_methodref(
          c->cf, "V6Object", is_getter ? "defineGetter" : "defineSetter",
          "(Ljava/lang/String;LV6Callable;)V");
      op_emit2(c->m, op_invokevirtual, def_idx);
    } else if (is_ctor) {
      emit_aload(c->m, cls_tmp);
      ctor_lambda_name = malloc(24);
      for (int i = 0; i < pending_instance_field_count; i++)
        c->pending_fields[i] = pending_instance_fields[i];
      c->pending_field_count = pending_instance_field_count;
      compile_closure_value(p, c, 0, 1, ctor_lambda_name);
      c->pending_field_count = 0;
      uint16_t ascall_idx =
          cf_methodref(c->cf, "V6Value", "asCallable", "()LV6Callable;");
      op_emit2(c->m, op_invokevirtual, ascall_idx);
      uint16_t ctor_field =
          cf_fieldref(c->cf, "V6Class", "ctor", "LV6Callable;");
      op_emit2(c->m, op_putfield, ctor_field);
    } else if (check(p, tok_lparen)) {
      uint16_t target_slot = is_static ? cls_tmp : proto_tmp;
      emit_aload(c->m, target_slot);
      if (computed) {
        emit_aload(c->m, computed_key_slot);
      } else {
        char* mkey = dup_tok(member_name);
        uint16_t mkey_idx = cf_string(c->cf, mkey);
        free(mkey);
        op_emit2(c->m, op_ldc_w, mkey_idx);
      }
      compile_closure_value(p, c, 0, 1, NULL);
      if (is_gen && is_async) {
        error_at(p, "async generator methods are not supported");
        return;
      } else if (is_gen) {
        emit_wrap_generator(c);
      } else if (is_async) {
        emit_wrap_async(c);
      }
      op_emit2(c->m, op_invokevirtual, set_idx);
    } else if (computed) {
      error_at(p, "computed field names are not supported");
      return;
    } else {
      const char* init_src = NULL;
      if (match(p, tok_assign)) {
        init_src = p->cur.start;
        skip_field_init(p);
      }
      match(p, tok_semi);
      if (is_static) {
        emit_aload(c->m, cls_tmp);
        char* mkey = dup_tok(member_name);
        uint16_t mkey_idx = cf_string(c->cf, mkey);
        free(mkey);
        op_emit2(c->m, op_ldc_w, mkey_idx);
        if (init_src) {
          parser fieldp;
          parser_init(&fieldp, init_src);
          parse_expr(&fieldp, c);
        } else {
          emit_undef(c->cf, c->m);
        }
        op_emit2(c->m, op_invokevirtual, set_idx);
      } else if (pending_instance_field_count < v6_max_fields) {
        pending_instance_fields[pending_instance_field_count].name =
            member_name.start;
        pending_instance_fields[pending_instance_field_count].name_len =
            member_name.len;
        pending_instance_fields[pending_instance_field_count].init_src =
            init_src;
        pending_instance_field_count++;
      }
    }
  }
  expect(p, tok_rbrace);

  if ((has_base || pending_instance_field_count > 0) && !ctor_lambda_name) {
    emit_aload(c->m, cls_tmp);
    ctor_lambda_name = malloc(24);
    for (int i = 0; i < pending_instance_field_count; i++)
      c->pending_fields[i] = pending_instance_fields[i];
    c->pending_field_count = pending_instance_field_count;
    parser synth;
    parser_init(&synth, has_base ? "(...args) { super(...args); }" : "() {}");
    compile_closure_value(&synth, c, 0, 1, ctor_lambda_name);
    c->pending_field_count = 0;
    uint16_t ascall_idx =
        cf_methodref(c->cf, "V6Value", "asCallable", "()LV6Callable;");
    op_emit2(c->m, op_invokevirtual, ascall_idx);
    uint16_t ctor_field = cf_fieldref(c->cf, "V6Class", "ctor", "LV6Callable;");
    op_emit2(c->m, op_putfield, ctor_field);
  }

  c->super_name = saved_super_name;
  c->super_len = saved_super_len;
  c->class_name = saved_class_name;
  c->class_name_len = saved_class_name_len;

  emit_aload(c->m, cls_tmp);
  emit_box_object_ref(c);

  if (is_expr)
    return;

  if (c->brace_depth == 0) {
    var_ref vr = resolve_var(c, name.start, name.len);
    if (vr.kind == var_not_found) {
      error_at(p, "internal: hoisted class missing");
      return;
    }
    emit_var_write_ref(c, vr);
    op_emit(c->m, op_pop);
  } else {
    uint16_t slot = c->next_local_slot++;
    emit_var_declare(c, slot);
    add_local(c, name, slot, 0, 0);
  }

  if (ctor_lambda_name &&
      !name_reassigned_in_scope(p->lex.src, name.start, name.len)) {
    local* le = find_local_entry(c, name.start, name.len);
    if (le) {
      le->direct_fn = 1;
      le->fn_method_name = ctor_lambda_name;
    }
  }
}
