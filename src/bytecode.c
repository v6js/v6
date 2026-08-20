#include "v6/bytecode.h"

#include "v6/internal.h"
#include "v6/module.h"
#include "v6/parser.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
  cp_utf8 = 1,
  cp_integer = 3,
  cp_float = 4,
  cp_long = 5,
  cp_double = 6,
  cp_class = 7,
  cp_string = 8,
  cp_fieldref = 9,
  cp_methodref = 10,
  cp_name_type = 12,
};

static char* cf_strdup(const char* s) {
  size_t n = strlen(s) + 1;
  char* out = malloc(n);
  memcpy(out, s, n);
  return out;
}

static uint32_t fnv1a(const char* s) {
  uint32_t h = 2166136261u;
  while (*s) {
    h ^= (uint8_t)*s++;
    h *= 16777619u;
  }
  return h;
}

static uint32_t fnv1a_more(uint32_t h, const char* s) {
  while (*s) {
    h ^= (uint8_t)*s++;
    h *= 16777619u;
  }
  return h;
}

static void cp_hash_grow(cp_hash* h) {
  size_t old_cap = h->cap;
  cp_hash_bucket* old = h->buckets;
  h->cap = old_cap ? old_cap * 2 : 128;
  h->buckets = malloc(h->cap * sizeof(cp_hash_bucket));
  for (size_t i = 0; i < h->cap; i++)
    h->buckets[i].entry_idx = -1;
  for (size_t i = 0; i < old_cap; i++) {
    if (old[i].entry_idx < 0)
      continue;
    size_t j = old[i].hash & (h->cap - 1);
    while (h->buckets[j].entry_idx >= 0)
      j = (j + 1) & (h->cap - 1);
    h->buckets[j] = old[i];
  }
  free(old);
}

static void cp_hash_insert(cp_hash* h, uint32_t hash, int32_t entry_idx) {
  if (h->cap == 0 || h->count * 10 >= h->cap * 7)
    cp_hash_grow(h);
  size_t j = hash & (h->cap - 1);
  while (h->buckets[j].entry_idx >= 0)
    j = (j + 1) & (h->cap - 1);
  h->buckets[j].hash = hash;
  h->buckets[j].entry_idx = entry_idx;
  h->count++;
}

static int has_4byte_utf8(const uint8_t* s, size_t n) {
  for (size_t i = 0; i < n; i++)
    if ((s[i] & 0xf8) == 0xf0)
      return 1;
  return 0;
}

static uint8_t* to_modified_utf8(const uint8_t* s, size_t n, size_t* out_len) {
  uint8_t* out = malloc(n * 2 + 1);
  size_t oi = 0;
  size_t i = 0;
  while (i < n) {
    if ((s[i] & 0xf8) == 0xf0 && i + 3 < n) {
      uint32_t cp = ((uint32_t)(s[i] & 0x07) << 18) |
                    ((uint32_t)(s[i + 1] & 0x3f) << 12) |
                    ((uint32_t)(s[i + 2] & 0x3f) << 6) |
                    (uint32_t)(s[i + 3] & 0x3f);
      i += 4;
      uint32_t v = cp - 0x10000;
      uint32_t hi = 0xd800 + (v >> 10);
      uint32_t lo = 0xdc00 + (v & 0x3ff);
      out[oi++] = (uint8_t)(0xe0 | (hi >> 12));
      out[oi++] = (uint8_t)(0x80 | ((hi >> 6) & 0x3f));
      out[oi++] = (uint8_t)(0x80 | (hi & 0x3f));
      out[oi++] = (uint8_t)(0xe0 | (lo >> 12));
      out[oi++] = (uint8_t)(0x80 | ((lo >> 6) & 0x3f));
      out[oi++] = (uint8_t)(0x80 | (lo & 0x3f));
    } else {
      out[oi++] = s[i++];
    }
  }
  out[oi] = 0;
  *out_len = oi;
  return out;
}

uint16_t cf_utf8(class_file* cf, const char* s) {
  uint32_t h = fnv1a(s);
  if (cf->utf8_hash.cap) {
    size_t j = h & (cf->utf8_hash.cap - 1);
    while (cf->utf8_hash.buckets[j].entry_idx >= 0) {
      if (cf->utf8_hash.buckets[j].hash == h) {
        int32_t ei = cf->utf8_hash.buckets[j].entry_idx;
        if (strcmp(cf->utf8_cache[ei].s, s) == 0)
          return cf->utf8_cache[ei].idx;
      }
      j = (j + 1) & (cf->utf8_hash.cap - 1);
    }
  }
  size_t n = strlen(s);
  const uint8_t* bytes = (const uint8_t*)s;
  uint8_t* transcoded = NULL;
  if (has_4byte_utf8(bytes, n)) {
    transcoded = to_modified_utf8(bytes, n, &n);
    bytes = transcoded;
  }
  cf->cp_count++;
  uint16_t idx = cf->cp_count;
  buf_u8(&cf->cp, cp_utf8);
  buf_u16(&cf->cp, (uint16_t)n);
  buf_bytes(&cf->cp, bytes, n);
  free(transcoded);
  if (cf->utf8_cache_len == cf->utf8_cache_cap) {
    cf->utf8_cache_cap = cf->utf8_cache_cap ? cf->utf8_cache_cap * 2 : 64;
    cf->utf8_cache =
        realloc(cf->utf8_cache, cf->utf8_cache_cap * sizeof(cp_utf8_entry));
  }
  cp_hash_insert(&cf->utf8_hash, h, (int32_t)cf->utf8_cache_len);
  cf->utf8_cache[cf->utf8_cache_len].s = cf_strdup(s);
  cf->utf8_cache[cf->utf8_cache_len].idx = idx;
  cf->utf8_cache_len++;
  return idx;
}

uint16_t cf_class(class_file* cf, const char* name) {
  uint32_t h = fnv1a(name);
  if (cf->class_hash.cap) {
    size_t j = h & (cf->class_hash.cap - 1);
    while (cf->class_hash.buckets[j].entry_idx >= 0) {
      if (cf->class_hash.buckets[j].hash == h) {
        int32_t ei = cf->class_hash.buckets[j].entry_idx;
        if (strcmp(cf->class_cache[ei].name, name) == 0)
          return cf->class_cache[ei].idx;
      }
      j = (j + 1) & (cf->class_hash.cap - 1);
    }
  }
  uint16_t name_idx = cf_utf8(cf, name);
  cf->cp_count++;
  uint16_t idx = cf->cp_count;
  buf_u8(&cf->cp, cp_class);
  buf_u16(&cf->cp, name_idx);
  if (cf->class_cache_len == cf->class_cache_cap) {
    cf->class_cache_cap = cf->class_cache_cap ? cf->class_cache_cap * 2 : 64;
    cf->class_cache =
        realloc(cf->class_cache, cf->class_cache_cap * sizeof(cp_class_entry));
  }
  cp_hash_insert(&cf->class_hash, h, (int32_t)cf->class_cache_len);
  cf->class_cache[cf->class_cache_len].name = cf_strdup(name);
  cf->class_cache[cf->class_cache_len].idx = idx;
  cf->class_cache_len++;
  return idx;
}

uint16_t cf_name_type(class_file* cf, const char* name, const char* desc) {
  uint32_t h = fnv1a_more(fnv1a(name), desc);
  if (cf->nt_hash.cap) {
    size_t j = h & (cf->nt_hash.cap - 1);
    while (cf->nt_hash.buckets[j].entry_idx >= 0) {
      if (cf->nt_hash.buckets[j].hash == h) {
        int32_t ei = cf->nt_hash.buckets[j].entry_idx;
        if (strcmp(cf->nt_cache[ei].name, name) == 0 &&
            strcmp(cf->nt_cache[ei].desc, desc) == 0)
          return cf->nt_cache[ei].idx;
      }
      j = (j + 1) & (cf->nt_hash.cap - 1);
    }
  }
  uint16_t name_idx = cf_utf8(cf, name);
  uint16_t desc_idx = cf_utf8(cf, desc);
  cf->cp_count++;
  uint16_t idx = cf->cp_count;
  buf_u8(&cf->cp, cp_name_type);
  buf_u16(&cf->cp, name_idx);
  buf_u16(&cf->cp, desc_idx);
  if (cf->nt_cache_len == cf->nt_cache_cap) {
    cf->nt_cache_cap = cf->nt_cache_cap ? cf->nt_cache_cap * 2 : 64;
    cf->nt_cache =
        realloc(cf->nt_cache, cf->nt_cache_cap * sizeof(cp_nt_entry));
  }
  cp_hash_insert(&cf->nt_hash, h, (int32_t)cf->nt_cache_len);
  cf->nt_cache[cf->nt_cache_len].name = cf_strdup(name);
  cf->nt_cache[cf->nt_cache_len].desc = cf_strdup(desc);
  cf->nt_cache[cf->nt_cache_len].idx = idx;
  cf->nt_cache_len++;
  return idx;
}

static uint16_t cf_ref(class_file* cf, cp_ref_entry** cache, size_t* len,
                       size_t* cap, cp_hash* hash_tbl, uint8_t tag,
                       const char* cls, const char* name, const char* desc) {
  cp_ref_entry* arr = *cache;
  uint32_t h = fnv1a_more(fnv1a_more(fnv1a(cls), name), desc);
  if (hash_tbl->cap) {
    size_t j = h & (hash_tbl->cap - 1);
    while (hash_tbl->buckets[j].entry_idx >= 0) {
      if (hash_tbl->buckets[j].hash == h) {
        int32_t ei = hash_tbl->buckets[j].entry_idx;
        if (strcmp(arr[ei].cls, cls) == 0 && strcmp(arr[ei].name, name) == 0 &&
            strcmp(arr[ei].desc, desc) == 0)
          return arr[ei].idx;
      }
      j = (j + 1) & (hash_tbl->cap - 1);
    }
  }
  uint16_t cls_idx = cf_class(cf, cls);
  uint16_t nt_idx = cf_name_type(cf, name, desc);
  cf->cp_count++;
  uint16_t idx = cf->cp_count;
  buf_u8(&cf->cp, tag);
  buf_u16(&cf->cp, cls_idx);
  buf_u16(&cf->cp, nt_idx);
  if (*len == *cap) {
    *cap = *cap ? *cap * 2 : 64;
    *cache = realloc(*cache, *cap * sizeof(cp_ref_entry));
  }
  arr = *cache;
  cp_hash_insert(hash_tbl, h, (int32_t)*len);
  arr[*len].cls = cf_strdup(cls);
  arr[*len].name = cf_strdup(name);
  arr[*len].desc = cf_strdup(desc);
  arr[*len].idx = idx;
  (*len)++;
  return idx;
}

uint16_t cf_methodref(class_file* cf, const char* cls, const char* name,
                      const char* desc) {
  return cf_ref(cf, &cf->methodref_cache, &cf->methodref_cache_len,
                &cf->methodref_cache_cap, &cf->methodref_hash, cp_methodref,
                cls, name, desc);
}

uint16_t cf_fieldref(class_file* cf, const char* cls, const char* name,
                     const char* desc) {
  return cf_ref(cf, &cf->fieldref_cache, &cf->fieldref_cache_len,
                &cf->fieldref_cache_cap, &cf->fieldref_hash, cp_fieldref, cls,
                name, desc);
}

uint16_t cf_string(class_file* cf, const char* s) {
  uint32_t h = fnv1a(s);
  if (cf->str_hash.cap) {
    size_t j = h & (cf->str_hash.cap - 1);
    while (cf->str_hash.buckets[j].entry_idx >= 0) {
      if (cf->str_hash.buckets[j].hash == h) {
        int32_t ei = cf->str_hash.buckets[j].entry_idx;
        if (strcmp(cf->str_cache[ei].s, s) == 0)
          return cf->str_cache[ei].idx;
      }
      j = (j + 1) & (cf->str_hash.cap - 1);
    }
  }
  uint16_t str_idx = cf_utf8(cf, s);
  cf->cp_count++;
  uint16_t idx = cf->cp_count;
  buf_u8(&cf->cp, cp_string);
  buf_u16(&cf->cp, str_idx);
  if (cf->str_cache_len == cf->str_cache_cap) {
    cf->str_cache_cap = cf->str_cache_cap ? cf->str_cache_cap * 2 : 64;
    cf->str_cache =
        realloc(cf->str_cache, cf->str_cache_cap * sizeof(cp_str_entry));
  }
  cp_hash_insert(&cf->str_hash, h, (int32_t)cf->str_cache_len);
  cf->str_cache[cf->str_cache_len].s = cf_strdup(s);
  cf->str_cache[cf->str_cache_len].idx = idx;
  cf->str_cache_len++;
  return idx;
}

uint16_t cf_integer(class_file* cf, int32_t v) {
  uint32_t h = (uint32_t)v * 2654435761u;
  if (cf->int_hash.cap) {
    size_t j = h & (cf->int_hash.cap - 1);
    while (cf->int_hash.buckets[j].entry_idx >= 0) {
      if (cf->int_hash.buckets[j].hash == h) {
        int32_t ei = cf->int_hash.buckets[j].entry_idx;
        if (cf->int_cache[ei].v == v)
          return cf->int_cache[ei].idx;
      }
      j = (j + 1) & (cf->int_hash.cap - 1);
    }
  }
  cf->cp_count++;
  uint16_t idx = cf->cp_count;
  buf_u8(&cf->cp, cp_integer);
  buf_u32(&cf->cp, (uint32_t)v);
  if (cf->int_cache_len == cf->int_cache_cap) {
    cf->int_cache_cap = cf->int_cache_cap ? cf->int_cache_cap * 2 : 64;
    cf->int_cache =
        realloc(cf->int_cache, cf->int_cache_cap * sizeof(cp_int_entry));
  }
  cp_hash_insert(&cf->int_hash, h, (int32_t)cf->int_cache_len);
  cf->int_cache[cf->int_cache_len].v = v;
  cf->int_cache[cf->int_cache_len].idx = idx;
  cf->int_cache_len++;
  return idx;
}

uint16_t cf_double(class_file* cf, double v) {
  uint64_t key_bits;
  memcpy(&key_bits, &v, sizeof(key_bits));
  uint32_t h = (uint32_t)(key_bits ^ (key_bits >> 32)) * 2654435761u;
  if (cf->dbl_hash.cap) {
    size_t j = h & (cf->dbl_hash.cap - 1);
    while (cf->dbl_hash.buckets[j].entry_idx >= 0) {
      if (cf->dbl_hash.buckets[j].hash == h) {
        int32_t ei = cf->dbl_hash.buckets[j].entry_idx;
        if (cf->dbl_cache[ei].v == v)
          return cf->dbl_cache[ei].idx;
      }
      j = (j + 1) & (cf->dbl_hash.cap - 1);
    }
  }
  cf->cp_count++;
  uint16_t idx = cf->cp_count;
  buf_u8(&cf->cp, cp_double);
  buf_u32(&cf->cp, (uint32_t)(key_bits >> 32));
  buf_u32(&cf->cp, (uint32_t)key_bits);
  cf->cp_count++;
  if (cf->dbl_cache_len == cf->dbl_cache_cap) {
    cf->dbl_cache_cap = cf->dbl_cache_cap ? cf->dbl_cache_cap * 2 : 64;
    cf->dbl_cache =
        realloc(cf->dbl_cache, cf->dbl_cache_cap * sizeof(cp_dbl_entry));
  }
  cp_hash_insert(&cf->dbl_hash, h, (int32_t)cf->dbl_cache_len);
  cf->dbl_cache[cf->dbl_cache_len].v = v;
  cf->dbl_cache[cf->dbl_cache_len].idx = idx;
  cf->dbl_cache_len++;
  return idx;
}

uint16_t cf_long(class_file* cf, int64_t v) {
  cf->cp_count++;
  uint16_t idx = cf->cp_count;
  buf_u8(&cf->cp, cp_long);
  buf_u32(&cf->cp, (uint32_t)((uint64_t)v >> 32));
  buf_u32(&cf->cp, (uint32_t)(uint64_t)v);
  cf->cp_count++;
  return idx;
}

uint16_t cf_float(class_file* cf, float v) {
  uint32_t bits;
  memcpy(&bits, &v, sizeof(bits));
  cf->cp_count++;
  uint16_t idx = cf->cp_count;
  buf_u8(&cf->cp, cp_float);
  buf_u32(&cf->cp, bits);
  return idx;
}

void cf_init(class_file* cf, const char* this_name, const char* super_name) {
  buf_init(&cf->cp);
  cf->cp_count = 0;
  cf->utf8_cache = NULL;
  cf->utf8_cache_len = 0;
  cf->utf8_cache_cap = 0;
  cf->class_cache = NULL;
  cf->class_cache_len = 0;
  cf->class_cache_cap = 0;
  cf->nt_cache = NULL;
  cf->nt_cache_len = 0;
  cf->nt_cache_cap = 0;
  cf->methodref_cache = NULL;
  cf->methodref_cache_len = 0;
  cf->methodref_cache_cap = 0;
  cf->fieldref_cache = NULL;
  cf->fieldref_cache_len = 0;
  cf->fieldref_cache_cap = 0;
  cf->str_cache = NULL;
  cf->str_cache_len = 0;
  cf->str_cache_cap = 0;
  cf->int_cache = NULL;
  cf->int_cache_len = 0;
  cf->int_cache_cap = 0;
  cf->dbl_cache = NULL;
  cf->dbl_cache_len = 0;
  cf->dbl_cache_cap = 0;
  memset(&cf->utf8_hash, 0, sizeof(cf->utf8_hash));
  memset(&cf->class_hash, 0, sizeof(cf->class_hash));
  memset(&cf->nt_hash, 0, sizeof(cf->nt_hash));
  memset(&cf->methodref_hash, 0, sizeof(cf->methodref_hash));
  memset(&cf->fieldref_hash, 0, sizeof(cf->fieldref_hash));
  memset(&cf->str_hash, 0, sizeof(cf->str_hash));
  memset(&cf->int_hash, 0, sizeof(cf->int_hash));
  memset(&cf->dbl_hash, 0, sizeof(cf->dbl_hash));
  cf->access = acc_public | acc_super;
  cf->this_idx = cf_class(cf, this_name);
  cf->super_idx = cf_class(cf, super_name);
  cf->code_utf8 = cf_utf8(cf, "Code");
  cf->methods = NULL;
  cf->method_len = 0;
  cf->method_cap = 0;
  cf->fields = NULL;
  cf->field_len = 0;
  cf->field_cap = 0;
}

void cf_free(class_file* cf) {
  for (size_t i = 0; i < cf->method_len; i++) {
    buf_free(&cf->methods[i]->code);
    free(cf->methods[i]->exceptions);
    free(cf->methods[i]);
  }
  free(cf->methods);
  free(cf->fields);
  buf_free(&cf->cp);
  for (size_t i = 0; i < cf->utf8_cache_len; i++)
    free(cf->utf8_cache[i].s);
  free(cf->utf8_cache);
  for (size_t i = 0; i < cf->class_cache_len; i++)
    free(cf->class_cache[i].name);
  free(cf->class_cache);
  for (size_t i = 0; i < cf->nt_cache_len; i++) {
    free(cf->nt_cache[i].name);
    free(cf->nt_cache[i].desc);
  }
  free(cf->nt_cache);
  for (size_t i = 0; i < cf->methodref_cache_len; i++) {
    free(cf->methodref_cache[i].cls);
    free(cf->methodref_cache[i].name);
    free(cf->methodref_cache[i].desc);
  }
  free(cf->methodref_cache);
  for (size_t i = 0; i < cf->fieldref_cache_len; i++) {
    free(cf->fieldref_cache[i].cls);
    free(cf->fieldref_cache[i].name);
    free(cf->fieldref_cache[i].desc);
  }
  free(cf->fieldref_cache);
  for (size_t i = 0; i < cf->str_cache_len; i++)
    free(cf->str_cache[i].s);
  free(cf->str_cache);
  free(cf->int_cache);
  free(cf->dbl_cache);
  free(cf->utf8_hash.buckets);
  free(cf->class_hash.buckets);
  free(cf->nt_hash.buckets);
  free(cf->methodref_hash.buckets);
  free(cf->fieldref_hash.buckets);
  free(cf->str_hash.buckets);
  free(cf->int_hash.buckets);
  free(cf->dbl_hash.buckets);
}

method* cf_method(class_file* cf, uint16_t access, const char* name,
                  const char* desc) {
  if (cf->method_len == cf->method_cap) {
    cf->method_cap = cf->method_cap ? cf->method_cap * 2 : 4;
    cf->methods = realloc(cf->methods, cf->method_cap * sizeof(method*));
  }
  method* m = malloc(sizeof(method));
  m->access = access;
  m->name_idx = cf_utf8(cf, name);
  m->desc_idx = cf_utf8(cf, desc);
  m->max_stack = 0;
  m->max_locals = 0;
  buf_init(&m->code);
  m->exceptions = NULL;
  m->exception_len = 0;
  m->exception_cap = 0;
  m->owner_cf = cf;
  cf->methods[cf->method_len++] = m;
  return m;
}

void method_add_exception(method* m, uint16_t start_pc, uint16_t end_pc,
                          uint16_t handler_pc, uint16_t catch_type) {
  if (m->exception_len == m->exception_cap) {
    m->exception_cap = m->exception_cap ? m->exception_cap * 2 : 4;
    m->exceptions =
        realloc(m->exceptions, m->exception_cap * sizeof(exc_entry));
  }
  exc_entry* e = &m->exceptions[m->exception_len++];
  e->start_pc = start_pc;
  e->end_pc = end_pc;
  e->handler_pc = handler_pc;
  e->catch_type = catch_type;
}

void cf_field(class_file* cf, uint16_t access, const char* name,
              const char* desc) {
  if (cf->field_len == cf->field_cap) {
    cf->field_cap = cf->field_cap ? cf->field_cap * 2 : 4;
    cf->fields = realloc(cf->fields, cf->field_cap * sizeof(field));
  }
  field* f = &cf->fields[cf->field_len++];
  f->access = access;
  f->name_idx = cf_utf8(cf, name);
  f->desc_idx = cf_utf8(cf, desc);
}

void op_emit(method* m, uint8_t code) {
  buf_u8(&m->code, code);
}

void op_emit1(method* m, uint8_t code, uint8_t a) {
  buf_u8(&m->code, code);
  buf_u8(&m->code, a);
}

void op_emit2(method* m, uint8_t code, uint16_t a) {
  buf_u8(&m->code, code);
  buf_u16(&m->code, a);
}

size_t op_pos(method* m) {
  return m->code.len;
}

void op_patch2(method* m, size_t at, uint16_t v) {
  m->code.data[at] = (uint8_t)(v >> 8);
  m->code.data[at + 1] = (uint8_t)v;
}

void cf_emit(class_file* cf, buf* out) {
  buf_u32(out, 0xcafebabeu);
  buf_u16(out, 0);
  buf_u16(out, 49);
  buf_u16(out, (uint16_t)(cf->cp_count + 1));
  buf_bytes(out, cf->cp.data, cf->cp.len);
  buf_u16(out, cf->access);
  buf_u16(out, cf->this_idx);
  buf_u16(out, cf->super_idx);
  buf_u16(out, 0);
  buf_u16(out, (uint16_t)cf->field_len);
  for (size_t i = 0; i < cf->field_len; i++) {
    field* f = &cf->fields[i];
    buf_u16(out, f->access);
    buf_u16(out, f->name_idx);
    buf_u16(out, f->desc_idx);
    buf_u16(out, 0);
  }
  buf_u16(out, (uint16_t)cf->method_len);
  for (size_t i = 0; i < cf->method_len; i++) {
    method* m = cf->methods[i];
    uint32_t exc_bytes = (uint32_t)m->exception_len * 8;
    uint32_t code_attr_len =
        2 + 2 + 4 + (uint32_t)m->code.len + 2 + exc_bytes + 2;
    buf_u16(out, m->access);
    buf_u16(out, m->name_idx);
    buf_u16(out, m->desc_idx);
    buf_u16(out, 1);
    buf_u16(out, cf->code_utf8);
    buf_u32(out, code_attr_len);
    buf_u16(out, m->max_stack);
    buf_u16(out, m->max_locals);
    buf_u32(out, (uint32_t)m->code.len);
    buf_bytes(out, m->code.data, m->code.len);
    buf_u16(out, (uint16_t)m->exception_len);
    for (size_t j = 0; j < m->exception_len; j++) {
      exc_entry* e = &m->exceptions[j];
      buf_u16(out, e->start_pc);
      buf_u16(out, e->end_pc);
      buf_u16(out, e->handler_pc);
      buf_u16(out, e->catch_type);
    }
    buf_u16(out, 0);
  }
  buf_u16(out, 0);
}

uint16_t value_class(class_file* cf) {
  return cf_class(cf, "V6Value");
}

uint16_t value_ctor(class_file* cf) {
  return cf_methodref(cf, "V6Value", "<init>", "(IDLjava/lang/Object;)V");
}

uint16_t value_num_method(class_file* cf) {
  return cf_methodref(cf, "V6Value", "num", "(D)LV6Value;");
}

void emit_to_int32_raw(class_file* cf, method* m) {
  uint16_t idx = cf_methodref(cf, "V6Value", "toInt32", "(D)I");
  op_emit2(m, op_invokestatic, idx);
}

void emit_dconst_val(class_file* cf, method* m, double v) {
  uint16_t idx = cf_double(cf, v);
  op_emit2(m, op_ldc2_w, idx);
}

static void emit_wide_slot_op(method* m, uint8_t opcode, uint16_t slot) {
  if (slot > 255) {
    op_emit(m, op_wide);
    op_emit2(m, opcode, slot);
  } else {
    op_emit1(m, opcode, (uint8_t)slot);
  }
}

void emit_dstore(method* m, uint16_t slot) {
  switch (slot) {
  case 0:
    op_emit(m, op_dstore_0);
    return;
  case 1:
    op_emit(m, op_dstore_1);
    return;
  case 2:
    op_emit(m, op_dstore_2);
    return;
  case 3:
    op_emit(m, op_dstore_3);
    return;
  default:
    emit_wide_slot_op(m, op_dstore, slot);
    return;
  }
}

void emit_dload(method* m, uint16_t slot) {
  switch (slot) {
  case 0:
    op_emit(m, op_dload_0);
    return;
  case 1:
    op_emit(m, op_dload_1);
    return;
  case 2:
    op_emit(m, op_dload_2);
    return;
  case 3:
    op_emit(m, op_dload_3);
    return;
  default:
    emit_wide_slot_op(m, op_dload, slot);
    return;
  }
}

void emit_istore(method* m, uint16_t slot) {
  switch (slot) {
  case 0:
    op_emit(m, op_istore_0);
    return;
  case 1:
    op_emit(m, op_istore_1);
    return;
  case 2:
    op_emit(m, op_istore_2);
    return;
  case 3:
    op_emit(m, op_istore_3);
    return;
  default:
    emit_wide_slot_op(m, op_istore, slot);
    return;
  }
}

void emit_iload_slot(method* m, uint16_t slot) {
  switch (slot) {
  case 0:
    op_emit(m, op_iload_0);
    return;
  case 1:
    op_emit(m, op_iload_1);
    return;
  case 2:
    op_emit(m, op_iload_2);
    return;
  case 3:
    op_emit(m, op_iload_3);
    return;
  default:
    emit_wide_slot_op(m, op_iload, slot);
    return;
  }
}

void emit_lload(method* m, uint16_t slot) {
  switch (slot) {
  case 0:
    op_emit(m, op_lload_0);
    return;
  case 1:
    op_emit(m, op_lload_1);
    return;
  case 2:
    op_emit(m, op_lload_2);
    return;
  case 3:
    op_emit(m, op_lload_3);
    return;
  default:
    emit_wide_slot_op(m, op_lload, slot);
    return;
  }
}

void emit_lstore(method* m, uint16_t slot) {
  switch (slot) {
  case 0:
    op_emit(m, op_lstore_0);
    return;
  case 1:
    op_emit(m, op_lstore_1);
    return;
  case 2:
    op_emit(m, op_lstore_2);
    return;
  case 3:
    op_emit(m, op_lstore_3);
    return;
  default:
    emit_wide_slot_op(m, op_lstore, slot);
    return;
  }
}

void emit_fload(method* m, uint16_t slot) {
  switch (slot) {
  case 0:
    op_emit(m, op_fload_0);
    return;
  case 1:
    op_emit(m, op_fload_1);
    return;
  case 2:
    op_emit(m, op_fload_2);
    return;
  case 3:
    op_emit(m, op_fload_3);
    return;
  default:
    emit_wide_slot_op(m, op_fload, slot);
    return;
  }
}

void emit_fstore(method* m, uint16_t slot) {
  switch (slot) {
  case 0:
    op_emit(m, op_fstore_0);
    return;
  case 1:
    op_emit(m, op_fstore_1);
    return;
  case 2:
    op_emit(m, op_fstore_2);
    return;
  case 3:
    op_emit(m, op_fstore_3);
    return;
  default:
    emit_wide_slot_op(m, op_fstore, slot);
    return;
  }
}

void emit_aload(method* m, uint16_t slot) {
  switch (slot) {
  case 0:
    op_emit(m, op_aload_0);
    return;
  case 1:
    op_emit(m, op_aload_1);
    return;
  case 2:
    op_emit(m, op_aload_2);
    return;
  case 3:
    op_emit(m, op_aload_3);
    return;
  default:
    emit_wide_slot_op(m, op_aload, slot);
    return;
  }
}

void emit_astore(method* m, uint16_t slot) {
  switch (slot) {
  case 0:
    op_emit(m, op_astore_0);
    return;
  case 1:
    op_emit(m, op_astore_1);
    return;
  case 2:
    op_emit(m, op_astore_2);
    return;
  case 3:
    op_emit(m, op_astore_3);
    return;
  default:
    emit_wide_slot_op(m, op_astore, slot);
    return;
  }
}

void emit_box_const(class_file* cf, method* m, uint8_t tag_op, uint8_t num_op) {
  if (tag_op == op_iconst_0) {
    op_emit(m, num_op);
    op_emit2(m, op_invokestatic, value_num_method(cf));
    return;
  }
  op_emit2(m, op_new, value_class(cf));
  op_emit(m, op_dup);
  op_emit(m, tag_op);
  op_emit(m, num_op);
  op_emit(m, op_aconst_null);
  op_emit2(m, op_invokespecial, value_ctor(cf));
}

void emit_const_singleton(class_file* cf, method* m, const char* field) {
  uint16_t idx = cf_fieldref(cf, "V6Value", field, "LV6Value;");
  op_emit2(m, op_getstatic, idx);
}

void emit_undef(class_file* cf, method* m) {
  emit_const_singleton(cf, m, "UNDEF");
}

static void emit_box_tag_m(class_file* cf, method* m, uint16_t scratch_slot,
                           uint8_t tag_op) {
  if (tag_op == op_iconst_0) {
    op_emit2(m, op_invokestatic, value_num_method(cf));
    return;
  }
  emit_dstore(m, scratch_slot);
  op_emit2(m, op_new, value_class(cf));
  op_emit(m, op_dup);
  op_emit(m, tag_op);
  emit_dload(m, scratch_slot);
  op_emit(m, op_aconst_null);
  op_emit2(m, op_invokespecial, value_ctor(cf));
}

void emit_box_tag(compiler* c, uint8_t tag_op) {
  emit_box_tag_m(c->cf, c->m, c->scratch_slot, tag_op);
}

void emit_box_bool(compiler* c) {
  emit_dstore(c->m, c->scratch_slot);
  emit_dload(c->m, c->scratch_slot);
  op_emit(c->m, op_dconst_0);
  op_emit(c->m, op_dcmpg);
  size_t false_jump = op_pos(c->m);
  op_emit2(c->m, op_ifeq, 0);
  emit_const_singleton(c->cf, c->m, "TRUE");
  size_t end_jump = op_pos(c->m);
  op_emit2(c->m, op_goto, 0);
  size_t false_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(false_jump + 1),
            (uint16_t)(false_pos - false_jump));
  emit_const_singleton(c->cf, c->m, "FALSE");
  size_t end_pos = op_pos(c->m);
  op_patch2(c->m, (uint16_t)(end_jump + 1), (uint16_t)(end_pos - end_jump));
}

void emit_to_number(compiler* c) {
  uint16_t idx = cf_methodref(c->cf, "V6Value", "toNumber", "()D");
  op_emit2(c->m, op_invokevirtual, idx);
}

void emit_truthy(compiler* c) {
  uint16_t idx = cf_methodref(c->cf, "V6Value", "truthy", "()Z");
  op_emit2(c->m, op_invokevirtual, idx);
}

void emit_iconst(method* m, int n) {
  if (n >= -1 && n <= 5) {
    op_emit(m, (uint8_t)(op_iconst_0 + n));
    return;
  }
  if (n >= -128 && n <= 127) {
    op_emit1(m, op_bipush, (uint8_t)n);
    return;
  }
  if (n >= -32768 && n <= 32767) {
    op_emit2(m, op_sipush, (uint16_t)n);
    return;
  }
  op_emit2(m, op_ldc_w, cf_integer(m->owner_cf, n));
}

static uint16_t ref_class(class_file* cf) {
  return cf_class(cf, "V6Ref");
}

static uint16_t ref_ctor(class_file* cf) {
  return cf_methodref(cf, "V6Ref", "<init>", "(LV6Value;)V");
}

static uint16_t ref_field(class_file* cf) {
  return cf_fieldref(cf, "V6Ref", "value", "LV6Value;");
}

void emit_ref_push(compiler* c, int is_upvalue, uint16_t index) {
  if (is_upvalue) {
    emit_aload(c->m, 0);
    emit_iconst(c->m, (int)index);
    op_emit(c->m, op_aaload);
    return;
  }
  if (c->use_frame_locals) {
    emit_aload(c->m, c->frame_slot);
    emit_iconst(c->m, (int)index);
    op_emit(c->m, op_aaload);
    return;
  }
  emit_aload(c->m, index);
}

static void emit_var_read(compiler* c, int is_upvalue, uint16_t index) {
  emit_ref_push(c, is_upvalue, index);
  op_emit2(c->m, op_getfield, ref_field(c->cf));
}

static void emit_var_write(compiler* c, int is_upvalue, uint16_t index) {
  emit_ref_push(c, is_upvalue, index);
  op_emit(c->m, op_swap);
  op_emit(c->m, op_dup_x1);
  op_emit2(c->m, op_putfield, ref_field(c->cf));
}

void emit_var_declare(compiler* c, uint16_t slot) {
  if (!c->box_locals && !c->use_frame_locals) {
    emit_astore(c->m, slot);
    return;
  }
  op_emit2(c->m, op_new, ref_class(c->cf));
  op_emit(c->m, op_dup_x1);
  op_emit(c->m, op_swap);
  op_emit2(c->m, op_invokespecial, ref_ctor(c->cf));
  if (c->use_frame_locals) {
    emit_aload(c->m, c->frame_slot);
    op_emit(c->m, op_swap);
    emit_iconst(c->m, (int)slot);
    op_emit(c->m, op_swap);
    op_emit(c->m, op_aastore);
    return;
  }
  emit_astore(c->m, slot);
}

void emit_var_read_ref(compiler* c, var_ref vr) {
  if (!c->box_locals && !c->use_frame_locals && vr.kind == var_local) {
    emit_aload(c->m, vr.index);
    return;
  }
  emit_var_read(c, vr.kind == var_upvalue, vr.index);
}

void emit_var_write_ref(compiler* c, var_ref vr) {
  if (!c->box_locals && !c->use_frame_locals && vr.kind == var_local) {
    op_emit(c->m, op_dup);
    emit_astore(c->m, vr.index);
    return;
  }
  emit_var_write(c, vr.kind == var_upvalue, vr.index);
}

uint16_t object_class(class_file* cf) {
  return cf_class(cf, "V6Object");
}

void emit_box_ref_computed(compiler* c, int tag_val) {
  emit_astore(c->m, c->scratch_slot);
  op_emit2(c->m, op_new, value_class(c->cf));
  op_emit(c->m, op_dup);
  emit_iconst(c->m, tag_val);
  op_emit(c->m, op_dconst_0);
  emit_aload(c->m, c->scratch_slot);
  op_emit2(c->m, op_invokespecial, value_ctor(c->cf));
}

void emit_box_object_ref(compiler* c) {
  emit_box_ref_computed(c, V6_TAG_OBJ);
}
