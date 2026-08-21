#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
  uint32_t state[5];
  uint64_t count;
  unsigned char buffer[64];
  size_t buffer_len;
} bundle_sha1_ctx;

void bundle_sha1_init(bundle_sha1_ctx* ctx);
void bundle_sha1_update(bundle_sha1_ctx* ctx, const unsigned char* data,
                        size_t len);
void bundle_sha1_final(bundle_sha1_ctx* ctx, unsigned char digest[20]);
