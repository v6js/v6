#include "v6/parser.h"

#include "v6/module.h"
#include "v6/internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v6/scope.h"
#include "v6/closures.h"

local* find_local_entry(compiler* c, const char* name, size_t len) {
  for (int i = c->local_count - 1; i >= 0; i--) {
    if (c->locals[i].dead)
      continue;
    if (c->locals[i].len == len && memcmp(c->locals[i].name, name, len) == 0)
      return &c->locals[i];
  }
  return NULL;
}

const char* find_direct_fn(compiler* c, const char* name, size_t len) {
  while (c) {
    for (int i = 0; i < c->param_count; i++) {
      if (c->params[i].len == len && memcmp(c->params[i].name, name, len) == 0)
        return NULL;
    }
    local* le = find_local_entry(c, name, len);
    if (le)
      return le->direct_fn ? le->fn_method_name : NULL;
    c = c->parent;
  }
  return NULL;
}

int name_reassigned_in_scope(const char* src, const char* name,
                             size_t name_len) {
  lexer lx;
  lex_init(&lx, src);
  lx.auto_regex = 1;
  int depth = 0;
  tok t = lex_next(&lx);
  while (t.kind != tok_eof) {
    if (t.kind == tok_lbrace) {
      depth++;
    } else if (t.kind == tok_rbrace) {
      if (depth == 0)
        break;
      depth--;
    } else if (t.kind == tok_ident && t.len == name_len &&
               memcmp(t.start, name, name_len) == 0) {
      tok next = lex_next(&lx);
      if (next.kind == tok_assign || next.kind == tok_plus_eq ||
          next.kind == tok_minus_eq || next.kind == tok_star_eq ||
          next.kind == tok_slash_eq || next.kind == tok_percent_eq ||
          next.kind == tok_amp_eq || next.kind == tok_pipe_eq ||
          next.kind == tok_caret_eq || next.kind == tok_shl_eq ||
          next.kind == tok_shr_eq || next.kind == tok_ushr_eq ||
          next.kind == tok_plus_plus || next.kind == tok_minus_minus) {
        return 1;
      }
      t = next;
      continue;
    } else if (t.kind == tok_plus_plus || t.kind == tok_minus_minus) {
      tok next = lex_next(&lx);
      if (next.kind == tok_ident && next.len == name_len &&
          memcmp(next.start, name, name_len) == 0)
        return 1;
      t = next;
      continue;
    }
    t = lex_next(&lx);
  }
  return 0;
}

int find_slot(compiler* c, const char* name, size_t len, uint16_t* out) {
  for (int i = 0; i < c->param_count; i++) {
    if (c->params[i].len == len && memcmp(c->params[i].name, name, len) == 0) {
      *out = c->params[i].slot;
      return 1;
    }
  }
  local* le = find_local_entry(c, name, len);
  if (le) {
    *out = le->slot;
    return 1;
  }
  return 0;
}

uint16_t next_declared_slot(compiler* c) {
  if (c->use_frame_locals)
    return c->next_frame_slot++;
  return c->next_local_slot++;
}

void add_local(compiler* c, tok name, uint16_t slot, int is_var, int is_const) {
  if (c->local_count >= v6_max_locals)
    return;
  if (c->local_count >= c->local_cap) {
    c->local_cap *= 2;
    c->locals = realloc(c->locals, sizeof(local) * c->local_cap);
  }
  c->locals[c->local_count].name = name.start;
  c->locals[c->local_count].len = name.len;
  c->locals[c->local_count].slot = slot;
  c->locals[c->local_count].is_var = is_var;
  c->locals[c->local_count].is_const = is_const;
  c->locals[c->local_count].dead = 0;
  c->locals[c->local_count].direct_fn = 0;
  c->locals[c->local_count].fn_method_name = NULL;
  c->locals[c->local_count].num_shadow_name = NULL;
  c->locals[c->local_count].num_arity = 0;
  c->local_count++;
}

var_ref resolve_var(compiler* c, const char* name, size_t len) {
  uint16_t slot;
  if (find_slot(c, name, len, &slot)) {
    var_ref vr;
    vr.kind = var_local;
    vr.index = slot;
    return vr;
  }
  if (!c->parent) {
    var_ref vr;
    vr.kind = var_not_found;
    vr.index = 0;
    return vr;
  }
  for (int i = 0; i < c->upvalue_count; i++) {
    if (c->upvalues[i].len == len &&
        memcmp(c->upvalues[i].name, name, len) == 0) {
      var_ref vr;
      vr.kind = var_upvalue;
      vr.index = (uint16_t)i;
      return vr;
    }
  }
  var_ref pref = resolve_var(c->parent, name, len);
  if (pref.kind == var_not_found)
    return pref;
  if (c->upvalue_count >= v6_max_upvalues) {
    var_ref vr;
    vr.kind = var_not_found;
    vr.index = 0;
    return vr;
  }
  if (c->upvalue_count >= c->upvalue_cap) {
    c->upvalue_cap *= 2;
    c->upvalues = realloc(c->upvalues, sizeof(upvalue) * c->upvalue_cap);
  }
  upvalue* uv = &c->upvalues[c->upvalue_count];
  uv->name = name;
  uv->len = len;
  uv->from_parent_local = pref.kind == var_local;
  uv->parent_index = pref.index;
  var_ref vr;
  vr.kind = var_upvalue;
  vr.index = (uint16_t)c->upvalue_count;
  c->upvalue_count++;
  return vr;
}

static void prescan_hoist_one(compiler* c, tok name) {
  uint16_t slot = next_declared_slot(c);
  emit_undef(c->cf, c->m);
  emit_var_declare(c, slot);
  add_local(c, name, slot, 1, 0);
  maybe_split_chunk_prescan(c);
}

static int prescan_tok_ends_value(tok_kind k) {
  switch (k) {
  case tok_ident:
  case tok_num:
  case tok_str:
  case tok_rparen:
  case tok_rbracket:
  case tok_kw_this:
  case tok_kw_true:
  case tok_kw_false:
  case tok_kw_null:
  case tok_kw_undefined:
  case tok_kw_super:
  case tok_regex:
  case tok_plus_plus:
  case tok_minus_minus:
  case tok_template:
    return 1;
  default:
    return 0;
  }
}

static tok prescan_skip_initializer(lexer* lx, tok t) {
  int edepth = 0;
  int prev_line = t.line;
  tok_kind prev_kind = tok_eof;
  while (t.kind != tok_eof) {
    if ((t.kind == tok_slash || t.kind == tok_slash_eq) &&
        !prescan_tok_ends_value(prev_kind)) {
      lexer regex_lex = *lx;
      regex_lex.cur = t.start;
      regex_lex.line = t.line;
      t = lex_regex_literal(&regex_lex);
      *lx = regex_lex;
    }
    if (t.kind == tok_lparen || t.kind == tok_lbracket ||
        t.kind == tok_lbrace) {
      edepth++;
    } else if (t.kind == tok_rparen || t.kind == tok_rbracket ||
               t.kind == tok_rbrace) {
      if (edepth == 0)
        break;
      edepth--;
    } else if (edepth == 0 && (t.kind == tok_comma || t.kind == tok_semi)) {
      break;
    } else if (edepth == 0 && t.line > prev_line &&
               prescan_tok_ends_value(prev_kind) && t.kind != tok_question &&
               t.kind != tok_colon) {
      break;
    }
    prev_kind = t.kind;
    prev_line = t.line;
    t = lex_next(lx);
  }
  return t;
}

static tok prescan_pattern_hoist(compiler* c, lexer* lx, int is_array) {
  tok_kind close = is_array ? tok_rbracket : tok_rbrace;
  tok t = lex_next(lx);
  int want_key = !is_array;

  while (t.kind != close && t.kind != tok_eof) {
    if (t.kind == tok_comma) {
      want_key = !is_array;
      t = lex_next(lx);
      continue;
    }
    if (t.kind == tok_ellipsis) {
      t = lex_next(lx);
      continue;
    }
    if (t.kind == tok_ident) {
      if (is_array) {
        prescan_hoist_one(c, t);
        t = lex_next(lx);
      } else if (want_key) {
        tok key = t;
        t = lex_next(lx);
        if (t.kind == tok_colon) {
          t = lex_next(lx);
          if (t.kind == tok_ident) {
            prescan_hoist_one(c, t);
            t = lex_next(lx);
          }
        } else {
          prescan_hoist_one(c, key);
        }
        want_key = 0;
      } else {
        t = lex_next(lx);
      }
      continue;
    }
    if (t.kind == tok_assign) {
      int edepth = 0;
      t = lex_next(lx);
      while (t.kind != tok_eof) {
        if (t.kind == tok_lparen || t.kind == tok_lbracket ||
            t.kind == tok_lbrace) {
          edepth++;
        } else if (t.kind == tok_rparen || t.kind == tok_rbracket ||
                   t.kind == tok_rbrace) {
          if (edepth == 0)
            break;
          edepth--;
        } else if (edepth == 0 && t.kind == tok_comma) {
          break;
        }
        t = lex_next(lx);
      }
      continue;
    }
    t = lex_next(lx);
  }
  if (t.kind == close)
    t = lex_next(lx);
  return t;
}

#define v6_max_pending_fns 1024

void prescan_decls(parser* p, compiler* c, const char* src,
                   int hoist_functions) {
  const char* pending_fns[v6_max_pending_fns];
  int pending_fns_async[v6_max_pending_fns];
  int pending_count = 0;

  lexer lx;
  lex_init(&lx, src);
  lx.auto_regex = 1;
  int depth = 0;
  tok t = lex_next(&lx);
  while (t.kind != tok_eof) {
    if (t.kind == tok_lbrace) {
      depth++;
    } else if (t.kind == tok_rbrace) {
      if (depth == 0)
        break;
      depth--;
    } else if (t.kind == tok_kw_var) {
      for (;;) {
        tok name = lex_next(&lx);
        if (name.kind == tok_lbracket || name.kind == tok_lbrace) {
          t = prescan_pattern_hoist(c, &lx, name.kind == tok_lbracket);
          if (t.kind != tok_assign)
            break;
          t = prescan_skip_initializer(&lx, lex_next(&lx));
          if (t.kind != tok_comma)
            break;
          continue;
        }
        if (!is_contextual_ident(name.kind)) {
          t = name;
          break;
        }
        uint16_t slot = next_declared_slot(c);
        emit_undef(c->cf, c->m);
        emit_var_declare(c, slot);
        add_local(c, name, slot, 1, 0);
        maybe_split_chunk_prescan(c);

        t = lex_next(&lx);
        if (t.kind == tok_assign) {
          t = prescan_skip_initializer(&lx, lex_next(&lx));
        }
        if (t.kind != tok_comma)
          break;
      }
      continue;
    } else if (hoist_functions && depth == 0 &&
               (t.kind == tok_kw_let || t.kind == tok_kw_const)) {
      int is_const_decl = t.kind == tok_kw_const;
      for (;;) {
        tok name = lex_next(&lx);
        if (name.kind == tok_lbracket || name.kind == tok_lbrace) {
          t = prescan_pattern_hoist(c, &lx, name.kind == tok_lbracket);
          if (t.kind != tok_assign)
            break;
          t = prescan_skip_initializer(&lx, lex_next(&lx));
          if (t.kind != tok_comma)
            break;
          continue;
        }
        if (!is_contextual_ident(name.kind)) {
          t = name;
          break;
        }
        uint16_t slot = next_declared_slot(c);
        emit_undef(c->cf, c->m);
        emit_var_declare(c, slot);
        add_local(c, name, slot, 0, is_const_decl);
        maybe_split_chunk_prescan(c);

        t = lex_next(&lx);
        if (t.kind == tok_assign) {
          t = prescan_skip_initializer(&lx, lex_next(&lx));
        }
        if (t.kind != tok_comma)
          break;
      }
      continue;
    } else if (hoist_functions && depth == 0 && t.kind == tok_kw_import) {
      t = lex_next(&lx);
      if (t.kind == tok_str) {
        t = lex_next(&lx);
      } else if (t.kind == tok_ident) {
        prescan_hoist_one(c, t);
        t = lex_next(&lx);
        if (t.kind == tok_comma) {
          t = lex_next(&lx);
          if (t.kind == tok_star) {
            t = lex_next(&lx);
            if (t.kind == tok_kw_as) {
              t = lex_next(&lx);
              if (t.kind == tok_ident) {
                prescan_hoist_one(c, t);
                t = lex_next(&lx);
              }
            }
          } else if (t.kind == tok_lbrace) {
            t = lex_next(&lx);
            while (t.kind != tok_rbrace && t.kind != tok_eof) {
              if (t.kind == tok_ident) {
                tok local = t;
                t = lex_next(&lx);
                if (t.kind == tok_kw_as) {
                  t = lex_next(&lx);
                  if (t.kind == tok_ident) {
                    local = t;
                    t = lex_next(&lx);
                  }
                }
                prescan_hoist_one(c, local);
              } else {
                t = lex_next(&lx);
              }
              if (t.kind == tok_comma)
                t = lex_next(&lx);
            }
            if (t.kind == tok_rbrace)
              t = lex_next(&lx);
          }
        }
      } else if (t.kind == tok_star) {
        t = lex_next(&lx);
        if (t.kind == tok_kw_as) {
          t = lex_next(&lx);
          if (t.kind == tok_ident) {
            prescan_hoist_one(c, t);
            t = lex_next(&lx);
          }
        }
      } else if (t.kind == tok_lbrace) {
        t = lex_next(&lx);
        while (t.kind != tok_rbrace && t.kind != tok_eof) {
          if (t.kind == tok_ident) {
            tok local = t;
            t = lex_next(&lx);
            if (t.kind == tok_kw_as) {
              t = lex_next(&lx);
              if (t.kind == tok_ident) {
                local = t;
                t = lex_next(&lx);
              }
            }
            prescan_hoist_one(c, local);
          } else {
            t = lex_next(&lx);
          }
          if (t.kind == tok_comma)
            t = lex_next(&lx);
        }
        if (t.kind == tok_rbrace)
          t = lex_next(&lx);
      }
      while (t.kind != tok_semi && t.kind != tok_eof)
        t = lex_next(&lx);
      if (t.kind == tok_semi)
        t = lex_next(&lx);
      continue;
    } else if (hoist_functions && depth == 0 && t.kind == tok_kw_function) {
      const char* body_start = lx.cur;
      tok name = lex_next(&lx);
      if (name.kind == tok_star)
        name = lex_next(&lx);
      if (is_contextual_ident(name.kind)) {
        prescan_hoist_one(c, name);
        if (pending_count < v6_max_pending_fns) {
          pending_fns[pending_count] = body_start;
          pending_fns_async[pending_count] = 0;
          pending_count++;
        }
      }

      t = lex_next(&lx);
      if (t.kind == tok_lparen) {
        int pdepth = 1;
        t = lex_next(&lx);
        while (t.kind != tok_eof && pdepth > 0) {
          if (t.kind == tok_lparen)
            pdepth++;
          else if (t.kind == tok_rparen)
            pdepth--;
          t = lex_next(&lx);
        }
      }
      if (t.kind == tok_lbrace) {
        int bdepth = 1;
        t = lex_next(&lx);
        while (t.kind != tok_eof && bdepth > 0) {
          if (t.kind == tok_lbrace)
            bdepth++;
          else if (t.kind == tok_rbrace)
            bdepth--;
          t = lex_next(&lx);
        }
      }
      continue;
    } else if (hoist_functions && depth == 0 && t.kind == tok_kw_async) {
      lexer peek_lx = lx;
      tok next = lex_next(&peek_lx);
      if (next.kind != tok_kw_function) {
        t = lex_next(&lx);
        continue;
      }
      lx = peek_lx;
      const char* body_start = lx.cur;
      tok name = lex_next(&lx);
      if (name.kind == tok_star)
        name = lex_next(&lx);
      if (is_contextual_ident(name.kind)) {
        prescan_hoist_one(c, name);
        if (pending_count < v6_max_pending_fns) {
          pending_fns[pending_count] = body_start;
          pending_fns_async[pending_count] = 1;
          pending_count++;
        }
      }

      t = lex_next(&lx);
      if (t.kind == tok_lparen) {
        int pdepth = 1;
        t = lex_next(&lx);
        while (t.kind != tok_eof && pdepth > 0) {
          if (t.kind == tok_lparen)
            pdepth++;
          else if (t.kind == tok_rparen)
            pdepth--;
          t = lex_next(&lx);
        }
      }
      if (t.kind == tok_lbrace) {
        int bdepth = 1;
        t = lex_next(&lx);
        while (t.kind != tok_eof && bdepth > 0) {
          if (t.kind == tok_lbrace)
            bdepth++;
          else if (t.kind == tok_rbrace)
            bdepth--;
          t = lex_next(&lx);
        }
      }
      continue;
    } else if (hoist_functions && depth == 0 && t.kind == tok_kw_class) {
      tok name = lex_next(&lx);
      if (name.kind == tok_ident)
        prescan_hoist_one(c, name);

      t = lex_next(&lx);
      if (t.kind == tok_kw_extends) {
        lex_next(&lx);
        t = lex_next(&lx);
      }
      if (t.kind == tok_lbrace) {
        int cdepth = 1;
        t = lex_next(&lx);
        while (t.kind != tok_eof && cdepth > 0) {
          if (t.kind == tok_lbrace)
            cdepth++;
          else if (t.kind == tok_rbrace)
            cdepth--;
          t = lex_next(&lx);
        }
      }
      continue;
    }
    t = lex_next(&lx);
  }

  for (int i = 0; i < pending_count; i++) {
    parser fp;
    parser_init(&fp, pending_fns[i]);
    int is_async = pending_fns_async[i];
    int is_gen = match(&fp, tok_star);
    if (!is_contextual_ident(fp.cur.kind))
      continue;
    tok name = fp.cur;
    advance(&fp);
    char* lambda_name = malloc(24);
    local* direct_le = NULL;
    if (!is_gen && !is_async &&
        !name_reassigned_in_scope(src, name.start, name.len)) {
      local* le = find_local_entry(c, name.start, name.len);
      if (le) {
        le->direct_fn = 1;
        le->fn_method_name = lambda_name;
        direct_le = le;
      }
    }
    c->pending_async_gen = is_gen && is_async;
    compile_closure_value(&fp, c, 0, 1, lambda_name);
    if (fp.had_error) {
      p->had_error = 1;
      p->err_line = fp.err_line;
      snprintf(p->err_msg, sizeof(p->err_msg), "%s", fp.err_msg);
      return;
    }
    if (direct_le) {
      char* shadow_name = malloc(24);
      int shadow_arity = 0;
      if (try_compile_num_shadow(c, name, name.start + name.len, shadow_name,
                                 &shadow_arity)) {
        direct_le->num_shadow_name = shadow_name;
        direct_le->num_arity = shadow_arity;
      } else {
        free(shadow_name);
      }
    }
    if (is_gen && is_async)
      emit_wrap_async_generator(c);
    else if (is_gen)
      emit_wrap_generator(c);
    else if (is_async)
      emit_wrap_async(c);
    var_ref vr = resolve_var(c, name.start, name.len);
    if (vr.kind != var_not_found) {
      emit_var_write_ref(c, vr);
      op_emit(c->m, op_pop);
    }
    maybe_split_chunk_prescan(c);
  }
}
