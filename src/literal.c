#include "v6/parser.h"

#include "v6/module.h"
#include "v6/internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v6/literal.h"

static int hex_val(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

static size_t encode_utf8(uint32_t cp, char* out) {
  if (cp == 0) {
    out[0] = (char)0xc0;
    out[1] = (char)0x80;
    return 2;
  }
  if (cp <= 0x7f) {
    out[0] = (char)cp;
    return 1;
  }
  if (cp <= 0x7ff) {
    out[0] = (char)(0xc0 | (cp >> 6));
    out[1] = (char)(0x80 | (cp & 0x3f));
    return 2;
  }
  out[0] = (char)(0xe0 | (cp >> 12));
  out[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
  out[2] = (char)(0x80 | (cp & 0x3f));
  return 3;
}

char* decode_string(tok t) {
  char* buf = malloc(t.len > 0 ? t.len : 1);
  size_t j = 0;
  for (size_t i = 1; i + 1 < t.len; i++) {
    char ch = t.start[i];
    if (ch == '\\' && i + 2 < t.len) {
      i++;
      char e = t.start[i];
      if (e == 'n') {
        buf[j++] = '\n';
      } else if (e == 't') {
        buf[j++] = '\t';
      } else if (e == 'r') {
        buf[j++] = '\r';
      } else if (e == '0') {
        j += encode_utf8(0, buf + j);
      } else if (e == 'u' && i + 4 < t.len) {
        int h0 = hex_val(t.start[i + 1]);
        int h1 = hex_val(t.start[i + 2]);
        int h2 = hex_val(t.start[i + 3]);
        int h3 = hex_val(t.start[i + 4]);
        if (h0 >= 0 && h1 >= 0 && h2 >= 0 && h3 >= 0) {
          uint32_t cp = (uint32_t)((h0 << 12) | (h1 << 8) | (h2 << 4) | h3);
          j += encode_utf8(cp, buf + j);
          i += 4;
        } else {
          buf[j++] = e;
        }
      } else if (e == 'x' && i + 2 < t.len) {
        int h0 = hex_val(t.start[i + 1]);
        int h1 = hex_val(t.start[i + 2]);
        if (h0 >= 0 && h1 >= 0) {
          uint32_t cp = (uint32_t)((h0 << 4) | h1);
          j += encode_utf8(cp, buf + j);
          i += 2;
        } else {
          buf[j++] = e;
        }
      } else {
        buf[j++] = e;
      }
    } else {
      buf[j++] = ch;
    }
  }
  buf[j] = '\0';
  return buf;
}

char* dup_tok(tok t) {
  char* s = malloc(t.len + 1);
  memcpy(s, t.start, t.len);
  s[t.len] = '\0';
  return s;
}

void emit_string_value(compiler* c, const char* s) {
  uint16_t str_idx = cf_string(c->cf, s);
  op_emit2(c->m, op_new, value_class(c->cf));
  op_emit(c->m, op_dup);
  op_emit(c->m, op_iconst_5);
  op_emit(c->m, op_dconst_0);
  op_emit2(c->m, op_ldc_w, str_idx);
  op_emit2(c->m, op_invokespecial, value_ctor(c->cf));
}

void emit_throw_reference_error(compiler* c, const char* name, size_t len) {
  char msg[256];
  snprintf(msg, sizeof(msg), "ReferenceError: %.*s is not defined", (int)len,
           name);
  emit_string_value(c, msg);
  uint16_t throw_cls = cf_class(c->cf, "V6Throw");
  uint16_t throw_ctor =
      cf_methodref(c->cf, "V6Throw", "<init>", "(LV6Value;)V");
  op_emit2(c->m, op_new, throw_cls);
  op_emit(c->m, op_dup_x1);
  op_emit(c->m, op_swap);
  op_emit2(c->m, op_invokespecial, throw_ctor);
  op_emit(c->m, op_athrow);
}
