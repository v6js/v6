#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct buf {
  uint8_t* data;
  size_t len;
  size_t cap;
} buf;

void buf_init(buf* b);
void buf_free(buf* b);
void buf_u8(buf* b, uint8_t v);
void buf_u16(buf* b, uint16_t v);
void buf_u32(buf* b, uint32_t v);
void buf_bytes(buf* b, const uint8_t* p, size_t n);
