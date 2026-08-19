#include "v6/parser.h"

#include "v6/module.h"
#include "v6/internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "v6/scope.h"

local* find_local_entry(compiler* c, const char* name, size_t len) {
  for (int i = c->local_count - 1; i >= 0; i--) {
    if (c->locals[i].dead)
      continue;
    if (c->locals[i].len == len && memcmp(c->locals[i].name, name, len) == 0)
      return &c->locals[i];
  }
  return NULL;
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
