#include "v6/parser.h"

#include "v6/module.h"
#include "v6/internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v6/pattern.h"
#include "v6/expr.h"
#include "v6/literal.h"
#include "v6/scope.h"

void parse_one_declarator_named(parser* p, compiler* c, tok_kind kind,
                                tok name) {
  if (kind == tok_kw_var) {
    var_ref vr = resolve_var(c, name.start, name.len);
    if (vr.kind == var_not_found) {
      error_at(p, "internal: hoisted var missing");
      return;
    }
    if (match(p, tok_assign)) {
      parse_expr(p, c);
      emit_var_write_ref(c, vr);
      op_emit(c->m, op_pop);
    }
    return;
  }

  if (c->brace_depth == 0) {
    local* le = find_local_entry(c, name.start, name.len);
    if (le) {
      if (match(p, tok_assign)) {
        parse_expr(p, c);
        var_ref vr;
        vr.kind = var_local;
        vr.index = le->slot;
        emit_var_write_ref(c, vr);
        op_emit(c->m, op_pop);
      }
      return;
    }
  }

  if (match(p, tok_assign))
    parse_expr(p, c);
  else
    emit_undef(c->cf, c->m);

  uint16_t slot = next_declared_slot(c);
  emit_var_declare(c, slot);
  add_local(c, name, slot, 0, kind == tok_kw_const);
}

void skip_field_init(parser* p) {
  int save_auto_regex = p->lex.auto_regex;
  p->lex.auto_regex = 1;
  int depth = 0;
  while (!check(p, tok_eof)) {
    if (check(p, tok_lparen) || check(p, tok_lbracket) ||
        check(p, tok_lbrace)) {
      depth++;
    } else if (check(p, tok_rparen) || check(p, tok_rbracket)) {
      depth--;
    } else if (check(p, tok_rbrace)) {
      if (depth == 0) {
        p->lex.auto_regex = save_auto_regex;
        return;
      }
      depth--;
    } else if (check(p, tok_semi) && depth == 0) {
      advance(p);
      p->lex.auto_regex = save_auto_regex;
      return;
    }
    advance(p);
  }
  p->lex.auto_regex = save_auto_regex;
}

static void bind_pattern_target(parser* p, compiler* c, tok_kind kind,
                                tok name) {
  if (kind == tok_kw_var) {
    var_ref vr = resolve_var(c, name.start, name.len);
    if (vr.kind == var_not_found) {
      error_at(p, "internal: hoisted var missing");
      return;
    }
    emit_var_write_ref(c, vr);
    op_emit(c->m, op_pop);
  } else if (c->brace_depth == 0 && find_local_entry(c, name.start, name.len)) {
    local* le = find_local_entry(c, name.start, name.len);
    var_ref vr;
    vr.kind = var_local;
    vr.index = le->slot;
    emit_var_write_ref(c, vr);
    op_emit(c->m, op_pop);
  } else {
    uint16_t slot = next_declared_slot(c);
    emit_var_declare(c, slot);
    add_local(c, name, slot, 0, kind == tok_kw_const);
  }
}

static void emit_pattern_default(parser* p, compiler* c) {
  if (!match(p, tok_assign))
    return;
  op_emit(c->m, op_dup);
  uint16_t isundef_idx = cf_methodref(c->cf, "V6Value", "isUndefined", "()Z");
  op_emit2(c->m, op_invokevirtual, isundef_idx);
  size_t skip_jump = op_pos(c->m);
  op_emit2(c->m, op_ifeq, 0);
  op_emit(c->m, op_pop);
  parse_expr(p, c);
  size_t after = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(skip_jump + 1), (uint16_t)(after - skip_jump));
}

void parse_array_pattern(parser* p, compiler* c, tok_kind kind,
                         uint16_t src_slot) {
  uint16_t getprop_idx = cf_methodref(c->cf, "V6Value", "getProp",
                                      "(Ljava/lang/String;)LV6Value;");
  int idx = 0;
  for (;;) {
    if (check(p, tok_rbracket))
      break;
    if (match(p, tok_comma)) {
      idx++;
      continue;
    }
    if (match(p, tok_ellipsis)) {
      if (!expect(p, tok_ident))
        return;
      tok name = p->prev;
      uint16_t restfrom_idx =
          cf_methodref(c->cf, "V6Value", "restFrom", "(I)LV6Array;");
      emit_aload(c->m, src_slot);
      emit_iconst(c->m, idx);
      op_emit2(c->m, op_invokevirtual, restfrom_idx);
      emit_box_object_ref(c);
      bind_pattern_target(p, c, kind, name);
      break;
    }

    if (check(p, tok_lbracket) || check(p, tok_lbrace)) {
      int nested_is_array = check(p, tok_lbracket);
      tok_kind open = nested_is_array ? tok_lbracket : tok_lbrace;
      tok_kind close = nested_is_array ? tok_rbracket : tok_rbrace;
      advance(p);
      const char* nested_pattern_start = p->cur.start;
      skip_balanced(p, open, close);

      char idxbuf[16];
      snprintf(idxbuf, sizeof(idxbuf), "%d", idx);
      uint16_t key_idx = cf_string(c->cf, idxbuf);
      emit_aload(c->m, src_slot);
      op_emit2(c->m, op_ldc_w, key_idx);
      op_emit2(c->m, op_invokevirtual, getprop_idx);

      emit_pattern_default(p, c);

      uint16_t nested_slot = c->next_local_slot++;
      emit_astore(c->m, nested_slot);

      parser pp;
      parser_init(&pp, nested_pattern_start);
      if (nested_is_array)
        parse_array_pattern(&pp, c, kind, nested_slot);
      else
        parse_object_pattern(&pp, c, kind, nested_slot);

      idx++;
      maybe_split_chunk_prescan(c);
      if (!match(p, tok_comma))
        break;
      continue;
    }

    if (!expect(p, tok_ident))
      return;
    tok name = p->prev;

    char idxbuf[16];
    snprintf(idxbuf, sizeof(idxbuf), "%d", idx);
    uint16_t key_idx = cf_string(c->cf, idxbuf);
    emit_aload(c->m, src_slot);
    op_emit2(c->m, op_ldc_w, key_idx);
    op_emit2(c->m, op_invokevirtual, getprop_idx);

    emit_pattern_default(p, c);
    bind_pattern_target(p, c, kind, name);
    idx++;
    maybe_split_chunk_prescan(c);
    if (!match(p, tok_comma))
      break;
  }
}

void parse_object_pattern(parser* p, compiler* c, tok_kind kind,
                          uint16_t src_slot) {
  uint16_t getprop_idx = cf_methodref(c->cf, "V6Value", "getProp",
                                      "(Ljava/lang/String;)LV6Value;");
  for (;;) {
    if (check(p, tok_rbrace))
      break;
    if (!expect(p, tok_ident))
      return;
    tok key_name = p->prev;

    char* keystr = dup_tok(key_name);
    uint16_t key_idx = cf_string(c->cf, keystr);
    free(keystr);

    int has_colon = match(p, tok_colon);

    if (has_colon && (check(p, tok_lbracket) || check(p, tok_lbrace))) {
      int nested_is_array = check(p, tok_lbracket);
      tok_kind open = nested_is_array ? tok_lbracket : tok_lbrace;
      tok_kind close = nested_is_array ? tok_rbracket : tok_rbrace;
      advance(p);
      const char* nested_pattern_start = p->cur.start;
      skip_balanced(p, open, close);

      emit_aload(c->m, src_slot);
      op_emit2(c->m, op_ldc_w, key_idx);
      op_emit2(c->m, op_invokevirtual, getprop_idx);

      emit_pattern_default(p, c);

      uint16_t nested_slot = c->next_local_slot++;
      emit_astore(c->m, nested_slot);

      parser pp;
      parser_init(&pp, nested_pattern_start);
      if (nested_is_array)
        parse_array_pattern(&pp, c, kind, nested_slot);
      else
        parse_object_pattern(&pp, c, kind, nested_slot);

      maybe_split_chunk_prescan(c);
      if (!match(p, tok_comma))
        break;
      continue;
    }

    tok target_name = key_name;
    if (has_colon) {
      if (!expect(p, tok_ident))
        return;
      target_name = p->prev;
    }

    emit_aload(c->m, src_slot);
    op_emit2(c->m, op_ldc_w, key_idx);
    op_emit2(c->m, op_invokevirtual, getprop_idx);

    emit_pattern_default(p, c);
    bind_pattern_target(p, c, kind, target_name);
    maybe_split_chunk_prescan(c);
    if (!match(p, tok_comma))
      break;
  }
}

void parse_one_declarator(parser* p, compiler* c, tok_kind kind) {
  if (check(p, tok_lbracket) || check(p, tok_lbrace)) {
    int is_array = check(p, tok_lbracket);
    tok_kind open = is_array ? tok_lbracket : tok_lbrace;
    tok_kind close = is_array ? tok_rbracket : tok_rbrace;
    advance(p);
    const char* pattern_start = p->cur.start;
    skip_balanced(p, open, close);
    if (!expect(p, tok_assign))
      return;
    parse_expr(p, c);
    uint16_t src_slot = c->next_local_slot++;
    emit_astore(c->m, src_slot);

    parser pp;
    parser_init(&pp, pattern_start);
    if (is_array)
      parse_array_pattern(&pp, c, kind, src_slot);
    else
      parse_object_pattern(&pp, c, kind, src_slot);
    return;
  }

  if (!is_contextual_ident(p->cur.kind)) {
    error_at(p, "expected identifier");
    return;
  }
  advance(p);
  parse_one_declarator_named(p, c, kind, p->prev);
}

void parse_var_decl(parser* p, compiler* c, tok_kind kind) {
  parse_one_declarator(p, c, kind);
  maybe_split_chunk(p, c);
  while (match(p, tok_comma)) {
    parse_one_declarator(p, c, kind);
    maybe_split_chunk(p, c);
  }
}
