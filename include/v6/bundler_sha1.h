#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
  uint32_t state[5];
  uint64_t count;
  unsigned char buffer[64];
  size_t buffer_len;
} v6_bundler_sha1_ctx;

void v6_bundler_sha1_init(v6_bundler_sha1_ctx* ctx);
void v6_bundler_sha1_update(v6_bundler_sha1_ctx* ctx, const unsigned char* data,
                            size_t len);
void v6_bundler_sha1_final(v6_bundler_sha1_ctx* ctx, unsigned char digest[20]);
