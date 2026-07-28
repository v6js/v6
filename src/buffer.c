#include "v6/buffer.h"

#include <stdlib.h>
#include <string.h>

static void buf_grow(buf* b, size_t need) {
  if (b->len + need <= b->cap)
    return;
  size_t cap = b->cap ? b->cap * 2 : 64;
  while (cap < b->len + need)
    cap *= 2;
  b->data = realloc(b->data, cap);
  b->cap = cap;
}

void buf_init(buf* b) {
  b->data = NULL;
  b->len = 0;
  b->cap = 0;
}

void buf_free(buf* b) {
  free(b->data);
  b->data = NULL;
  b->len = 0;
  b->cap = 0;
}

void buf_u8(buf* b, uint8_t v) {
  buf_grow(b, 1);
  b->data[b->len++] = v;
}

void buf_u16(buf* b, uint16_t v) {
  buf_u8(b, (uint8_t)(v >> 8));
  buf_u8(b, (uint8_t)v);
}

void buf_u32(buf* b, uint32_t v) {
  buf_u8(b, (uint8_t)(v >> 24));
  buf_u8(b, (uint8_t)(v >> 16));
  buf_u8(b, (uint8_t)(v >> 8));
  buf_u8(b, (uint8_t)v);
}

void buf_bytes(buf* b, const uint8_t* p, size_t n) {
  buf_grow(b, n);
  memcpy(b->data + b->len, p, n);
  b->len += n;
}
