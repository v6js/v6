#include "v6/ast_codegen.h"

#include "v6/ast_hoist.h"
#include "v6/closures.h"
#include "v6/import.h"
#include "v6/literal.h"
#include "v6/module.h"
#include "v6/scope.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  int had_error;
  char err_msg[1024];
  int err_line;
} ast_cg_err;

static ast_cg_err g_cgerr;

int g_v6_use_ast_compiler = 0;

void ast_cg_reset_error(void) {
  g_cgerr.had_error = 0;
  g_cgerr.err_msg[0] = '\0';
  g_cgerr.err_line = 0;
}

int ast_cg_had_error(char* out_msg, size_t out_msg_cap, int* out_line) {
  if (!g_cgerr.had_error)
    return 0;
  if (out_msg && out_msg_cap > 0)
    snprintf(out_msg, out_msg_cap, "%s", g_cgerr.err_msg);
  if (out_line)
    *out_line = g_cgerr.err_line;
  return 1;
}

static void cg_error(const char* msg, int line) {
  if (g_cgerr.had_error)
    return;
  g_cgerr.had_error = 1;
  g_cgerr.err_line = line;
  size_t n = strlen(msg);
  if (n >= sizeof(g_cgerr.err_msg))
    n = sizeof(g_cgerr.err_msg) - 1;
  memcpy(g_cgerr.err_msg, msg, n);
  g_cgerr.err_msg[n] = '\0';
}

static tok mktok_from_ident(ast_node* n) {
  tok t;
  memset(&t, 0, sizeof(t));
  t.kind = tok_ident;
  t.start = n->str;
  t.len = n->str_len;
  return t;
}

static void cg_expr(compiler* c, ast_node* e);
static void cg_stmt(compiler* c, ast_node* s);
static void cg_stmt_list(compiler* c, ast_list* list);
static void cg_block(compiler* c, ast_node* block);
static void cg_hoist_and_block(compiler* c, ast_node* block);
static void cg_call_args(compiler* c, ast_list* args, int has_spread);
static int list_has_spread(ast_list* args);
static void cg_chain_value(compiler* c, ast_node* node);
static void cg_member_key(compiler* c, ast_node* member);
static void cg_bind_target_value_on_stack(compiler* c, ast_node* target,
                                          tok_kind kind);
static void cg_bind_pattern_from_slot(compiler* c, ast_node* pattern,
                                      tok_kind kind, uint16_t src_slot);
static void cg_emit_pattern_default(compiler* c, ast_node* default_expr);
static void cg_class_expr(compiler* c, ast_node* n, int is_stmt);
static void cg_tagged_template(compiler* c, ast_node* node);
static ast_arena* g_synth_arena(void);
static ast_node* ast_synth_dup(ast_node* src);

static int list_has_spread(ast_list* args) {
  for (int i = 0; i < args->len; i++)
    if (args->items[i]->kind == ast_spread)
      return 1;
  return 0;
}

static void cg_call_args(compiler* c, ast_list* args, int has_spread) {
  if (!has_spread) {
    emit_iconst(c->m, args->len);
    op_emit2(c->m, op_anewarray, value_class(c->cf));
    for (int i = 0; i < args->len; i++) {
      op_emit(c->m, op_dup);
      emit_iconst(c->m, i);
      cg_expr(c, args->items[i]);
      op_emit(c->m, op_aastore);
    }
    return;
  }

  uint16_t arr_cls = cf_class(c->cf, "V6Array");
  uint16_t arr_ctor = cf_methodref(c->cf, "V6Array", "<init>", "()V");
  uint16_t push_idx = cf_methodref(c->cf, "V6Object", "push", "(LV6Value;)V");
  uint16_t pushall_idx =
      cf_methodref(c->cf, "V6Object", "pushAll", "(LV6Value;)V");
  uint16_t tovalarr_idx =
      cf_methodref(c->cf, "V6Object", "toValueArray", "()[LV6Value;");

  op_emit2(c->m, op_new, arr_cls);
  op_emit(c->m, op_dup);
  op_emit2(c->m, op_invokespecial, arr_ctor);

  for (int i = 0; i < args->len; i++) {
    op_emit(c->m, op_dup);
    if (args->items[i]->kind == ast_spread) {
      cg_expr(c, args->items[i]->a);
      op_emit2(c->m, op_invokevirtual, pushall_idx);
    } else {
      cg_expr(c, args->items[i]);
      op_emit2(c->m, op_invokevirtual, push_idx);
    }
  }
  op_emit2(c->m, op_invokevirtual, tovalarr_idx);
}

static void cg_invoke_with_this_fn_on_stack(compiler* c, ast_list* args) {
  int has_spread = list_has_spread(args);
  op_emit(c->m, op_swap);
  cg_call_args(c, args, has_spread);
  uint16_t call_idx =
      cf_methodref(c->cf, "V6Value", "call", "(LV6Value;[LV6Value;)LV6Value;");
  op_emit2(c->m, op_invokevirtual, call_idx);
}

static void cg_member_key(compiler* c, ast_node* member) {
  if (member->flag_a) {
    cg_expr(c, member->b);
    uint16_t tostring_idx =
        cf_methodref(c->cf, "V6Value", "toString", "()Ljava/lang/String;");
    op_emit2(c->m, op_invokevirtual, tostring_idx);
  } else {
    uint16_t key_idx = cf_string(c->cf, member->str);
    op_emit2(c->m, op_ldc_w, key_idx);
  }
}

static void cg_optional_guard_begin(compiler* c, size_t* jumps, int* count) {
  op_emit(c->m, op_dup);
  uint16_t nullish_idx = cf_methodref(c->cf, "V6Value", "isNullish", "()Z");
  op_emit2(c->m, op_invokevirtual, nullish_idx);
  size_t skip_pos = op_pos(c->m);
  op_emit2(c->m, op_ifeq, 0);
  op_emit(c->m, op_pop);
  emit_undef(c->cf, c->m);
  if (*count < 32)
    jumps[(*count)++] = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);
  size_t cont_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(skip_pos + 1), (uint16_t)(cont_pos - skip_pos));
}

static void cg_chain_impl(compiler* c, ast_node* node, size_t* jumps,
                          int* count) {
  if (node->kind == ast_member) {
    cg_chain_impl(c, node->a, jumps, count);
    if (node->flag_b) {
      cg_optional_guard_begin(c, jumps, count);
      cg_member_key(c, node);
      uint16_t get_idx = cf_methodref(c->cf, "V6Value", "getProp",
                                      "(Ljava/lang/String;)LV6Value;");
      op_emit2(c->m, op_invokevirtual, get_idx);
      return;
    }
    cg_member_key(c, node);
    uint16_t get_idx = cf_methodref(c->cf, "V6Value", "getProp",
                                    "(Ljava/lang/String;)LV6Value;");
    op_emit2(c->m, op_invokevirtual, get_idx);
    return;
  }

  if (node->kind == ast_call) {
    ast_node* callee = node->a;
    if (callee->kind == ast_super_member) {
      var_ref base_vr = resolve_var(c, c->super_name, c->super_len);
      var_ref this_vr = resolve_var(c, "this", 4);
      if (!c->super_name || base_vr.kind == var_not_found ||
          this_vr.kind == var_not_found) {
        cg_error("'super' outside class", node->line);
        emit_undef(c->cf, c->m);
        return;
      }
      uint16_t proto_str = cf_string(c->cf, "prototype");
      uint16_t getprop_idx = cf_methodref(c->cf, "V6Value", "getProp",
                                          "(Ljava/lang/String;)LV6Value;");
      uint16_t key_idx = cf_string(c->cf, callee->str);
      emit_var_read_ref(c, base_vr);
      op_emit2(c->m, op_ldc_w, proto_str);
      op_emit2(c->m, op_invokevirtual, getprop_idx);
      op_emit2(c->m, op_ldc_w, key_idx);
      op_emit2(c->m, op_invokevirtual, getprop_idx);
      emit_var_read_ref(c, this_vr);
      op_emit(c->m, op_swap);
      cg_invoke_with_this_fn_on_stack(c, &node->list);
      return;
    }

    if (callee->kind == ast_member) {
      cg_chain_impl(c, callee->a, jumps, count);
      if (callee->flag_b)
        cg_optional_guard_begin(c, jumps, count);
      op_emit(c->m, op_dup);
      cg_member_key(c, callee);
      uint16_t get_idx = cf_methodref(c->cf, "V6Value", "getProp",
                                      "(Ljava/lang/String;)LV6Value;");
      op_emit2(c->m, op_invokevirtual, get_idx);
      cg_invoke_with_this_fn_on_stack(c, &node->list);
      return;
    }

    cg_expr(c, callee);
    emit_undef(c->cf, c->m);
    op_emit(c->m, op_swap);
    if (node->flag_a) {
      op_emit(c->m, op_dup);
      uint16_t nullish_idx = cf_methodref(c->cf, "V6Value", "isNullish", "()Z");
      op_emit2(c->m, op_invokevirtual, nullish_idx);
      size_t skip_pos = op_pos(c->m);
      op_emit2(c->m, op_ifeq, 0);
      op_emit(c->m, op_pop);
      op_emit(c->m, op_pop);
      emit_undef(c->cf, c->m);
      if (*count < 32)
        jumps[(*count)++] = op_pos(c->m);
      op_emit2(c->m, op_goto, 0);
      size_t cont_pos = op_pos(c->m);
      op_patch2(c->m, (uint16_t)(skip_pos + 1),
                (uint16_t)(cont_pos - skip_pos));
    }
    cg_invoke_with_this_fn_on_stack(c, &node->list);
    return;
  }

  if (node->kind == ast_tagged_template) {
    ast_node* tag = node->a;
    if (tag->kind == ast_member) {
      cg_chain_impl(c, tag->a, jumps, count);
      op_emit(c->m, op_dup);
      cg_member_key(c, tag);
      uint16_t get_idx = cf_methodref(c->cf, "V6Value", "getProp",
                                      "(Ljava/lang/String;)LV6Value;");
      op_emit2(c->m, op_invokevirtual, get_idx);
    } else {
      cg_expr(c, tag);
      emit_undef(c->cf, c->m);
      op_emit(c->m, op_swap);
    }
    cg_tagged_template(c, node);
    return;
  }

  cg_expr(c, node);
}

static void cg_chain_value(compiler* c, ast_node* node) {
  size_t jumps[32];
  int count = 0;
  cg_chain_impl(c, node, jumps, &count);
  size_t end_pos = op_pos(c->m);
  for (int i = 0; i < count; i++)
    op_patch2(c->m, (uint16_t)(jumps[i] + 1), (uint16_t)(end_pos - jumps[i]));
}

static void cg_tagged_template(compiler* c, ast_node* node) {
  uint16_t fn_slot = c->next_local_slot++;
  uint16_t this_slot = c->next_local_slot++;
  emit_astore(c->m, fn_slot);
  emit_astore(c->m, this_slot);

  uint16_t arr_cls = cf_class(c->cf, "V6Array");
  uint16_t arr_ctor = cf_methodref(c->cf, "V6Array", "<init>", "()V");
  uint16_t push_idx = cf_methodref(c->cf, "V6Object", "push", "(LV6Value;)V");
  uint16_t set_idx =
      cf_methodref(c->cf, "V6Object", "set", "(Ljava/lang/String;LV6Value;)V");
  uint16_t tovalarr_idx =
      cf_methodref(c->cf, "V6Object", "toValueArray", "()[LV6Value;");

  op_emit2(c->m, op_new, arr_cls);
  op_emit(c->m, op_dup);
  op_emit2(c->m, op_invokespecial, arr_ctor);
  uint16_t strings_slot = c->next_local_slot++;
  emit_astore(c->m, strings_slot);

  op_emit2(c->m, op_new, arr_cls);
  op_emit(c->m, op_dup);
  op_emit2(c->m, op_invokespecial, arr_ctor);
  uint16_t raw_slot = c->next_local_slot++;
  emit_astore(c->m, raw_slot);

  op_emit2(c->m, op_new, arr_cls);
  op_emit(c->m, op_dup);
  op_emit2(c->m, op_invokespecial, arr_ctor);
  uint16_t callargs_slot = c->next_local_slot++;
  emit_astore(c->m, callargs_slot);

  emit_aload(c->m, callargs_slot);
  emit_aload(c->m, strings_slot);
  emit_box_object_ref(c);
  op_emit2(c->m, op_invokevirtual, push_idx);

  int cooked_n = node->quasis_cooked.len;
  for (int i = 0; i < cooked_n; i++) {
    emit_aload(c->m, strings_slot);
    emit_string_value(c, node->quasis_cooked.items[i]->str);
    op_emit2(c->m, op_invokevirtual, push_idx);

    emit_aload(c->m, raw_slot);
    emit_string_value(c, node->quasis_raw.items[i]->str);
    op_emit2(c->m, op_invokevirtual, push_idx);

    if (i < node->list.len) {
      emit_aload(c->m, callargs_slot);
      cg_expr(c, node->list.items[i]);
      op_emit2(c->m, op_invokevirtual, push_idx);
    }
  }

  uint16_t raw_str = cf_string(c->cf, "raw");
  emit_aload(c->m, strings_slot);
  op_emit2(c->m, op_ldc_w, raw_str);
  emit_aload(c->m, raw_slot);
  emit_box_object_ref(c);
  op_emit2(c->m, op_invokevirtual, set_idx);

  emit_aload(c->m, fn_slot);
  emit_aload(c->m, this_slot);
  emit_aload(c->m, callargs_slot);
  op_emit2(c->m, op_invokevirtual, tovalarr_idx);
  uint16_t call_idx =
      cf_methodref(c->cf, "V6Value", "call", "(LV6Value;[LV6Value;)LV6Value;");
  op_emit2(c->m, op_invokevirtual, call_idx);
}

static void cg_assign_ident(compiler* c, ast_node* target, tok_kind op,
                            ast_node* value) {
  var_ref vr = resolve_var(c, target->str, target->str_len);
  if (vr.kind == var_not_found && op != tok_assign) {
    emit_throw_reference_error(c, target->str, target->str_len);
    cg_expr(c, value);
    op_emit(c->m, op_pop);
    return;
  }
  if (vr.kind == var_not_found) {
    cg_error("undeclared variable", target->line);
    return;
  }
  local* le = find_local_entry(c, target->str, target->str_len);
  if (le && le->is_const) {
    cg_error("assignment to constant variable", target->line);
    return;
  }

  if (op == tok_amp_amp_eq || op == tok_pipe_pipe_eq ||
      op == tok_question_question_eq) {
    emit_var_read_ref(c, vr);
    op_emit(c->m, op_dup);
    if (op == tok_question_question_eq) {
      uint16_t idx = cf_methodref(c->cf, "V6Value", "isNullish", "()Z");
      op_emit2(c->m, op_invokevirtual, idx);
    } else {
      emit_truthy(c);
    }
    size_t else_jump = op_pos(c->m);
    uint8_t jump_op = (op == tok_pipe_pipe_eq) ? op_ifne : op_ifeq;
    op_emit2(c->m, jump_op, 0);
    op_emit(c->m, op_pop);
    cg_expr(c, value);
    emit_var_write_ref(c, vr);
    size_t end_jump = op_pos(c->m);
    op_emit2(c->m, op_goto, 0);
    size_t else_pos = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(else_jump + 1),
              (uint16_t)(else_pos - else_jump));
    size_t end_pos = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(end_jump + 1), (uint16_t)(end_pos - end_jump));
    return;
  }

  if (op == tok_assign) {
    cg_expr(c, value);
  } else if (op == tok_plus_eq) {
    emit_var_read_ref(c, vr);
    cg_expr(c, value);
    uint16_t idx =
        cf_methodref(c->cf, "V6Value", "add", "(LV6Value;LV6Value;)LV6Value;");
    op_emit2(c->m, op_invokestatic, idx);
  } else if (op == tok_amp_eq || op == tok_pipe_eq || op == tok_caret_eq ||
             op == tok_shl_eq || op == tok_shr_eq || op == tok_ushr_eq) {
    emit_var_read_ref(c, vr);
    cg_expr(c, value);
    const char* mname = op == tok_amp_eq     ? "bitAnd"
                        : op == tok_pipe_eq  ? "bitOr"
                        : op == tok_caret_eq ? "bitXor"
                        : op == tok_shl_eq   ? "shl"
                        : op == tok_shr_eq   ? "shr"
                                             : "ushr";
    uint16_t idx =
        cf_methodref(c->cf, "V6Value", mname, "(LV6Value;LV6Value;)LV6Value;");
    op_emit2(c->m, op_invokestatic, idx);
  } else {
    emit_var_read_ref(c, vr);
    emit_to_number(c);
    cg_expr(c, value);
    emit_to_number(c);
    uint8_t bop =
        op == tok_minus_eq
            ? op_dsub
            : (op == tok_star_eq ? op_dmul
                                 : (op == tok_slash_eq ? op_ddiv : op_drem));
    op_emit(c->m, bop);
    emit_box_tag(c, op_iconst_0);
  }

  emit_var_write_ref(c, vr);
}

static void cg_assign_member(compiler* c, ast_node* target, tok_kind op,
                             ast_node* value) {
  cg_chain_value(c, target->a);
  cg_member_key(c, target);

  if (op == tok_amp_amp_eq || op == tok_pipe_pipe_eq ||
      op == tok_question_question_eq) {
    op_emit(c->m, op_dup2);
    uint16_t getprop_idx = cf_methodref(c->cf, "V6Value", "getProp",
                                        "(Ljava/lang/String;)LV6Value;");
    op_emit2(c->m, op_invokevirtual, getprop_idx);
    op_emit(c->m, op_dup);
    if (op == tok_question_question_eq) {
      uint16_t idx = cf_methodref(c->cf, "V6Value", "isNullish", "()Z");
      op_emit2(c->m, op_invokevirtual, idx);
    } else {
      emit_truthy(c);
    }
    size_t else_jump = op_pos(c->m);
    uint8_t jump_op = (op == tok_pipe_pipe_eq) ? op_ifne : op_ifeq;
    op_emit2(c->m, jump_op, 0);
    op_emit(c->m, op_pop);
    cg_expr(c, value);
    op_emit(c->m, op_dup_x2);
    uint16_t setprop_idx = cf_methodref(c->cf, "V6Value", "setProp",
                                        "(Ljava/lang/String;LV6Value;)V");
    op_emit2(c->m, op_invokevirtual, setprop_idx);
    size_t end_jump = op_pos(c->m);
    op_emit2(c->m, op_goto, 0);
    size_t else_pos = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(else_jump + 1),
              (uint16_t)(else_pos - else_jump));
    op_emit(c->m, op_dup_x2);
    op_emit(c->m, op_pop);
    op_emit(c->m, op_pop);
    op_emit(c->m, op_pop);
    size_t end_pos = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(end_jump + 1), (uint16_t)(end_pos - end_jump));
    return;
  }

  if (op == tok_assign) {
    cg_expr(c, value);
    op_emit(c->m, op_dup_x2);
    uint16_t set_idx = cf_methodref(c->cf, "V6Value", "setProp",
                                    "(Ljava/lang/String;LV6Value;)V");
    op_emit2(c->m, op_invokevirtual, set_idx);
    return;
  }

  uint16_t getprop_idx2 = cf_methodref(c->cf, "V6Value", "getProp",
                                       "(Ljava/lang/String;)LV6Value;");
  uint16_t set_idx = cf_methodref(c->cf, "V6Value", "setProp",
                                  "(Ljava/lang/String;LV6Value;)V");
  op_emit(c->m, op_dup2);
  op_emit2(c->m, op_invokevirtual, getprop_idx2);

  if (op == tok_plus_eq) {
    cg_expr(c, value);
    uint16_t idx =
        cf_methodref(c->cf, "V6Value", "add", "(LV6Value;LV6Value;)LV6Value;");
    op_emit2(c->m, op_invokestatic, idx);
  } else if (op == tok_amp_eq || op == tok_pipe_eq || op == tok_caret_eq ||
             op == tok_shl_eq || op == tok_shr_eq || op == tok_ushr_eq) {
    cg_expr(c, value);
    const char* mname = op == tok_amp_eq     ? "bitAnd"
                        : op == tok_pipe_eq  ? "bitOr"
                        : op == tok_caret_eq ? "bitXor"
                        : op == tok_shl_eq   ? "shl"
                        : op == tok_shr_eq   ? "shr"
                                             : "ushr";
    uint16_t idx =
        cf_methodref(c->cf, "V6Value", mname, "(LV6Value;LV6Value;)LV6Value;");
    op_emit2(c->m, op_invokestatic, idx);
  } else {
    emit_to_number(c);
    cg_expr(c, value);
    emit_to_number(c);
    uint8_t bop =
        op == tok_minus_eq
            ? op_dsub
            : (op == tok_star_eq ? op_dmul
                                 : (op == tok_slash_eq ? op_ddiv : op_drem));
    op_emit(c->m, bop);
    emit_box_tag(c, op_iconst_0);
  }

  op_emit(c->m, op_dup_x2);
  op_emit2(c->m, op_invokevirtual, set_idx);
}

static void cg_assign(compiler* c, ast_node* n) {
  if (n->a->kind == ast_ident) {
    cg_assign_ident(c, n->a, n->op, n->b);
  } else if (n->a->kind == ast_member) {
    cg_assign_member(c, n->a, n->op, n->b);
  } else {
    cg_error("invalid assignment target", n->line);
    emit_undef(c->cf, c->m);
  }
}

static void cg_update(compiler* c, ast_node* n) {
  ast_node* target = n->a;
  int is_inc = n->op == tok_plus_plus;

  if (target->kind == ast_member) {
    cg_chain_value(c, target->a);
    cg_member_key(c, target);
    uint16_t getprop_idx2 = cf_methodref(c->cf, "V6Value", "getProp",
                                         "(Ljava/lang/String;)LV6Value;");
    uint16_t set_idx = cf_methodref(c->cf, "V6Value", "setProp",
                                    "(Ljava/lang/String;LV6Value;)V");
    op_emit(c->m, op_dup2);
    op_emit2(c->m, op_invokevirtual, getprop_idx2);
    if (n->flag_a) {
      emit_to_number(c);
      op_emit(c->m, op_dconst_1);
      op_emit(c->m, is_inc ? op_dadd : op_dsub);
      emit_box_tag(c, op_iconst_0);
      op_emit(c->m, op_dup_x2);
      op_emit2(c->m, op_invokevirtual, set_idx);
    } else {
      op_emit(c->m, op_dup_x2);
      emit_to_number(c);
      op_emit(c->m, op_dconst_1);
      op_emit(c->m, is_inc ? op_dadd : op_dsub);
      emit_box_tag(c, op_iconst_0);
      op_emit2(c->m, op_invokevirtual, set_idx);
    }
    return;
  }

  if (target->kind != ast_ident) {
    cg_error("invalid update target", n->line);
    emit_undef(c->cf, c->m);
    return;
  }

  var_ref vr = resolve_var(c, target->str, target->str_len);
  if (vr.kind == var_not_found) {
    emit_throw_reference_error(c, target->str, target->str_len);
    return;
  }
  local* le = find_local_entry(c, target->str, target->str_len);
  if (le && le->is_const) {
    cg_error("assignment to constant variable", n->line);
    return;
  }

  if (n->flag_a) {
    emit_var_read_ref(c, vr);
    emit_to_number(c);
    op_emit(c->m, op_dconst_1);
    op_emit(c->m, is_inc ? op_dadd : op_dsub);
    emit_box_tag(c, op_iconst_0);
    emit_var_write_ref(c, vr);
  } else {
    emit_var_read_ref(c, vr);
    op_emit(c->m, op_dup);
    emit_to_number(c);
    op_emit(c->m, op_dconst_1);
    op_emit(c->m, is_inc ? op_dadd : op_dsub);
    emit_box_tag(c, op_iconst_0);
    emit_var_write_ref(c, vr);
    op_emit(c->m, op_pop);
  }
}

static void cg_delete(compiler* c, ast_node* target) {
  if (target->kind != ast_member) {
    cg_expr(c, target);
    op_emit(c->m, op_pop);
    emit_const_singleton(c->cf, c->m, "TRUE");
    return;
  }
  cg_chain_value(c, target->a);
  cg_member_key(c, target);
  uint16_t del_idx =
      cf_methodref(c->cf, "V6Value", "deleteProp", "(Ljava/lang/String;)Z");
  op_emit2(c->m, op_invokevirtual, del_idx);
  op_emit(c->m, op_i2d);
  emit_box_bool(c);
}

static void cg_new(compiler* c, ast_node* n) {
  cg_expr(c, n->a);
  int has_spread = list_has_spread(&n->list);
  cg_call_args(c, &n->list, has_spread);
  uint16_t construct_idx = cf_methodref(c->cf, "V6Value", "construct",
                                        "(LV6Value;[LV6Value;)LV6Value;");
  op_emit2(c->m, op_invokestatic, construct_idx);
}

static void cg_new_target(compiler* c) {
  if (c->class_name) {
    var_ref this_vr = resolve_var(c, "this", 4);
    if (this_vr.kind != var_not_found) {
      emit_var_read_ref(c, this_vr);
      uint16_t ref_idx =
          cf_methodref(c->cf, "V6Value", "ref", "()Ljava/lang/Object;");
      op_emit2(c->m, op_invokevirtual, ref_idx);
      uint16_t obj_cls = cf_class(c->cf, "V6Object");
      op_emit2(c->m, op_checkcast, obj_cls);
      uint16_t new_target_field =
          cf_fieldref(c->cf, "V6Object", "newTarget", "LV6Value;");
      op_emit2(c->m, op_getfield, new_target_field);
      op_emit(c->m, op_dup);
      size_t has_val_jump = op_pos(c->m);
      op_emit2(c->m, op_ifnonnull, 0);
      op_emit(c->m, op_pop);
      emit_undef(c->cf, c->m);
      size_t has_val_pos = op_pos(c->m);
      op_patch2(c->m, (uint16_t)(has_val_jump + 1),
                (uint16_t)(has_val_pos - has_val_jump));
      return;
    }
  }
  emit_undef(c->cf, c->m);
}

static void cg_expr(compiler* c, ast_node* e) {
  switch (e->kind) {
  case ast_num: {
    uint16_t idx = cf_double(c->cf, e->num);
    op_emit2(c->m, op_ldc2_w, idx);
    op_emit2(c->m, op_invokestatic, value_num_method(c->cf));
    break;
  }
  case ast_bigint: {
    uint16_t str_idx = cf_string(c->cf, e->str);
    uint16_t bigint_cls = cf_class(c->cf, "java/math/BigInteger");
    uint16_t bigint_ctor = cf_methodref(c->cf, "java/math/BigInteger", "<init>",
                                        "(Ljava/lang/String;)V");
    op_emit2(c->m, op_new, value_class(c->cf));
    op_emit(c->m, op_dup);
    emit_iconst(c->m, V6_TAG_BIGINT);
    op_emit(c->m, op_dconst_0);
    op_emit2(c->m, op_new, bigint_cls);
    op_emit(c->m, op_dup);
    op_emit2(c->m, op_ldc_w, str_idx);
    op_emit2(c->m, op_invokespecial, bigint_ctor);
    op_emit2(c->m, op_invokespecial, value_ctor(c->cf));
    break;
  }
  case ast_str:
    emit_string_value(c, e->str);
    break;
  case ast_bool:
    emit_const_singleton(c->cf, c->m, e->flag_a ? "TRUE" : "FALSE");
    break;
  case ast_null:
    emit_const_singleton(c->cf, c->m, "NUL");
    break;
  case ast_undef: {
    var_ref vr = resolve_var(c, "undefined", 9);
    if (vr.kind != var_not_found)
      emit_var_read_ref(c, vr);
    else
      emit_undef(c->cf, c->m);
    break;
  }
  case ast_this: {
    var_ref vr = resolve_var(c, "this", 4);
    if (vr.kind == var_not_found)
      emit_undef(c->cf, c->m);
    else
      emit_var_read_ref(c, vr);
    break;
  }
  case ast_new_target:
    cg_new_target(c);
    break;
  case ast_ident: {
    var_ref vr = resolve_var(c, e->str, e->str_len);
    if (vr.kind == var_not_found)
      emit_throw_reference_error(c, e->str, e->str_len);
    else
      emit_var_read_ref(c, vr);
    break;
  }
  case ast_template: {
    emit_string_value(c, "");
    uint16_t add_idx =
        cf_methodref(c->cf, "V6Value", "add", "(LV6Value;LV6Value;)LV6Value;");
    int n = e->quasis_cooked.len;
    for (int i = 0; i < n; i++) {
      emit_string_value(c, e->quasis_cooked.items[i]->str);
      op_emit2(c->m, op_invokestatic, add_idx);
      if (i < e->list.len) {
        cg_expr(c, e->list.items[i]);
        op_emit2(c->m, op_invokestatic, add_idx);
      }
    }
    break;
  }
  case ast_regex: {
    var_ref vr = resolve_var(c, "RegExp", 6);
    if (vr.kind == var_not_found) {
      cg_error("RegExp is not defined", e->line);
      emit_undef(c->cf, c->m);
      break;
    }
    emit_var_read_ref(c, vr);
    uint16_t cls_val_slot = c->next_local_slot++;
    emit_astore(c->m, cls_val_slot);
    emit_iconst(c->m, 2);
    op_emit2(c->m, op_anewarray, value_class(c->cf));
    op_emit(c->m, op_dup);
    emit_iconst(c->m, 0);
    emit_string_value(c, e->str);
    op_emit(c->m, op_aastore);
    op_emit(c->m, op_dup);
    emit_iconst(c->m, 1);
    emit_string_value(c, e->str2);
    op_emit(c->m, op_aastore);
    uint16_t args_slot = c->next_local_slot++;
    emit_astore(c->m, args_slot);
    emit_aload(c->m, cls_val_slot);
    emit_aload(c->m, args_slot);
    uint16_t construct_idx = cf_methodref(c->cf, "V6Value", "construct",
                                          "(LV6Value;[LV6Value;)LV6Value;");
    op_emit2(c->m, op_invokestatic, construct_idx);
    break;
  }
  case ast_array_lit: {
    uint16_t array_cls = cf_class(c->cf, "V6Array");
    uint16_t ctor_idx = cf_methodref(c->cf, "V6Array", "<init>", "()V");
    op_emit2(c->m, op_new, array_cls);
    op_emit(c->m, op_dup);
    op_emit2(c->m, op_invokespecial, ctor_idx);
    uint16_t push_idx = cf_methodref(c->cf, "V6Object", "push", "(LV6Value;)V");
    uint16_t pushall_idx =
        cf_methodref(c->cf, "V6Object", "pushAll", "(LV6Value;)V");
    for (int i = 0; i < e->list.len; i++) {
      ast_node* item = e->list.items[i];
      if (item->kind == ast_pat_hole)
        continue;
      op_emit(c->m, op_dup);
      if (item->kind == ast_spread) {
        cg_expr(c, item->a);
        op_emit2(c->m, op_invokevirtual, pushall_idx);
      } else {
        cg_expr(c, item);
        op_emit2(c->m, op_invokevirtual, push_idx);
      }
    }
    emit_box_object_ref(c);
    break;
  }
  case ast_object_lit: {
    uint16_t ctor_idx = cf_methodref(c->cf, "V6Object", "<init>", "()V");
    op_emit2(c->m, op_new, object_class(c->cf));
    op_emit(c->m, op_dup);
    op_emit2(c->m, op_invokespecial, ctor_idx);
    for (int i = 0; i < e->props.len; i++) {
      ast_prop* pr = &e->props.items[i];
      if (pr->is_spread) {
        op_emit(c->m, op_dup);
        cg_expr(c, pr->value);
        uint16_t spread_idx =
            cf_methodref(c->cf, "V6Object", "spreadFrom", "(LV6Value;)V");
        op_emit2(c->m, op_invokevirtual, spread_idx);
        continue;
      }
      op_emit(c->m, op_dup);
      if (pr->computed) {
        cg_expr(c, pr->key);
        uint16_t tostring_idx =
            cf_methodref(c->cf, "V6Value", "toString", "()Ljava/lang/String;");
        op_emit2(c->m, op_invokevirtual, tostring_idx);
      } else {
        uint16_t key_idx = cf_string(c->cf, pr->key->str);
        op_emit2(c->m, op_ldc_w, key_idx);
      }
      if (pr->is_getter || pr->is_setter) {
        cg_expr(c, pr->value);
        uint16_t ascall_idx =
            cf_methodref(c->cf, "V6Value", "asCallable", "()LV6Callable;");
        op_emit2(c->m, op_invokevirtual, ascall_idx);
        uint16_t def_idx = cf_methodref(
            c->cf, "V6Object", pr->is_getter ? "defineGetter" : "defineSetter",
            "(Ljava/lang/String;LV6Callable;)V");
        op_emit2(c->m, op_invokevirtual, def_idx);
        continue;
      }
      cg_expr(c, pr->value);
      if (pr->is_method) {
        if (pr->is_generator && pr->is_async)
          emit_wrap_async_generator(c);
        else if (pr->is_generator)
          emit_wrap_generator(c);
        else if (pr->is_async)
          emit_wrap_async(c);
      }
      uint16_t set_idx = cf_methodref(c->cf, "V6Object", "set",
                                      "(Ljava/lang/String;LV6Value;)V");
      op_emit2(c->m, op_invokevirtual, set_idx);
    }
    emit_box_object_ref(c);
    break;
  }
  case ast_func_expr: {
    char lambda_name[24];
    ast_codegen_function_value(c, e, lambda_name);
    if (e->flag_c && e->flag_b)
      emit_wrap_async_generator(c);
    else if (e->flag_c)
      emit_wrap_generator(c);
    else if (e->flag_b)
      emit_wrap_async(c);
    break;
  }
  case ast_class_expr:
    cg_class_expr(c, e, 0);
    break;
  case ast_unary: {
    if (e->op == tok_kw_delete) {
      cg_delete(c, e->a);
      break;
    }
    if (e->op == tok_plus) {
      cg_expr(c, e->a);
      emit_to_number(c);
      emit_box_tag(c, op_iconst_0);
      break;
    }
    if (e->op == tok_minus) {
      cg_expr(c, e->a);
      uint16_t idx =
          cf_methodref(c->cf, "V6Value", "neg", "(LV6Value;)LV6Value;");
      op_emit2(c->m, op_invokestatic, idx);
      break;
    }
    if (e->op == tok_bang) {
      cg_expr(c, e->a);
      emit_truthy(c);
      op_emit(c->m, op_iconst_1);
      op_emit(c->m, op_ixor);
      op_emit(c->m, op_i2d);
      emit_box_bool(c);
      break;
    }
    if (e->op == tok_tilde) {
      cg_expr(c, e->a);
      uint16_t idx =
          cf_methodref(c->cf, "V6Value", "bitNot", "(LV6Value;)LV6Value;");
      op_emit2(c->m, op_invokestatic, idx);
      break;
    }
    if (e->op == tok_kw_void) {
      cg_expr(c, e->a);
      op_emit(c->m, op_pop);
      emit_undef(c->cf, c->m);
      break;
    }
    if (e->op == tok_kw_typeof) {
      if (e->a->kind == ast_ident) {
        var_ref probe = resolve_var(c, e->a->str, e->a->str_len);
        if (probe.kind == var_not_found) {
          emit_undef(c->cf, c->m);
        } else {
          cg_expr(c, e->a);
        }
      } else {
        cg_expr(c, e->a);
      }
      uint16_t idx =
          cf_methodref(c->cf, "V6Value", "typeOf", "()Ljava/lang/String;");
      op_emit2(c->m, op_invokevirtual, idx);
      emit_box_ref_computed(c, V6_TAG_STR);
      break;
    }
    break;
  }
  case ast_update:
    cg_update(c, e);
    break;
  case ast_binary: {
    if (e->op == tok_kw_instanceof) {
      cg_expr(c, e->a);
      cg_expr(c, e->b);
      uint16_t idx =
          cf_methodref(c->cf, "V6Value", "instanceOf", "(LV6Value;LV6Value;)Z");
      op_emit2(c->m, op_invokestatic, idx);
      op_emit(c->m, op_i2d);
      emit_box_bool(c);
      break;
    }
    if (e->op == tok_kw_in) {
      cg_expr(c, e->a);
      cg_expr(c, e->b);
      uint16_t idx =
          cf_methodref(c->cf, "V6Value", "hasProp", "(LV6Value;LV6Value;)Z");
      op_emit2(c->m, op_invokestatic, idx);
      op_emit(c->m, op_i2d);
      emit_box_bool(c);
      break;
    }
    if (e->op == tok_lt || e->op == tok_gt || e->op == tok_le ||
        e->op == tok_ge) {
      cg_expr(c, e->a);
      cg_expr(c, e->b);
      const char* name = e->op == tok_lt   ? "lt"
                         : e->op == tok_le ? "le"
                         : e->op == tok_gt ? "gt"
                                           : "ge";
      uint16_t idx =
          cf_methodref(c->cf, "V6Value", name, "(LV6Value;LV6Value;)Z");
      op_emit2(c->m, op_invokestatic, idx);
      op_emit(c->m, op_i2d);
      emit_box_bool(c);
      break;
    }
    if (e->op == tok_eq || e->op == tok_neq || e->op == tok_eq_strict ||
        e->op == tok_neq_strict) {
      cg_expr(c, e->a);
      cg_expr(c, e->b);
      int strict = e->op == tok_eq_strict || e->op == tok_neq_strict;
      int negate = e->op == tok_neq || e->op == tok_neq_strict;
      uint16_t idx = cf_methodref(c->cf, "V6Value",
                                  strict ? "strictEquals" : "looseEquals",
                                  "(LV6Value;LV6Value;)Z");
      op_emit2(c->m, op_invokestatic, idx);
      if (negate) {
        op_emit(c->m, op_iconst_1);
        op_emit(c->m, op_ixor);
      }
      op_emit(c->m, op_i2d);
      emit_box_bool(c);
      break;
    }
    cg_expr(c, e->a);
    cg_expr(c, e->b);
    const char* mname = NULL;
    switch (e->op) {
    case tok_star_star:
      mname = "pow";
      break;
    case tok_star:
      mname = "mul";
      break;
    case tok_slash:
      mname = "div";
      break;
    case tok_percent:
      mname = "mod";
      break;
    case tok_plus:
      mname = "add";
      break;
    case tok_minus:
      mname = "sub";
      break;
    case tok_shl:
      mname = "shl";
      break;
    case tok_shr:
      mname = "shr";
      break;
    case tok_ushr:
      mname = "ushr";
      break;
    case tok_amp:
      mname = "bitAnd";
      break;
    case tok_caret:
      mname = "bitXor";
      break;
    case tok_pipe:
      mname = "bitOr";
      break;
    default:
      mname = "add";
      break;
    }
    uint16_t idx =
        cf_methodref(c->cf, "V6Value", mname, "(LV6Value;LV6Value;)LV6Value;");
    op_emit2(c->m, op_invokestatic, idx);
    break;
  }
  case ast_logical: {
    cg_expr(c, e->a);
    if (e->op == tok_amp_amp) {
      op_emit(c->m, op_dup);
      emit_truthy(c);
      size_t is_left_pos = op_pos(c->m);
      op_emit2(c->m, op_ifeq, 0);
      op_emit(c->m, op_pop);
      cg_expr(c, e->b);
      size_t end_pos = op_pos(c->m);
      op_patch2(c->m, (uint16_t)(is_left_pos + 1),
                (uint16_t)(end_pos - is_left_pos));
    } else if (e->op == tok_pipe_pipe) {
      op_emit(c->m, op_dup);
      emit_truthy(c);
      size_t is_left_pos = op_pos(c->m);
      op_emit2(c->m, op_ifne, 0);
      op_emit(c->m, op_pop);
      cg_expr(c, e->b);
      size_t end_pos = op_pos(c->m);
      op_patch2(c->m, (uint16_t)(is_left_pos + 1),
                (uint16_t)(end_pos - is_left_pos));
    } else {
      op_emit(c->m, op_dup);
      uint16_t idx = cf_methodref(c->cf, "V6Value", "isNullish", "()Z");
      op_emit2(c->m, op_invokevirtual, idx);
      size_t is_left_pos = op_pos(c->m);
      op_emit2(c->m, op_ifeq, 0);
      op_emit(c->m, op_pop);
      cg_expr(c, e->b);
      size_t end_pos = op_pos(c->m);
      op_patch2(c->m, (uint16_t)(is_left_pos + 1),
                (uint16_t)(end_pos - is_left_pos));
    }
    break;
  }
  case ast_assign:
    cg_assign(c, e);
    break;
  case ast_cond: {
    cg_expr(c, e->a);
    emit_truthy(c);
    size_t else_jump = op_pos(c->m);
    op_emit2(c->m, op_ifeq, 0);
    cg_expr(c, e->b);
    size_t end_jump = op_pos(c->m);
    op_emit2(c->m, op_goto, 0);
    size_t else_pos = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(else_jump + 1),
              (uint16_t)(else_pos - else_jump));
    cg_expr(c, e->c);
    size_t end_pos = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(end_jump + 1), (uint16_t)(end_pos - end_jump));
    break;
  }
  case ast_call: {
    if (e->raw_src && e->a->kind == ast_ident && e->a->str_len == 7 &&
        memcmp(e->a->str, "require", 7) == 0) {
      var_ref existing = resolve_var(c, e->a->str, e->a->str_len);
      if (existing.kind == var_not_found) {
        parser subp;
        parser_init(&subp, e->raw_src);
        emit_require_expr(&subp, c);
        if (subp.had_error)
          cg_error(subp.err_msg, subp.err_line);
        break;
      }
    }
    cg_chain_value(c, e);
    break;
  }
  case ast_new:
    cg_new(c, e);
    break;
  case ast_member:
    cg_chain_value(c, e);
    break;
  case ast_tagged_template:
    cg_chain_value(c, e);
    break;
  case ast_seq: {
    for (int i = 0; i < e->list.len; i++) {
      if (i > 0)
        op_emit(c->m, op_pop);
      cg_expr(c, e->list.items[i]);
    }
    break;
  }
  case ast_spread:
    cg_expr(c, e->a);
    break;
  case ast_yield: {
    if (e->a)
      cg_expr(c, e->a);
    else
      emit_undef(c->cf, c->m);
    if (e->flag_a) {
      uint16_t iter_cls = cf_class(c->cf, "V6Iterator");
      uint16_t iter_ctor =
          cf_methodref(c->cf, "V6Iterator", "<init>", "(LV6Value;)V");
      uint16_t iter_slot = c->next_local_slot++;
      op_emit2(c->m, op_new, iter_cls);
      op_emit(c->m, op_dup_x1);
      op_emit(c->m, op_swap);
      op_emit2(c->m, op_invokespecial, iter_ctor);
      emit_astore(c->m, iter_slot);

      uint16_t has_next_idx =
          cf_methodref(c->cf, "V6Iterator", "hasNext", "()Z");
      uint16_t next_idx =
          cf_methodref(c->cf, "V6Iterator", "next", "()LV6Value;");
      uint16_t yield_idx = cf_methodref(c->cf, "V6Generator", "currentYield",
                                        "(LV6Value;)LV6Value;");

      size_t loop_pos = op_pos(c->m);
      emit_aload(c->m, iter_slot);
      op_emit2(c->m, op_invokevirtual, has_next_idx);
      size_t exit_jump = op_pos(c->m);
      op_emit2(c->m, op_ifeq, 0);

      emit_aload(c->m, iter_slot);
      op_emit2(c->m, op_invokevirtual, next_idx);
      op_emit2(c->m, op_invokestatic, yield_idx);
      op_emit(c->m, op_pop);

      size_t back_jump = op_pos(c->m);
      op_emit2(c->m, op_goto, 0);
      op_patch2(c->m, (uint16_t)(back_jump + 1),
                (uint16_t)(loop_pos - back_jump));

      size_t end_pos = op_pos(c->m);
      op_patch2(c->m, (uint16_t)(exit_jump + 1),
                (uint16_t)(end_pos - exit_jump));

      emit_undef(c->cf, c->m);
      break;
    }
    uint16_t yield_idx = cf_methodref(c->cf, "V6Generator", "currentYield",
                                      "(LV6Value;)LV6Value;");
    op_emit2(c->m, op_invokestatic, yield_idx);
    break;
  }
  case ast_await: {
    cg_expr(c, e->a);
    uint16_t yield_idx = cf_methodref(
        c->cf, "V6Generator", c->is_async_gen ? "currentAwait" : "currentYield",
        "(LV6Value;)LV6Value;");
    op_emit2(c->m, op_invokestatic, yield_idx);
    break;
  }
  case ast_super_call: {
    var_ref base_vr = resolve_var(c, c->super_name, c->super_len);
    var_ref this_vr = resolve_var(c, "this", 4);
    if (!c->super_name || base_vr.kind == var_not_found ||
        this_vr.kind == var_not_found) {
      cg_error("'super' outside class", e->line);
      emit_undef(c->cf, c->m);
      break;
    }
    emit_var_read_ref(c, base_vr);
    emit_var_read_ref(c, this_vr);
    int has_spread = list_has_spread(&e->list);
    cg_call_args(c, &e->list, has_spread);
    uint16_t sc_idx = cf_methodref(c->cf, "V6Value", "superConstruct",
                                   "(LV6Value;LV6Value;[LV6Value;)V");
    op_emit2(c->m, op_invokestatic, sc_idx);
    emit_undef(c->cf, c->m);
    break;
  }
  case ast_super_member:
    cg_error("expected '('", e->line);
    emit_undef(c->cf, c->m);
    break;
  case ast_paren_pattern_assign: {
    cg_expr(c, e->b);
    op_emit(c->m, op_dup);
    uint16_t src_slot = c->next_local_slot++;
    emit_astore(c->m, src_slot);
    cg_bind_pattern_from_slot(c, e->a, tok_kw_var, src_slot);
    emit_aload(c->m, src_slot);
    break;
  }
  default:
    emit_undef(c->cf, c->m);
    break;
  }
}

static void cg_emit_pattern_default(compiler* c, ast_node* default_expr) {
  op_emit(c->m, op_dup);
  uint16_t isundef_idx = cf_methodref(c->cf, "V6Value", "isUndefined", "()Z");
  op_emit2(c->m, op_invokevirtual, isundef_idx);
  size_t skip_jump = op_pos(c->m);
  op_emit2(c->m, op_ifeq, 0);
  op_emit(c->m, op_pop);
  cg_expr(c, default_expr);
  size_t after = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(skip_jump + 1), (uint16_t)(after - skip_jump));
}

static void cg_bind_simple_target(compiler* c, ast_node* target,
                                  tok_kind kind) {
  if (kind == tok_kw_var) {
    var_ref vr = resolve_var(c, target->str, target->str_len);
    if (vr.kind == var_not_found) {
      cg_error("internal: hoisted var missing", target->line);
      return;
    }
    emit_var_write_ref(c, vr);
    op_emit(c->m, op_pop);
    return;
  }
  if (c->brace_depth == 0) {
    local* le = find_local_entry(c, target->str, target->str_len);
    if (le) {
      var_ref vr;
      vr.kind = var_local;
      vr.index = le->slot;
      emit_var_write_ref(c, vr);
      op_emit(c->m, op_pop);
      return;
    }
  }
  uint16_t slot = next_declared_slot(c);
  emit_var_declare(c, slot);
  tok t = mktok_from_ident(target);
  add_local(c, t, slot, 0, kind == tok_kw_const);
}

static void cg_bind_target_value_on_stack(compiler* c, ast_node* target,
                                          tok_kind kind) {
  if (target->kind == ast_pat_ident) {
    cg_bind_simple_target(c, target, kind);
    return;
  }
  uint16_t slot = c->next_local_slot++;
  emit_astore(c->m, slot);
  cg_bind_pattern_from_slot(c, target, kind, slot);
}

static void cg_bind_pattern_from_slot(compiler* c, ast_node* pattern,
                                      tok_kind kind, uint16_t src_slot) {
  uint16_t getprop_idx = cf_methodref(c->cf, "V6Value", "getProp",
                                      "(Ljava/lang/String;)LV6Value;");
  if (pattern->kind == ast_pat_array) {
    int idx = 0;
    for (int i = 0; i < pattern->list.len; i++) {
      ast_node* el = pattern->list.items[i];
      if (el->kind == ast_pat_hole) {
        idx++;
        continue;
      }
      if (el->kind == ast_pat_rest) {
        uint16_t restfrom_idx =
            cf_methodref(c->cf, "V6Value", "restFrom", "(I)LV6Array;");
        emit_aload(c->m, src_slot);
        emit_iconst(c->m, idx);
        op_emit2(c->m, op_invokevirtual, restfrom_idx);
        emit_box_object_ref(c);
        cg_bind_target_value_on_stack(c, el->a, kind);
        break;
      }
      ast_node* elem_target = el;
      ast_node* elem_default = NULL;
      if (el->kind == ast_pat_assign) {
        elem_default = el->b;
        elem_target = el->a;
      }
      char idxbuf[16];
      snprintf(idxbuf, sizeof(idxbuf), "%d", idx);
      uint16_t key_idx = cf_string(c->cf, idxbuf);
      emit_aload(c->m, src_slot);
      op_emit2(c->m, op_ldc_w, key_idx);
      op_emit2(c->m, op_invokevirtual, getprop_idx);
      if (elem_default)
        cg_emit_pattern_default(c, elem_default);
      cg_bind_target_value_on_stack(c, elem_target, kind);
      idx++;
    }
    return;
  }
  if (pattern->kind == ast_pat_object) {
    for (int i = 0; i < pattern->props.len; i++) {
      ast_prop* pr = &pattern->props.items[i];
      ast_node* elem_target = pr->value;
      ast_node* elem_default = NULL;
      if (elem_target->kind == ast_pat_assign) {
        elem_default = elem_target->b;
        elem_target = elem_target->a;
      }
      uint16_t key_idx = cf_string(c->cf, pr->key->str);
      emit_aload(c->m, src_slot);
      op_emit2(c->m, op_ldc_w, key_idx);
      op_emit2(c->m, op_invokevirtual, getprop_idx);
      if (elem_default)
        cg_emit_pattern_default(c, elem_default);
      cg_bind_target_value_on_stack(c, elem_target, kind);
    }
    return;
  }
}

static void cg_var_declarator(compiler* c, ast_node* item, tok_kind kind) {
  ast_node* target = item;
  ast_node* init = NULL;
  if (item->kind == ast_pat_assign) {
    target = item->a;
    init = item->b;
  }

  if (target->kind == ast_pat_ident) {
    if (kind == tok_kw_var) {
      var_ref vr = resolve_var(c, target->str, target->str_len);
      if (vr.kind == var_not_found) {
        cg_error("internal: hoisted var missing", target->line);
        return;
      }
      if (init) {
        cg_expr(c, init);
        emit_var_write_ref(c, vr);
        op_emit(c->m, op_pop);
      }
      return;
    }
    if (c->brace_depth == 0) {
      local* le = find_local_entry(c, target->str, target->str_len);
      if (le) {
        if (init) {
          cg_expr(c, init);
          var_ref vr;
          vr.kind = var_local;
          vr.index = le->slot;
          emit_var_write_ref(c, vr);
          op_emit(c->m, op_pop);
        }
        return;
      }
    }
    if (init)
      cg_expr(c, init);
    else
      emit_undef(c->cf, c->m);
    uint16_t slot = next_declared_slot(c);
    emit_var_declare(c, slot);
    tok t = mktok_from_ident(target);
    add_local(c, t, slot, 0, kind == tok_kw_const);
    return;
  }

  if (!init) {
    cg_error("missing initializer in destructuring declaration", target->line);
    return;
  }
  cg_expr(c, init);
  uint16_t src_slot = c->next_local_slot++;
  emit_astore(c->m, src_slot);
  cg_bind_pattern_from_slot(c, target, kind, src_slot);
}

static void cg_bind_simple_param(compiler* fc, ast_node* pat_ident,
                                 ast_node* default_expr, int idx) {
  uint16_t slot = next_declared_slot(fc);
  emit_aload(fc->m, 2);
  emit_iconst(fc->m, idx);
  uint16_t argat_idx =
      cf_methodref(fc->cf, "V6Value", "argAt", "([LV6Value;I)LV6Value;");
  op_emit2(fc->m, op_invokestatic, argat_idx);

  if (default_expr) {
    op_emit(fc->m, op_dup);
    uint16_t isundef_idx =
        cf_methodref(fc->cf, "V6Value", "isUndefined", "()Z");
    op_emit2(fc->m, op_invokevirtual, isundef_idx);
    size_t has_val_jump = op_pos(fc->m);
    op_emit2(fc->m, op_ifeq, 0);
    op_emit(fc->m, op_pop);
    cg_expr(fc, default_expr);
    size_t end_jump = op_pos(fc->m);
    op_emit2(fc->m, op_goto, 0);
    size_t has_val_pos = op_pos(fc->m);
    op_patch2(fc->m, (uint16_t)(has_val_jump + 1),
              (uint16_t)(has_val_pos - has_val_jump));
    size_t end_pos = op_pos(fc->m);
    op_patch2(fc->m, (uint16_t)(end_jump + 1), (uint16_t)(end_pos - end_jump));
  }

  emit_var_declare(fc, slot);
  if (fc->param_count < v6_max_params) {
    fc->params[fc->param_count].name = pat_ident->str;
    fc->params[fc->param_count].len = pat_ident->str_len;
    fc->params[fc->param_count].slot = slot;
    fc->param_count++;
  }
}

static void cg_bind_rest_param(compiler* fc, ast_node* pat_ident, int idx) {
  uint16_t slot = next_declared_slot(fc);
  emit_aload(fc->m, 2);
  emit_iconst(fc->m, idx);
  uint16_t restargs_idx = cf_methodref(fc->cf, "V6Object", "restFromArgs",
                                       "([LV6Value;I)LV6Array;");
  op_emit2(fc->m, op_invokestatic, restargs_idx);
  emit_box_object_ref(fc);
  emit_var_declare(fc, slot);
  if (fc->param_count < v6_max_params) {
    fc->params[fc->param_count].name = pat_ident->str;
    fc->params[fc->param_count].len = pat_ident->str_len;
    fc->params[fc->param_count].slot = slot;
    fc->param_count++;
  }
}

static void cg_hoist_and_block(compiler* c, ast_node* block) {
  ast_hoist_scope(c, &block->list);
  cg_block(c, block);
}

void ast_codegen_function_value(compiler* c, ast_node* fn,
                                char* out_lambda_name) {
  int id = (*c->lambda_counter)++;
  char mname[24];
  snprintf(mname, sizeof(mname), "lambda%d", id);
  if (out_lambda_name)
    strcpy(out_lambda_name, mname);

  method* m = cf_method(c->cf, acc_static, mname,
                        "([LV6Ref;LV6Value;[LV6Value;)LV6Value;");
  m->max_stack = 64;

  compiler fc;
  fc.cf = c->cf;
  fc.m = m;
  fc.parent = c;
  fc.lambda_counter = c->lambda_counter;
  fc.is_arrow = fn->flag_a;
  fc.param_count = 0;
  fc.locals = malloc(sizeof(local) * v6_initial_locals);
  fc.local_count = 0;
  fc.local_cap = v6_initial_locals;
  fc.use_frame_locals = 0;
  fc.frame_slot = 0;
  fc.next_frame_slot = 0;
  fc.chunk_id = 0;
  fc.scratch_slot = 3;
  fc.next_local_slot = 5;
  fc.upvalues = malloc(sizeof(upvalue) * v6_initial_upvalues);
  fc.upvalue_count = 0;
  fc.upvalue_cap = v6_initial_upvalues;
  fc.break_depth = 0;
  fc.continue_depth = 0;
  fc.catch_depth = 0;
  fc.brace_depth = -1;
  fc.super_name = c->super_name;
  fc.super_len = c->super_len;
  fc.class_name = c->class_name;
  fc.class_name_len = c->class_name_len;
  fc.pending_field_count = 0;
  fc.box_locals = 1;
  fc.label_count = 0;
  fc.pending_label_count = 0;
  fc.finally_depth = 0;
  fc.is_async_gen = c->pending_async_gen;
  fc.pending_async_gen = 0;
  c->pending_async_gen = 0;
  fc.is_module = c->is_module;
  fc.this_class_name = c->this_class_name;
  fc.modctx = c->modctx;
  fc.module_dir = c->module_dir;

  int idx = 0;
  for (int i = 0; i < fn->params.len; i++) {
    ast_param* pr = &fn->params.items[i];
    if (pr->is_rest) {
      cg_bind_rest_param(&fc, pr->pattern, idx);
      idx++;
      continue;
    }
    ast_node* target = pr->pattern;
    ast_node* deflt = NULL;
    if (target->kind == ast_pat_assign) {
      deflt = target->b;
      target = target->a;
    }
    if (target->kind == ast_pat_ident) {
      cg_bind_simple_param(&fc, target, deflt, idx);
    } else {
      emit_aload(fc.m, 2);
      emit_iconst(fc.m, idx);
      uint16_t argat_idx =
          cf_methodref(fc.cf, "V6Value", "argAt", "([LV6Value;I)LV6Value;");
      op_emit2(fc.m, op_invokestatic, argat_idx);
      if (deflt)
        cg_emit_pattern_default(&fc, deflt);
      uint16_t src_slot = fc.next_local_slot++;
      emit_astore(fc.m, src_slot);
      cg_bind_pattern_from_slot(&fc, target, tok_kw_let, src_slot);
    }
    idx++;
  }

  if (!fn->flag_a) {
    uint16_t slot = next_declared_slot(&fc);
    emit_aload(fc.m, 1);
    emit_var_declare(&fc, slot);
    tok tt;
    memset(&tt, 0, sizeof(tt));
    tt.kind = tok_kw_this;
    tt.start = "this";
    tt.len = 4;
    add_local(&fc, tt, slot, 0, 0);
  }

  if (!fn->flag_a) {
    uint16_t slot = next_declared_slot(&fc);
    emit_aload(fc.m, 2);
    emit_iconst(fc.m, 0);
    uint16_t restargs_idx = cf_methodref(fc.cf, "V6Object", "restFromArgs",
                                         "([LV6Value;I)LV6Array;");
    op_emit2(fc.m, op_invokestatic, restargs_idx);
    emit_box_object_ref(&fc);
    emit_var_declare(&fc, slot);
    tok at;
    memset(&at, 0, sizeof(at));
    at.kind = tok_ident;
    at.start = "arguments";
    at.len = 9;
    add_local(&fc, at, slot, 0, 0);
  }

  if (!fn->flag_a && c->pending_field_count > 0) {
    var_ref this_vr = resolve_var(&fc, "this", 4);
    for (int i = 0; i < c->pending_field_count; i++) {
      field_init* fi = &c->pending_fields[i];
      emit_var_read_ref(&fc, this_vr);
      char keystr[192];
      size_t n =
          fi->name_len < sizeof(keystr) - 1 ? fi->name_len : sizeof(keystr) - 1;
      memcpy(keystr, fi->name, n);
      keystr[n] = '\0';
      uint16_t key_idx = cf_string(fc.cf, keystr);
      op_emit2(fc.m, op_ldc_w, key_idx);
      if (fi->init_ast) {
        cg_expr(&fc, (ast_node*)fi->init_ast);
      } else {
        emit_undef(fc.cf, fc.m);
      }
      uint16_t setprop_idx = cf_methodref(fc.cf, "V6Value", "setProp",
                                          "(Ljava/lang/String;LV6Value;)V");
      op_emit2(fc.m, op_invokevirtual, setprop_idx);
    }
  }

  if (fn->flag_a && !fn->flag_d) {
    cg_expr(&fc, fn->a);
    op_emit(fc.m, op_areturn);
  } else {
    cg_hoist_and_block(&fc, fn->a);
    emit_undef(fc.cf, fc.m);
    op_emit(fc.m, op_areturn);
  }

  fc.m->max_locals = fc.next_local_slot;

  uint16_t closure_cls = cf_class(c->cf, "V6Closure");
  uint16_t closure_ctor =
      cf_methodref(c->cf, "V6Closure", "<init>",
                   "(Ljava/lang/Class;Ljava/lang/String;[LV6Ref;)V");
  uint16_t main_cls_idx = cf_class(c->cf, c->this_class_name);
  uint16_t name_str = cf_string(c->cf, mname);

  op_emit2(c->m, op_new, closure_cls);
  op_emit(c->m, op_dup);
  op_emit2(c->m, op_ldc_w, main_cls_idx);
  op_emit2(c->m, op_ldc_w, name_str);
  emit_iconst(c->m, fc.upvalue_count);
  op_emit2(c->m, op_anewarray, cf_class(c->cf, "V6Ref"));
  for (int i = 0; i < fc.upvalue_count; i++) {
    op_emit(c->m, op_dup);
    emit_iconst(c->m, i);
    emit_ref_push(c, !fc.upvalues[i].from_parent_local,
                  fc.upvalues[i].parent_index);
    op_emit(c->m, op_aastore);
  }
  op_emit2(c->m, op_invokespecial, closure_ctor);
  emit_box_ref_computed(c, V6_TAG_FUNC);

  free(fc.locals);
  free(fc.upvalues);
}

static void cg_class_expr(compiler* c, ast_node* n, int is_stmt) {
  tok name;
  memset(&name, 0, sizeof(name));
  if (n->str_len > 0) {
    name.kind = tok_ident;
    name.start = n->str;
    name.len = n->str_len;
  } else {
    char* synth = malloc(24);
    snprintf(synth, 24, "$anonclass%d", (*c->lambda_counter)++);
    name.kind = tok_ident;
    name.start = synth;
    name.len = strlen(synth);
  }

  tok base_name;
  memset(&base_name, 0, sizeof(base_name));
  int has_base = n->flag_a;
  if (has_base) {
    cg_expr(c, n->a);
    uint16_t base_slot = next_declared_slot(c);
    emit_var_declare(c, base_slot);
    char* synth = malloc(24);
    snprintf(synth, 24, "$super%d", (*c->lambda_counter)++);
    base_name.kind = tok_ident;
    base_name.start = synth;
    base_name.len = strlen(synth);
    add_local(c, base_name, base_slot, 0, 0);
  }

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
      cg_error("undeclared variable", n->line);
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

  for (int mi = 0; mi < n->members.len; mi++) {
    ast_class_member* member = &n->members.items[mi];
    uint16_t target_slot = member->is_static ? cls_tmp : proto_tmp;

    if (member->is_getter || member->is_setter) {
      emit_aload(c->m, target_slot);
      if (member->computed) {
        cg_expr(c, member->key);
        uint16_t tostring_idx =
            cf_methodref(c->cf, "V6Value", "toString", "()Ljava/lang/String;");
        op_emit2(c->m, op_invokevirtual, tostring_idx);
      } else {
        uint16_t mkey_idx = cf_string(c->cf, member->key->str);
        op_emit2(c->m, op_ldc_w, mkey_idx);
      }
      ast_codegen_function_value(c, member->value, NULL);
      uint16_t ascall_idx =
          cf_methodref(c->cf, "V6Value", "asCallable", "()LV6Callable;");
      op_emit2(c->m, op_invokevirtual, ascall_idx);
      uint16_t def_idx =
          cf_methodref(c->cf, "V6Object",
                       member->is_getter ? "defineGetter" : "defineSetter",
                       "(Ljava/lang/String;LV6Callable;)V");
      op_emit2(c->m, op_invokevirtual, def_idx);
    } else if (member->is_ctor) {
      emit_aload(c->m, cls_tmp);
      ctor_lambda_name = malloc(24);
      for (int i = 0; i < pending_instance_field_count; i++)
        c->pending_fields[i] = pending_instance_fields[i];
      c->pending_field_count = pending_instance_field_count;
      ast_codegen_function_value(c, member->value, ctor_lambda_name);
      c->pending_field_count = 0;
      uint16_t ascall_idx =
          cf_methodref(c->cf, "V6Value", "asCallable", "()LV6Callable;");
      op_emit2(c->m, op_invokevirtual, ascall_idx);
      uint16_t ctor_field =
          cf_fieldref(c->cf, "V6Class", "ctor", "LV6Callable;");
      op_emit2(c->m, op_putfield, ctor_field);
    } else if (!member->is_field) {
      emit_aload(c->m, target_slot);
      if (member->computed) {
        cg_expr(c, member->key);
        uint16_t tostring_idx =
            cf_methodref(c->cf, "V6Value", "toString", "()Ljava/lang/String;");
        op_emit2(c->m, op_invokevirtual, tostring_idx);
      } else {
        uint16_t mkey_idx = cf_string(c->cf, member->key->str);
        op_emit2(c->m, op_ldc_w, mkey_idx);
      }
      ast_codegen_function_value(c, member->value, NULL);
      if (member->is_generator && member->is_async) {
        cg_error("async generator methods are not supported", n->line);
        return;
      } else if (member->is_generator) {
        emit_wrap_generator(c);
      } else if (member->is_async) {
        emit_wrap_async(c);
      }
      op_emit2(c->m, op_invokevirtual, set_idx);
    } else if (member->computed) {
      cg_error("computed field names are not supported", n->line);
      return;
    } else {
      if (member->is_static) {
        emit_aload(c->m, cls_tmp);
        uint16_t mkey_idx = cf_string(c->cf, member->key->str);
        op_emit2(c->m, op_ldc_w, mkey_idx);
        if (member->value)
          cg_expr(c, member->value);
        else
          emit_undef(c->cf, c->m);
        op_emit2(c->m, op_invokevirtual, set_idx);
      } else if (pending_instance_field_count < v6_max_fields) {
        pending_instance_fields[pending_instance_field_count].name =
            member->key->str;
        pending_instance_fields[pending_instance_field_count].name_len =
            member->key->str_len;
        pending_instance_fields[pending_instance_field_count].init_src = NULL;
        pending_instance_fields[pending_instance_field_count].init_ast =
            member->value;
        pending_instance_field_count++;
      }
    }
  }

  if ((has_base || pending_instance_field_count > 0) && !ctor_lambda_name) {
    emit_aload(c->m, cls_tmp);
    ctor_lambda_name = malloc(24);
    for (int i = 0; i < pending_instance_field_count; i++)
      c->pending_fields[i] = pending_instance_fields[i];
    c->pending_field_count = pending_instance_field_count;

    ast_node synth_fn;
    memset(&synth_fn, 0, sizeof(synth_fn));
    synth_fn.kind = ast_func_expr;
    synth_fn.flag_d = 1;
    ast_node synth_body;
    memset(&synth_body, 0, sizeof(synth_body));
    synth_body.kind = ast_block;
    synth_fn.a = &synth_body;

    ast_node super_call_stmt;
    ast_node super_call_expr;
    ast_node spread_arg;
    ast_param rest_param;
    ast_node rest_ident;
    if (has_base) {
      memset(&rest_ident, 0, sizeof(rest_ident));
      rest_ident.kind = ast_pat_ident;
      rest_ident.str = "args";
      rest_ident.str_len = 4;
      rest_param.is_rest = 1;
      rest_param.pattern = &rest_ident;
      ast_param_list_push(g_synth_arena(), &synth_fn.params, rest_param);

      memset(&super_call_expr, 0, sizeof(super_call_expr));
      super_call_expr.kind = ast_super_call;
      memset(&spread_arg, 0, sizeof(spread_arg));
      spread_arg.kind = ast_spread;
      ast_node args_ident;
      memset(&args_ident, 0, sizeof(args_ident));
      args_ident.kind = ast_ident;
      args_ident.str = "args";
      args_ident.str_len = 4;
      ast_node* args_ident_p = ast_synth_dup(&args_ident);
      spread_arg.a = args_ident_p;
      ast_list_push(g_synth_arena(), &super_call_expr.list,
                    ast_synth_dup(&spread_arg));

      memset(&super_call_stmt, 0, sizeof(super_call_stmt));
      super_call_stmt.kind = ast_expr_stmt;
      super_call_stmt.a = ast_synth_dup(&super_call_expr);
      ast_list_push(g_synth_arena(), &synth_body.list,
                    ast_synth_dup(&super_call_stmt));
    }

    ast_codegen_function_value(c, &synth_fn, ctor_lambda_name);
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

  if (!is_stmt)
    return;

  if (c->brace_depth == 0) {
    var_ref vr = resolve_var(c, name.start, name.len);
    if (vr.kind == var_not_found) {
      cg_error("internal: hoisted class missing", n->line);
      return;
    }
    emit_var_write_ref(c, vr);
    op_emit(c->m, op_pop);
  } else {
    uint16_t slot = next_declared_slot(c);
    emit_var_declare(c, slot);
    add_local(c, name, slot, 0, 0);
  }
}

static ast_arena g_synth_arena_storage;
static int g_synth_arena_init = 0;

static ast_arena* g_synth_arena(void) {
  if (!g_synth_arena_init) {
    ast_arena_init(&g_synth_arena_storage);
    g_synth_arena_init = 1;
  }
  return &g_synth_arena_storage;
}

static ast_node* ast_synth_dup(ast_node* src) {
  ast_node* n = ast_arena_alloc(g_synth_arena(), sizeof(ast_node));
  *n = *src;
  return n;
}

static void push_loop(compiler* c, size_t continue_target) {
  c->breaks[c->break_depth].count = 0;
  c->break_depth++;
  c->continues[c->continue_depth] = continue_target;
  int continue_idx = c->continue_depth;
  c->continue_depth++;
  for (int i = 0; i < c->pending_label_count; i++) {
    for (int j = c->label_count - 1; j >= 0; j--) {
      if (c->label_lens[j] == c->pending_label_lens[i] &&
          memcmp(c->label_names[j], c->pending_label_names[i],
                 c->label_lens[j]) == 0 &&
          c->label_continue_depth[j] == -1) {
        c->label_continue_depth[j] = continue_idx;
        break;
      }
    }
  }
}

static void patch_breaks(compiler* c, size_t end_pos) {
  break_ctx* bc = &c->breaks[c->break_depth - 1];
  for (size_t i = 0; i < bc->count; i++)
    op_patch2(c->m, (uint16_t)(bc->jumps[i] + 1),
              (uint16_t)(end_pos - bc->jumps[i]));
}

static void pop_loop(compiler* c, size_t end_pos) {
  patch_breaks(c, end_pos);
  c->break_depth--;
  c->continue_depth--;
}

static void emit_inline_finally_at(compiler* c, int idx) {
  int saved_depth = c->finally_depth;
  c->finally_depth = idx;
  cg_block(c, (ast_node*)c->finally_ast[idx]);
  c->finally_depth = saved_depth;
}

static void emit_all_pending_finally(compiler* c) {
  for (int i = c->finally_depth - 1; i >= 0; i--)
    emit_inline_finally_at(c, i);
}

static void emit_pending_finally_for_break(compiler* c, int target_break_idx) {
  for (int i = c->finally_depth - 1; i >= 0; i--) {
    if (target_break_idx >= c->finally_break_depth[i])
      break;
    emit_inline_finally_at(c, i);
  }
}

static void emit_pending_finally_for_continue(compiler* c,
                                              int target_continue_idx) {
  for (int i = c->finally_depth - 1; i >= 0; i--) {
    if (target_continue_idx >= c->finally_continue_depth[i])
      break;
    emit_inline_finally_at(c, i);
  }
}

static void cg_for_in(compiler* c, ast_node* s) {
  cg_expr(c, s->b);

  uint16_t keys_slot = c->next_local_slot++;
  uint16_t idx_slot = c->next_local_slot++;
  uint16_t len_slot = c->next_local_slot++;

  uint16_t enumkeys_idx =
      cf_methodref(c->cf, "V6Value", "enumKeys", "()LV6Value;");
  op_emit2(c->m, op_invokevirtual, enumkeys_idx);
  emit_astore(c->m, keys_slot);

  emit_box_const(c->cf, c->m, op_iconst_0, op_dconst_0);
  emit_astore(c->m, idx_slot);

  uint16_t length_str = cf_string(c->cf, "length");
  uint16_t getprop_idx = cf_methodref(c->cf, "V6Value", "getProp",
                                      "(Ljava/lang/String;)LV6Value;");
  emit_aload(c->m, keys_slot);
  op_emit2(c->m, op_ldc_w, length_str);
  op_emit2(c->m, op_invokevirtual, getprop_idx);
  emit_astore(c->m, len_slot);

  size_t cond_pos = op_pos(c->m);
  emit_aload(c->m, idx_slot);
  emit_to_number(c);
  emit_aload(c->m, len_slot);
  emit_to_number(c);
  op_emit(c->m, op_dcmpg);
  size_t exit_jump = op_pos(c->m);
  op_emit2(c->m, op_ifge, 0);

  size_t body_jump = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);

  size_t inc_pos = op_pos(c->m);
  emit_aload(c->m, idx_slot);
  emit_to_number(c);
  op_emit(c->m, op_dconst_1);
  op_emit(c->m, op_dadd);
  emit_box_tag(c, op_iconst_0);
  emit_astore(c->m, idx_slot);
  size_t inc_to_cond = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);
  op_patch2(c->m, (uint16_t)(inc_to_cond + 1),
            (uint16_t)(cond_pos - inc_to_cond));

  size_t body_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(body_jump + 1), (uint16_t)(body_pos - body_jump));

  int saved_count = c->local_count;
  var_ref var_vr;
  var_vr.kind = var_local;
  var_vr.index = 0;
  if (s->flag_a == tok_kw_var) {
    var_vr = resolve_var(c, s->a->str, s->a->str_len);
    if (var_vr.kind == var_not_found) {
      cg_error("internal: hoisted var missing", s->line);
      return;
    }
  } else {
    var_vr.index = next_declared_slot(c);
    tok t = mktok_from_ident(s->a);
    add_local(c, t, var_vr.index, 0, s->flag_a == tok_kw_const);
  }

  uint16_t tostring_idx =
      cf_methodref(c->cf, "V6Value", "toString", "()Ljava/lang/String;");
  emit_aload(c->m, keys_slot);
  emit_aload(c->m, idx_slot);
  op_emit2(c->m, op_invokevirtual, tostring_idx);
  op_emit2(c->m, op_invokevirtual, getprop_idx);
  if (s->flag_a == tok_kw_var) {
    emit_var_write_ref(c, var_vr);
    op_emit(c->m, op_pop);
  } else {
    emit_var_declare(c, var_vr.index);
  }

  push_loop(c, inc_pos);
  cg_stmt(c, s->c);

  size_t body_to_inc = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);
  op_patch2(c->m, (uint16_t)(body_to_inc + 1),
            (uint16_t)(inc_pos - body_to_inc));

  size_t end_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(exit_jump + 1), (uint16_t)(end_pos - exit_jump));
  pop_loop(c, end_pos);

  for (int i = saved_count; i < c->local_count; i++)
    if (!c->locals[i].is_var)
      c->locals[i].dead = 1;
}

static void cg_for_of(compiler* c, ast_node* s) {
  if (s->flag_b) {
    uint16_t iterable_slot = c->next_local_slot++;
    cg_expr(c, s->b);
    emit_astore(c->m, iterable_slot);

    uint16_t next_str = cf_string(c->cf, "next");
    uint16_t value_str = cf_string(c->cf, "value");
    uint16_t done_str = cf_string(c->cf, "done");
    uint16_t getprop_idx = cf_methodref(c->cf, "V6Value", "getProp",
                                        "(Ljava/lang/String;)LV6Value;");
    uint16_t call_idx = cf_methodref(c->cf, "V6Value", "call",
                                     "(LV6Value;[LV6Value;)LV6Value;");
    uint16_t await_idx = cf_methodref(
        c->cf, "V6Generator", c->is_async_gen ? "currentAwait" : "currentYield",
        "(LV6Value;)LV6Value;");

    size_t cond_pos = op_pos(c->m);
    emit_aload(c->m, iterable_slot);
    op_emit2(c->m, op_ldc_w, next_str);
    op_emit2(c->m, op_invokevirtual, getprop_idx);
    emit_aload(c->m, iterable_slot);
    emit_iconst(c->m, 0);
    op_emit2(c->m, op_anewarray, value_class(c->cf));
    op_emit2(c->m, op_invokevirtual, call_idx);
    op_emit2(c->m, op_invokestatic, await_idx);
    uint16_t result_slot = c->next_local_slot++;
    emit_astore(c->m, result_slot);

    emit_aload(c->m, result_slot);
    op_emit2(c->m, op_ldc_w, done_str);
    op_emit2(c->m, op_invokevirtual, getprop_idx);
    emit_truthy(c);
    size_t exit_jump = op_pos(c->m);
    op_emit2(c->m, op_ifne, 0);

    int saved_count = c->local_count;
    var_ref var_vr;
    var_vr.kind = var_local;
    var_vr.index = 0;
    if (s->flag_a == tok_kw_var) {
      var_vr = resolve_var(c, s->a->str, s->a->str_len);
      if (var_vr.kind == var_not_found) {
        cg_error("internal: hoisted var missing", s->line);
        return;
      }
    } else {
      var_vr.index = next_declared_slot(c);
      tok t = mktok_from_ident(s->a);
      add_local(c, t, var_vr.index, 0, s->flag_a == tok_kw_const);
    }

    emit_aload(c->m, result_slot);
    op_emit2(c->m, op_ldc_w, value_str);
    op_emit2(c->m, op_invokevirtual, getprop_idx);
    if (s->flag_a == tok_kw_var) {
      emit_var_write_ref(c, var_vr);
      op_emit(c->m, op_pop);
    } else {
      emit_var_declare(c, var_vr.index);
    }

    push_loop(c, cond_pos);
    cg_stmt(c, s->c);

    size_t back_jump = op_pos(c->m);
    op_emit2(c->m, op_goto, 0);
    op_patch2(c->m, (uint16_t)(back_jump + 1),
              (uint16_t)(cond_pos - back_jump));

    size_t end_pos2 = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(exit_jump + 1),
              (uint16_t)(end_pos2 - exit_jump));
    pop_loop(c, end_pos2);

    for (int i = saved_count; i < c->local_count; i++)
      if (!c->locals[i].is_var)
        c->locals[i].dead = 1;
    return;
  }

  uint16_t iter_slot = c->next_local_slot++;
  uint16_t iter_cls = cf_class(c->cf, "V6Iterator");
  uint16_t iter_ctor =
      cf_methodref(c->cf, "V6Iterator", "<init>", "(LV6Value;)V");

  op_emit2(c->m, op_new, iter_cls);
  op_emit(c->m, op_dup);
  cg_expr(c, s->b);
  op_emit2(c->m, op_invokespecial, iter_ctor);
  emit_astore(c->m, iter_slot);

  uint16_t has_next_idx = cf_methodref(c->cf, "V6Iterator", "hasNext", "()Z");
  uint16_t next_idx = cf_methodref(c->cf, "V6Iterator", "next", "()LV6Value;");

  size_t cond_pos = op_pos(c->m);
  emit_aload(c->m, iter_slot);
  op_emit2(c->m, op_invokevirtual, has_next_idx);
  size_t exit_jump = op_pos(c->m);
  op_emit2(c->m, op_ifeq, 0);

  int saved_count = c->local_count;

  emit_aload(c->m, iter_slot);
  op_emit2(c->m, op_invokevirtual, next_idx);

  if (s->a->kind == ast_pat_ident) {
    var_ref var_vr;
    var_vr.kind = var_local;
    var_vr.index = 0;
    if (s->flag_a == tok_kw_var) {
      var_vr = resolve_var(c, s->a->str, s->a->str_len);
      if (var_vr.kind == var_not_found) {
        cg_error("internal: hoisted var missing", s->line);
        return;
      }
      emit_var_write_ref(c, var_vr);
      op_emit(c->m, op_pop);
    } else {
      var_vr.index = next_declared_slot(c);
      tok t = mktok_from_ident(s->a);
      add_local(c, t, var_vr.index, 0, s->flag_a == tok_kw_const);
      emit_var_declare(c, var_vr.index);
    }
  } else {
    uint16_t val_slot = c->next_local_slot++;
    emit_astore(c->m, val_slot);
    cg_bind_pattern_from_slot(c, s->a, s->flag_a, val_slot);
  }

  push_loop(c, cond_pos);
  cg_stmt(c, s->c);

  size_t back_jump = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);
  op_patch2(c->m, (uint16_t)(back_jump + 1), (uint16_t)(cond_pos - back_jump));

  size_t end_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(exit_jump + 1), (uint16_t)(end_pos - exit_jump));
  pop_loop(c, end_pos);

  for (int i = saved_count; i < c->local_count; i++)
    if (!c->locals[i].is_var)
      c->locals[i].dead = 1;
}

static void cg_stmt(compiler* c, ast_node* s) {
  switch (s->kind) {
  case ast_empty:
  case ast_debugger:
    break;
  case ast_block:
    cg_block(c, s);
    break;
  case ast_func_decl:
    if (c->brace_depth != 0) {
      uint16_t slot = next_declared_slot(c);
      emit_undef(c->cf, c->m);
      emit_var_declare(c, slot);
      tok t = mktok_from_ident(s);
      add_local(c, t, slot, 0, 0);
      char lambda_name[24];
      ast_codegen_function_value(c, s, lambda_name);
      if (s->flag_c)
        emit_wrap_generator(c);
      var_ref vr = resolve_var(c, s->str, s->str_len);
      emit_var_write_ref(c, vr);
      op_emit(c->m, op_pop);
    }
    break;
  case ast_class_decl:
    cg_class_expr(c, s, 1);
    break;
  case ast_import: {
    parser subp;
    parser_init(&subp, s->raw_src);
    parse_import_stmt(&subp, c);
    if (subp.had_error)
      cg_error(subp.err_msg, subp.err_line);
    break;
  }
  case ast_expr_stmt:
    cg_expr(c, s->a);
    op_emit(c->m, op_pop);
    break;
  case ast_var_decl:
    for (int i = 0; i < s->list.len; i++)
      cg_var_declarator(c, s->list.items[i], s->flag_a);
    break;
  case ast_if: {
    cg_expr(c, s->a);
    emit_truthy(c);
    size_t else_jump = op_pos(c->m);
    op_emit2(c->m, op_ifeq, 0);
    cg_stmt(c, s->b);
    size_t end_jump = op_pos(c->m);
    op_emit2(c->m, op_goto, 0);
    size_t else_pos = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(else_jump + 1),
              (uint16_t)(else_pos - else_jump));
    if (s->c)
      cg_stmt(c, s->c);
    size_t end_pos = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(end_jump + 1), (uint16_t)(end_pos - end_jump));
    break;
  }
  case ast_while: {
    size_t start_pos = op_pos(c->m);
    cg_expr(c, s->a);
    emit_truthy(c);
    size_t exit_jump = op_pos(c->m);
    op_emit2(c->m, op_ifeq, 0);
    push_loop(c, start_pos);
    cg_stmt(c, s->b);
    size_t back_jump = op_pos(c->m);
    op_emit2(c->m, op_goto, 0);
    op_patch2(c->m, (uint16_t)(back_jump + 1),
              (uint16_t)(start_pos - back_jump));
    size_t end_pos = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(exit_jump + 1), (uint16_t)(end_pos - exit_jump));
    pop_loop(c, end_pos);
    break;
  }
  case ast_do_while: {
    size_t entry_jump = op_pos(c->m);
    op_emit2(c->m, op_goto, 0);
    size_t cond_pos = op_pos(c->m);
    cg_expr(c, s->b);
    emit_truthy(c);
    size_t exit_jump = op_pos(c->m);
    op_emit2(c->m, op_ifeq, 0);
    size_t loop_back_jump = op_pos(c->m);
    op_emit2(c->m, op_goto, 0);
    size_t body_pos = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(entry_jump + 1),
              (uint16_t)(body_pos - entry_jump));
    op_patch2(c->m, (uint16_t)(loop_back_jump + 1),
              (uint16_t)(body_pos - loop_back_jump));
    push_loop(c, cond_pos);
    cg_stmt(c, s->a);
    size_t body_to_cond = op_pos(c->m);
    op_emit2(c->m, op_goto, 0);
    op_patch2(c->m, (uint16_t)(body_to_cond + 1),
              (uint16_t)(cond_pos - body_to_cond));
    size_t end_pos = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(exit_jump + 1), (uint16_t)(end_pos - exit_jump));
    pop_loop(c, end_pos);
    break;
  }
  case ast_for: {
    int saved_count = c->local_count;
    if (s->a) {
      if (s->a->kind == ast_var_decl) {
        for (int i = 0; i < s->a->list.len; i++)
          cg_var_declarator(c, s->a->list.items[i], s->a->flag_a);
      } else {
        cg_expr(c, s->a);
        op_emit(c->m, op_pop);
      }
    }
    size_t cond_pos = op_pos(c->m);
    int has_cond = s->b != NULL;
    size_t exit_jump = 0;
    if (has_cond) {
      cg_expr(c, s->b);
      emit_truthy(c);
      exit_jump = op_pos(c->m);
      op_emit2(c->m, op_ifeq, 0);
    }
    size_t body_jump = op_pos(c->m);
    op_emit2(c->m, op_goto, 0);
    size_t inc_pos = op_pos(c->m);
    if (s->c) {
      cg_expr(c, s->c);
      op_emit(c->m, op_pop);
    }
    size_t inc_to_cond = op_pos(c->m);
    op_emit2(c->m, op_goto, 0);
    op_patch2(c->m, (uint16_t)(inc_to_cond + 1),
              (uint16_t)(cond_pos - inc_to_cond));
    size_t body_pos = op_pos(c->m);
    op_patch2(c->m, (uint16_t)(body_jump + 1),
              (uint16_t)(body_pos - body_jump));
    push_loop(c, inc_pos);
    cg_stmt(c, s->d);
    size_t body_to_inc = op_pos(c->m);
    op_emit2(c->m, op_goto, 0);
    op_patch2(c->m, (uint16_t)(body_to_inc + 1),
              (uint16_t)(inc_pos - body_to_inc));
    size_t end_pos = op_pos(c->m);
    if (has_cond)
      op_patch2(c->m, (uint16_t)(exit_jump + 1),
                (uint16_t)(end_pos - exit_jump));
    pop_loop(c, end_pos);
    for (int i = saved_count; i < c->local_count; i++)
      if (!c->locals[i].is_var)
        c->locals[i].dead = 1;
    break;
  }
  case ast_for_in:
    cg_for_in(c, s);
    break;
  case ast_for_of:
    cg_for_of(c, s);
    break;
  case ast_switch: {
    cg_expr(c, s->a);
    emit_astore(c->m, c->scratch_slot);

    c->breaks[c->break_depth].count = 0;
    c->break_depth++;

    size_t prev_case_jump = 0;
    int have_prev_case_jump = 0;

    for (int i = 0; i < s->cases.len; i++) {
      ast_switch_case* sc = &s->cases.items[i];
      if (sc->test) {
        emit_aload(c->m, c->scratch_slot);
        cg_expr(c, sc->test);
        uint16_t idx = cf_methodref(c->cf, "V6Value", "strictEquals",
                                    "(LV6Value;LV6Value;)Z");
        op_emit2(c->m, op_invokestatic, idx);
        size_t skip_jump = op_pos(c->m);
        op_emit2(c->m, op_ifeq, 0);

        if (have_prev_case_jump) {
          size_t here = op_pos(c->m);
          op_patch2(c->m, (uint16_t)(prev_case_jump + 1),
                    (uint16_t)(here - prev_case_jump));
          have_prev_case_jump = 0;
        }

        cg_stmt_list(c, &sc->body);

        prev_case_jump = op_pos(c->m);
        op_emit2(c->m, op_goto, 0);
        have_prev_case_jump = 1;

        size_t after = op_pos(c->m);
        op_patch2(c->m, (uint16_t)(skip_jump + 1),
                  (uint16_t)(after - skip_jump));
      } else {
        if (have_prev_case_jump) {
          size_t here = op_pos(c->m);
          op_patch2(c->m, (uint16_t)(prev_case_jump + 1),
                    (uint16_t)(here - prev_case_jump));
          have_prev_case_jump = 0;
        }
        cg_stmt_list(c, &sc->body);
      }
    }

    if (have_prev_case_jump) {
      size_t here = op_pos(c->m);
      op_patch2(c->m, (uint16_t)(prev_case_jump + 1),
                (uint16_t)(here - prev_case_jump));
    }

    size_t end_pos = op_pos(c->m);
    patch_breaks(c, end_pos);
    c->break_depth--;
    break;
  }
  case ast_try: {
    int pushed_finally = 0;
    if (s->flag_b && c->finally_depth < v6_max_pending_finally) {
      c->finally_ast[c->finally_depth] = s->d;
      c->finally_break_depth[c->finally_depth] = c->break_depth;
      c->finally_continue_depth[c->finally_depth] = c->continue_depth;
      c->finally_depth++;
      pushed_finally = 1;
    }

    size_t try_start = op_pos(c->m);
    cg_block(c, s->a);
    size_t try_end = op_pos(c->m);

    size_t goto_after_try = 0;
    if (s->flag_a) {
      goto_after_try = op_pos(c->m);
      op_emit2(c->m, op_goto, 0);

      size_t catch_handler_pc = op_pos(c->m);
      uint16_t throw_cls = cf_class(c->cf, "V6Throw");
      method_add_exception(c->m, (uint16_t)try_start, (uint16_t)try_end,
                           (uint16_t)catch_handler_pc, throw_cls);

      int catch_scope_saved = c->local_count;
      uint16_t value_field =
          cf_fieldref(c->cf, "V6Throw", "value", "LV6Value;");
      op_emit2(c->m, op_getfield, value_field);
      if (s->b) {
        uint16_t err_slot = next_declared_slot(c);
        emit_var_declare(c, err_slot);
        tok t = mktok_from_ident(s->b);
        add_local(c, t, err_slot, 0, 0);
      } else {
        op_emit(c->m, op_pop);
      }

      cg_block(c, s->c);

      for (int i = catch_scope_saved; i < c->local_count; i++)
        if (!c->locals[i].is_var)
          c->locals[i].dead = 1;

      size_t normal_after = op_pos(c->m);
      op_patch2(c->m, (uint16_t)(goto_after_try + 1),
                (uint16_t)(normal_after - goto_after_try));
    }

    if (pushed_finally)
      c->finally_depth--;

    if (s->flag_b) {
      size_t guard_start = try_start;
      size_t guard_end = op_pos(c->m);

      cg_block(c, s->d);

      size_t skip_guard_jump = op_pos(c->m);
      op_emit2(c->m, op_goto, 0);

      size_t guard_handler_pc = op_pos(c->m);
      uint16_t guard_scratch = c->next_local_slot++;
      emit_astore(c->m, guard_scratch);

      cg_block(c, s->d);

      emit_aload(c->m, guard_scratch);
      op_emit(c->m, op_athrow);

      size_t after_guard = op_pos(c->m);
      op_patch2(c->m, (uint16_t)(skip_guard_jump + 1),
                (uint16_t)(after_guard - skip_guard_jump));

      method_add_exception(c->m, (uint16_t)guard_start, (uint16_t)guard_end,
                           (uint16_t)guard_handler_pc, 0);
    }
    break;
  }
  case ast_throw: {
    cg_expr(c, s->a);
    uint16_t throw_cls = cf_class(c->cf, "V6Throw");
    uint16_t throw_ctor =
        cf_methodref(c->cf, "V6Throw", "<init>", "(LV6Value;)V");
    op_emit2(c->m, op_new, throw_cls);
    op_emit(c->m, op_dup_x1);
    op_emit(c->m, op_swap);
    op_emit2(c->m, op_invokespecial, throw_ctor);
    op_emit(c->m, op_athrow);
    break;
  }
  case ast_return: {
    if (s->a)
      cg_expr(c, s->a);
    else
      emit_undef(c->cf, c->m);
    emit_all_pending_finally(c);
    op_emit(c->m, op_areturn);
    break;
  }
  case ast_break: {
    if (s->str_len > 0) {
      int found = -1;
      for (int i = c->label_count - 1; i >= 0; i--) {
        if (c->label_lens[i] == s->str_len &&
            memcmp(c->label_names[i], s->str, s->str_len) == 0) {
          found = i;
          break;
        }
      }
      if (found < 0) {
        cg_error("undefined label", s->line);
      } else {
        emit_pending_finally_for_break(c, c->label_break_depth[found]);
        break_ctx* bc = &c->breaks[c->label_break_depth[found]];
        bc->jumps[bc->count++] = op_pos(c->m);
        op_emit2(c->m, op_goto, 0);
      }
    } else if (c->break_depth == 0) {
      cg_error("'break' outside loop or switch", s->line);
    } else {
      emit_pending_finally_for_break(c, c->break_depth - 1);
      break_ctx* bc = &c->breaks[c->break_depth - 1];
      bc->jumps[bc->count++] = op_pos(c->m);
      op_emit2(c->m, op_goto, 0);
    }
    break;
  }
  case ast_continue: {
    if (s->str_len > 0) {
      int found = -1;
      for (int i = c->label_count - 1; i >= 0; i--) {
        if (c->label_lens[i] == s->str_len &&
            memcmp(c->label_names[i], s->str, s->str_len) == 0) {
          found = i;
          break;
        }
      }
      if (found < 0 || c->label_continue_depth[found] < 0) {
        cg_error("undefined label or label does not denote a loop", s->line);
      } else {
        emit_pending_finally_for_continue(c, c->label_continue_depth[found]);
        size_t target = c->continues[c->label_continue_depth[found]];
        size_t here = op_pos(c->m);
        op_emit2(c->m, op_goto, 0);
        op_patch2(c->m, (uint16_t)(here + 1), (uint16_t)(target - here));
      }
    } else if (c->continue_depth == 0) {
      cg_error("'continue' outside loop", s->line);
    } else {
      emit_pending_finally_for_continue(c, c->continue_depth - 1);
      size_t target = c->continues[c->continue_depth - 1];
      size_t here = op_pos(c->m);
      op_emit2(c->m, op_goto, 0);
      op_patch2(c->m, (uint16_t)(here + 1), (uint16_t)(target - here));
    }
    break;
  }
  case ast_labeled: {
    if (c->break_depth >= v6_max_loops || c->label_count >= v6_max_labels) {
      cg_error("too many nested labels/loops", s->line);
      break;
    }
    int bidx = c->break_depth;
    c->breaks[bidx].count = 0;
    c->break_depth++;

    int lidx = c->label_count;
    c->label_names[lidx] = s->str;
    c->label_lens[lidx] = s->str_len;
    c->label_break_depth[lidx] = bidx;
    c->label_continue_depth[lidx] = -1;
    c->label_count++;

    int save_pending = c->pending_label_count;
    if (c->pending_label_count < v6_max_pending_labels) {
      c->pending_label_names[c->pending_label_count] = s->str;
      c->pending_label_lens[c->pending_label_count] = s->str_len;
      c->pending_label_count++;
    }

    cg_stmt(c, s->a);

    c->pending_label_count = save_pending;
    c->label_count = lidx;
    c->break_depth--;

    size_t end_pos = op_pos(c->m);
    break_ctx* bc = &c->breaks[bidx];
    for (size_t i = 0; i < bc->count; i++)
      op_patch2(c->m, (uint16_t)(bc->jumps[i] + 1),
                (uint16_t)(end_pos - bc->jumps[i]));
    break;
  }
  case ast_paren_pattern_assign:
    cg_expr(c, s);
    op_emit(c->m, op_pop);
    break;
  default:
    cg_expr(c, s);
    op_emit(c->m, op_pop);
    break;
  }
}

static void cg_stmt_list(compiler* c, ast_list* list) {
  for (int i = 0; i < list->len; i++)
    cg_stmt(c, list->items[i]);
}

static void cg_block(compiler* c, ast_node* block) {
  int saved_count = c->local_count;
  c->brace_depth++;
  cg_stmt_list(c, &block->list);
  c->brace_depth--;
  for (int i = saved_count; i < c->local_count; i++)
    if (!c->locals[i].is_var)
      c->locals[i].dead = 1;
}

void ast_codegen_stmt_list(compiler* c, ast_list* body) {
  cg_stmt_list(c, body);
}

void ast_codegen_stmt(compiler* c, ast_node* s) {
  cg_stmt(c, s);
}

void ast_codegen_expr(compiler* c, ast_node* e) {
  cg_expr(c, e);
}
