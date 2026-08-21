#include "v6/bundler_ws.h"
#include "v6/bundler_sha1.h"

#include <stdio.h>
#include <string.h>

static const char v6_ws_guid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
static const char b64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void base64_encode(const unsigned char* data, size_t len, char* out,
                          size_t out_size) {
  size_t j = 0;
  size_t i = 0;
  while (i + 3 <= len && j + 4 < out_size) {
    unsigned int n = ((unsigned int)data[i] << 16) |
                     ((unsigned int)data[i + 1] << 8) | data[i + 2];
    out[j++] = b64_chars[(n >> 18) & 0x3F];
    out[j++] = b64_chars[(n >> 12) & 0x3F];
    out[j++] = b64_chars[(n >> 6) & 0x3F];
    out[j++] = b64_chars[n & 0x3F];
    i += 3;
  }
  size_t rem = len - i;
  if (rem == 1 && j + 4 < out_size) {
    unsigned int n = (unsigned int)data[i] << 16;
    out[j++] = b64_chars[(n >> 18) & 0x3F];
    out[j++] = b64_chars[(n >> 12) & 0x3F];
    out[j++] = '=';
    out[j++] = '=';
  } else if (rem == 2 && j + 4 < out_size) {
    unsigned int n =
        ((unsigned int)data[i] << 16) | ((unsigned int)data[i + 1] << 8);
    out[j++] = b64_chars[(n >> 18) & 0x3F];
    out[j++] = b64_chars[(n >> 12) & 0x3F];
    out[j++] = b64_chars[(n >> 6) & 0x3F];
    out[j++] = '=';
  }
  out[j] = '\0';
}

int v6_bundler_ws_accept_key(const char* client_key, char* out,
                             size_t out_size) {
  char combined[256];
  int n = snprintf(combined, sizeof(combined), "%s%s", client_key, v6_ws_guid);
  if (n < 0 || (size_t)n >= sizeof(combined))
    return -1;

  v6_bundler_sha1_ctx ctx;
  unsigned char digest[20];
  v6_bundler_sha1_init(&ctx);
  v6_bundler_sha1_update(&ctx, (const unsigned char*)combined,
                         strlen(combined));
  v6_bundler_sha1_final(&ctx, digest);

  base64_encode(digest, 20, out, out_size);
  return 0;
}

int v6_bundler_ws_encode_text_frame(const char* text, size_t text_len,
                                    unsigned char* out, size_t out_cap,
                                    size_t* out_len) {
  size_t header_len;
  if (text_len < 126) {
    header_len = 2;
  } else if (text_len <= 0xFFFF) {
    header_len = 4;
  } else {
    header_len = 10;
  }
  if (header_len + text_len > out_cap)
    return -1;

  out[0] = 0x81;
  if (text_len < 126) {
    out[1] = (unsigned char)text_len;
  } else if (text_len <= 0xFFFF) {
    out[1] = 126;
    out[2] = (unsigned char)(text_len >> 8);
    out[3] = (unsigned char)(text_len & 0xFF);
  } else {
    out[1] = 127;
    for (int i = 0; i < 8; i++)
      out[2 + i] =
          (unsigned char)((unsigned long long)text_len >> (56 - i * 8));
  }
  memcpy(out + header_len, text, text_len);
  *out_len = header_len + text_len;
  return 0;
}
