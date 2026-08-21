#include "v6/bundle_sha1.h"

#include <string.h>

static uint32_t rol32(uint32_t v, int bits) {
  return (v << bits) | (v >> (32 - bits));
}

static void sha1_process_block(uint32_t state[5], const unsigned char block[64]) {
  uint32_t w[80];
  for (int i = 0; i < 16; i++) {
    w[i] = ((uint32_t)block[i * 4] << 24) | ((uint32_t)block[i * 4 + 1] << 16) |
           ((uint32_t)block[i * 4 + 2] << 8) | (uint32_t)block[i * 4 + 3];
  }
  for (int i = 16; i < 80; i++) {
    w[i] = rol32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
  }

  uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];

  for (int i = 0; i < 80; i++) {
    uint32_t f, k;
    if (i < 20) {
      f = (b & c) | ((~b) & d);
      k = 0x5A827999u;
    } else if (i < 40) {
      f = b ^ c ^ d;
      k = 0x6ED9EBA1u;
    } else if (i < 60) {
      f = (b & c) | (b & d) | (c & d);
      k = 0x8F1BBCDCu;
    } else {
      f = b ^ c ^ d;
      k = 0xCA62C1D6u;
    }
    uint32_t temp = rol32(a, 5) + f + e + k + w[i];
    e = d;
    d = c;
    c = rol32(b, 30);
    b = a;
    a = temp;
  }

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
}

void bundle_sha1_init(bundle_sha1_ctx* ctx) {
  ctx->state[0] = 0x67452301u;
  ctx->state[1] = 0xEFCDAB89u;
  ctx->state[2] = 0x98BADCFEu;
  ctx->state[3] = 0x10325476u;
  ctx->state[4] = 0xC3D2E1F0u;
  ctx->count = 0;
  ctx->buffer_len = 0;
}

void bundle_sha1_update(bundle_sha1_ctx* ctx, const unsigned char* data, size_t len) {
  ctx->count += len;
  while (len > 0) {
    size_t take = 64 - ctx->buffer_len;
    if (take > len)
      take = len;
    memcpy(ctx->buffer + ctx->buffer_len, data, take);
    ctx->buffer_len += take;
    data += take;
    len -= take;
    if (ctx->buffer_len == 64) {
      sha1_process_block(ctx->state, ctx->buffer);
      ctx->buffer_len = 0;
    }
  }
}

void bundle_sha1_final(bundle_sha1_ctx* ctx, unsigned char digest[20]) {
  uint64_t bit_count = ctx->count * 8;

  unsigned char pad = 0x80;
  bundle_sha1_update(ctx, &pad, 1);

  unsigned char zero = 0;
  while (ctx->buffer_len != 56) {
    bundle_sha1_update(ctx, &zero, 1);
  }

  unsigned char len_bytes[8];
  for (int i = 0; i < 8; i++) {
    len_bytes[i] = (unsigned char)(bit_count >> (56 - i * 8));
  }
  size_t saved_len = ctx->buffer_len;
  memcpy(ctx->buffer + saved_len, len_bytes, 8);
  sha1_process_block(ctx->state, ctx->buffer);

  for (int i = 0; i < 5; i++) {
    digest[i * 4] = (unsigned char)(ctx->state[i] >> 24);
    digest[i * 4 + 1] = (unsigned char)(ctx->state[i] >> 16);
    digest[i * 4 + 2] = (unsigned char)(ctx->state[i] >> 8);
    digest[i * 4 + 3] = (unsigned char)(ctx->state[i]);
  }
}
