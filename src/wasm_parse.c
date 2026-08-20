#include "v6/wasm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct wasm_reader {
  const uint8_t* buf;
  size_t len;
  size_t pos;
  int failed;
  char err_msg[256];
} wasm_reader;

static void rd_fail(wasm_reader* r, const char* msg) {
  if (r->failed)
    return;
  r->failed = 1;
  snprintf(r->err_msg, sizeof(r->err_msg), "%s", msg);
}

static int rd_ok(wasm_reader* r) {
  return !r->failed;
}

static uint8_t rd_u8(wasm_reader* r) {
  if (r->failed || r->pos >= r->len) {
    rd_fail(r, "unexpected end of wasm binary");
    return 0;
  }
  return r->buf[r->pos++];
}

static uint32_t rd_u32_leb(wasm_reader* r) {
  uint32_t result = 0;
  int shift = 0;
  for (;;) {
    if (r->failed)
      return 0;
    uint8_t byte = rd_u8(r);
    result |= (uint32_t)(byte & 0x7F) << shift;
    if ((byte & 0x80) == 0)
      break;
    shift += 7;
    if (shift >= 35) {
      rd_fail(r, "malformed LEB128 u32");
      return 0;
    }
  }
  return result;
}

static int64_t rd_i64_leb(wasm_reader* r) {
  int64_t result = 0;
  int shift = 0;
  uint8_t byte;
  for (;;) {
    if (r->failed)
      return 0;
    byte = rd_u8(r);
    result |= (int64_t)(byte & 0x7F) << shift;
    shift += 7;
    if ((byte & 0x80) == 0)
      break;
    if (shift >= 70) {
      rd_fail(r, "malformed LEB128 i64");
      return 0;
    }
  }
  if (shift < 64 && (byte & 0x40))
    result |= -((int64_t)1 << shift);
  return result;
}

static int32_t rd_i32_leb(wasm_reader* r) {
  return (int32_t)rd_i64_leb(r);
}

static const uint8_t* rd_bytes(wasm_reader* r, uint32_t n) {
  if (r->failed || r->pos + n > r->len) {
    rd_fail(r, "unexpected end reading byte span");
    return NULL;
  }
  const uint8_t* p = r->buf + r->pos;
  r->pos += n;
  return p;
}

static char* rd_name(wasm_reader* r) {
  uint32_t n = rd_u32_leb(r);
  const uint8_t* p = rd_bytes(r, n);
  if (!p)
    return NULL;
  char* s = malloc(n + 1);
  memcpy(s, p, n);
  s[n] = '\0';
  return s;
}

static uint8_t* rd_valtype_vec(wasm_reader* r, uint32_t* out_count) {
  uint32_t n = rd_u32_leb(r);
  *out_count = n;
  if (n == 0)
    return NULL;
  uint8_t* v = malloc(n);
  for (uint32_t i = 0; i < n; i++)
    v[i] = rd_u8(r);
  return v;
}

static wasm_limits rd_limits(wasm_reader* r) {
  wasm_limits lim;
  memset(&lim, 0, sizeof(lim));
  uint8_t flags = rd_u8(r);
  lim.min = rd_u32_leb(r);
  if (flags & 1) {
    lim.has_max = 1;
    lim.max = rd_u32_leb(r);
  }
  return lim;
}

static void skip_expr(wasm_reader* r) {
  int depth = 1;
  while (depth > 0 && rd_ok(r)) {
    uint8_t op = rd_u8(r);
    if (r->failed)
      return;
    if (op == 0x0B) {
      depth--;
      continue;
    }
    if (op == 0x02 || op == 0x03 || op == 0x04) {
      rd_u8(r);
      depth++;
      continue;
    }
    switch (op) {
    case 0x0C:
    case 0x0D:
    case 0x10:
    case 0x20:
    case 0x21:
    case 0x22:
    case 0x23:
    case 0x24:
      rd_u32_leb(r);
      break;
    case 0x11:
      rd_u32_leb(r);
      rd_u32_leb(r);
      break;
    case 0x41:
      rd_i32_leb(r);
      break;
    case 0x42:
      rd_i64_leb(r);
      break;
    case 0x43:
      rd_bytes(r, 4);
      break;
    case 0x44:
      rd_bytes(r, 8);
      break;
    case 0xD0:
      rd_u8(r);
      break;
    case 0xD2:
      rd_u32_leb(r);
      break;
    default:
      if (op >= 0x28 && op <= 0x3E) {
        rd_u32_leb(r);
        rd_u32_leb(r);
      } else if (op == 0x0E) {
        uint32_t n = rd_u32_leb(r);
        for (uint32_t i = 0; i < n; i++)
          rd_u32_leb(r);
        rd_u32_leb(r);
      }
      break;
    }
  }
}

static void parse_type_section(wasm_reader* r, wasm_module* m) {
  uint32_t count = rd_u32_leb(r);
  m->types = calloc(count, sizeof(wasm_functype));
  m->type_count = count;
  for (uint32_t i = 0; i < count && rd_ok(r); i++) {
    uint8_t form = rd_u8(r);
    if (form != 0x60) {
      rd_fail(r, "expected function type form 0x60");
      return;
    }
    m->types[i].params = rd_valtype_vec(r, &m->types[i].param_count);
    m->types[i].results = rd_valtype_vec(r, &m->types[i].result_count);
  }
}

static void parse_import_section(wasm_reader* r, wasm_module* m) {
  uint32_t count = rd_u32_leb(r);
  m->imports = calloc(count, sizeof(wasm_import));
  m->import_count = count;
  for (uint32_t i = 0; i < count && rd_ok(r); i++) {
    wasm_import* im = &m->imports[i];
    im->module_name = rd_name(r);
    im->field_name = rd_name(r);
    im->kind = rd_u8(r);
    switch (im->kind) {
    case wasm_import_func:
      im->type_index = rd_u32_leb(r);
      m->imported_func_count++;
      break;
    case wasm_import_table:
      im->table_reftype = rd_u8(r);
      im->table_limits = rd_limits(r);
      m->imported_table_count++;
      break;
    case wasm_import_mem:
      im->mem_limits = rd_limits(r);
      m->imported_mem_count++;
      break;
    case wasm_import_global:
      im->global_type = rd_u8(r);
      im->global_mutable = rd_u8(r);
      m->imported_global_count++;
      break;
    default:
      rd_fail(r, "unknown import kind");
      return;
    }
  }
}

static void parse_function_section(wasm_reader* r, wasm_module* m) {
  uint32_t count = rd_u32_leb(r);
  m->func_type_indices = calloc(count, sizeof(uint32_t));
  m->func_count = count;
  for (uint32_t i = 0; i < count && rd_ok(r); i++)
    m->func_type_indices[i] = rd_u32_leb(r);
}

static void parse_table_section(wasm_reader* r, wasm_module* m) {
  uint32_t count = rd_u32_leb(r);
  m->tables = calloc(count, sizeof(wasm_limits));
  m->table_reftypes = calloc(count, sizeof(uint8_t));
  m->table_count = count;
  for (uint32_t i = 0; i < count && rd_ok(r); i++) {
    m->table_reftypes[i] = rd_u8(r);
    m->tables[i] = rd_limits(r);
  }
}

static void parse_memory_section(wasm_reader* r, wasm_module* m) {
  uint32_t count = rd_u32_leb(r);
  m->memories = calloc(count, sizeof(wasm_limits));
  m->memory_count = count;
  for (uint32_t i = 0; i < count && rd_ok(r); i++)
    m->memories[i] = rd_limits(r);
}

static void parse_global_section(wasm_reader* r, wasm_module* m) {
  uint32_t count = rd_u32_leb(r);
  m->globals = calloc(count, sizeof(wasm_global_decl));
  m->global_count = count;
  for (uint32_t i = 0; i < count && rd_ok(r); i++) {
    wasm_global_decl* g = &m->globals[i];
    g->val_type = rd_u8(r);
    g->is_mutable = rd_u8(r);
    g->init_start = r->buf + r->pos;
    skip_expr(r);
    g->init_len = (size_t)((r->buf + r->pos) - g->init_start);
  }
}

static void parse_export_section(wasm_reader* r, wasm_module* m) {
  uint32_t count = rd_u32_leb(r);
  m->exports = calloc(count, sizeof(wasm_export));
  m->export_count = count;
  for (uint32_t i = 0; i < count && rd_ok(r); i++) {
    m->exports[i].name = rd_name(r);
    m->exports[i].kind = rd_u8(r);
    m->exports[i].index = rd_u32_leb(r);
  }
}

static void parse_start_section(wasm_reader* r, wasm_module* m) {
  m->has_start = 1;
  m->start_func_index = rd_u32_leb(r);
}

static void parse_element_section(wasm_reader* r, wasm_module* m) {
  uint32_t count = rd_u32_leb(r);
  m->elements = calloc(count, sizeof(wasm_element_seg));
  m->element_count = count;
  for (uint32_t i = 0; i < count && rd_ok(r); i++) {
    wasm_element_seg* e = &m->elements[i];
    uint32_t flags = rd_u32_leb(r);
    if (flags == 0) {
      e->table_index = 0;
      e->offset_start = r->buf + r->pos;
      skip_expr(r);
      e->offset_len = (size_t)((r->buf + r->pos) - e->offset_start);
      uint32_t n = rd_u32_leb(r);
      e->func_indices = calloc(n, sizeof(uint32_t));
      e->func_count = n;
      for (uint32_t j = 0; j < n; j++)
        e->func_indices[j] = rd_u32_leb(r);
    } else if (flags == 1) {
      rd_u8(r);
      uint32_t n = rd_u32_leb(r);
      e->func_indices = calloc(n, sizeof(uint32_t));
      e->func_count = n;
      for (uint32_t j = 0; j < n; j++)
        e->func_indices[j] = rd_u32_leb(r);
    } else if (flags == 2) {
      e->table_index = rd_u32_leb(r);
      e->offset_start = r->buf + r->pos;
      skip_expr(r);
      e->offset_len = (size_t)((r->buf + r->pos) - e->offset_start);
      rd_u8(r);
      uint32_t n = rd_u32_leb(r);
      e->func_indices = calloc(n, sizeof(uint32_t));
      e->func_count = n;
      for (uint32_t j = 0; j < n; j++)
        e->func_indices[j] = rd_u32_leb(r);
    } else {
      rd_fail(r, "unsupported element segment kind");
      return;
    }
  }
}

static void parse_code_section(wasm_reader* r, wasm_module* m) {
  uint32_t count = rd_u32_leb(r);
  m->codes = calloc(count, sizeof(wasm_code_body));
  m->code_count = count;
  for (uint32_t i = 0; i < count && rd_ok(r); i++) {
    uint32_t body_size = rd_u32_leb(r);
    size_t body_start = r->pos;
    wasm_code_body* c = &m->codes[i];

    uint32_t local_decl_count = rd_u32_leb(r);
    uint32_t total_locals = 0;
    uint32_t* decl_counts = malloc(sizeof(uint32_t) * (local_decl_count + 1));
    uint8_t* decl_types = malloc(sizeof(uint8_t) * (local_decl_count + 1));
    for (uint32_t j = 0; j < local_decl_count; j++) {
      decl_counts[j] = rd_u32_leb(r);
      decl_types[j] = rd_u8(r);
      total_locals += decl_counts[j];
    }
    c->local_count = total_locals;
    c->local_types = malloc(total_locals > 0 ? total_locals : 1);
    uint32_t idx = 0;
    for (uint32_t j = 0; j < local_decl_count; j++) {
      for (uint32_t k = 0; k < decl_counts[j]; k++)
        c->local_types[idx++] = decl_types[j];
    }
    free(decl_counts);
    free(decl_types);

    size_t body_end = body_start + body_size;
    c->start = r->buf + r->pos;
    c->len = (r->buf + body_end) - c->start;
    r->pos = body_end - 1;
    rd_u8(r);
    if (r->pos != body_end) {
      rd_fail(r, "code body size mismatch");
      return;
    }
  }
}

static void parse_data_section(wasm_reader* r, wasm_module* m) {
  uint32_t count = rd_u32_leb(r);
  m->datas = calloc(count, sizeof(wasm_data_seg));
  m->data_count = count;
  for (uint32_t i = 0; i < count && rd_ok(r); i++) {
    wasm_data_seg* d = &m->datas[i];
    uint32_t flags = rd_u32_leb(r);
    if (flags == 0) {
      d->mem_index = 0;
      d->offset_start = r->buf + r->pos;
      skip_expr(r);
      d->offset_len = (size_t)((r->buf + r->pos) - d->offset_start);
    } else if (flags == 1) {
      d->offset_start = NULL;
      d->offset_len = 0;
    } else if (flags == 2) {
      d->mem_index = rd_u32_leb(r);
      d->offset_start = r->buf + r->pos;
      skip_expr(r);
      d->offset_len = (size_t)((r->buf + r->pos) - d->offset_start);
    } else {
      rd_fail(r, "unsupported data segment kind");
      return;
    }
    uint32_t n = rd_u32_leb(r);
    d->data = rd_bytes(r, n);
    d->data_len = n;
  }
}

int wasm_parse_module(const uint8_t* buf, size_t len, wasm_module* out) {
  memset(out, 0, sizeof(*out));
  out->buf = buf;
  out->buf_len = len;

  wasm_reader r;
  memset(&r, 0, sizeof(r));
  r.buf = buf;
  r.len = len;

  if (len < 8) {
    snprintf(out->err_msg, sizeof(out->err_msg), "wasm binary too short");
    out->ok = 0;
    return -1;
  }
  if (buf[0] != 0x00 || buf[1] != 0x61 || buf[2] != 0x73 || buf[3] != 0x6D) {
    snprintf(out->err_msg, sizeof(out->err_msg), "bad wasm magic number");
    out->ok = 0;
    return -1;
  }
  uint32_t version = (uint32_t)buf[4] | ((uint32_t)buf[5] << 8) |
                     ((uint32_t)buf[6] << 16) | ((uint32_t)buf[7] << 24);
  if (version != 1) {
    snprintf(out->err_msg, sizeof(out->err_msg), "unsupported wasm version");
    out->ok = 0;
    return -1;
  }
  r.pos = 8;

  while (r.pos < r.len && rd_ok(&r)) {
    uint8_t sec_id = rd_u8(&r);
    if (r.failed)
      break;
    uint32_t sec_size = rd_u32_leb(&r);
    size_t sec_end = r.pos + sec_size;

    switch (sec_id) {
    case wasm_sec_custom:
      r.pos = sec_end;
      break;
    case wasm_sec_type:
      parse_type_section(&r, out);
      break;
    case wasm_sec_import:
      parse_import_section(&r, out);
      break;
    case wasm_sec_function:
      parse_function_section(&r, out);
      break;
    case wasm_sec_table:
      parse_table_section(&r, out);
      break;
    case wasm_sec_memory:
      parse_memory_section(&r, out);
      break;
    case wasm_sec_global:
      parse_global_section(&r, out);
      break;
    case wasm_sec_export:
      parse_export_section(&r, out);
      break;
    case wasm_sec_start:
      parse_start_section(&r, out);
      break;
    case wasm_sec_element:
      parse_element_section(&r, out);
      break;
    case wasm_sec_code:
      parse_code_section(&r, out);
      break;
    case wasm_sec_data:
      parse_data_section(&r, out);
      break;
    case wasm_sec_datacount:
      r.pos = sec_end;
      break;
    default:
      rd_fail(&r, "unknown wasm section id");
      break;
    }

    if (r.failed)
      break;
    if (r.pos != sec_end) {
      r.pos = sec_end;
    }
  }

  if (r.failed) {
    snprintf(out->err_msg, sizeof(out->err_msg), "%s", r.err_msg);
    out->ok = 0;
    return -1;
  }

  out->ok = 1;
  return 0;
}

void wasm_module_free(wasm_module* m) {
  for (uint32_t i = 0; i < m->type_count; i++) {
    free(m->types[i].params);
    free(m->types[i].results);
  }
  free(m->types);

  for (uint32_t i = 0; i < m->import_count; i++) {
    free(m->imports[i].module_name);
    free(m->imports[i].field_name);
  }
  free(m->imports);

  free(m->func_type_indices);
  free(m->tables);
  free(m->table_reftypes);
  free(m->memories);
  free(m->globals);

  for (uint32_t i = 0; i < m->export_count; i++)
    free(m->exports[i].name);
  free(m->exports);

  for (uint32_t i = 0; i < m->element_count; i++)
    free(m->elements[i].func_indices);
  free(m->elements);

  for (uint32_t i = 0; i < m->code_count; i++)
    free(m->codes[i].local_types);
  free(m->codes);

  free(m->datas);
}

uint32_t wasm_func_type_index(const wasm_module* m, uint32_t func_index) {
  if (func_index < m->imported_func_count) {
    uint32_t seen = 0;
    for (uint32_t i = 0; i < m->import_count; i++) {
      if (m->imports[i].kind != wasm_import_func)
        continue;
      if (seen == func_index)
        return m->imports[i].type_index;
      seen++;
    }
    return 0;
  }
  return m->func_type_indices[func_index - m->imported_func_count];
}

const wasm_functype* wasm_func_type(const wasm_module* m, uint32_t func_index) {
  uint32_t ti = wasm_func_type_index(m, func_index);
  if (ti >= m->type_count)
    return NULL;
  return &m->types[ti];
}

int wasm_func_is_import(const wasm_module* m, uint32_t func_index) {
  return func_index < m->imported_func_count;
}
