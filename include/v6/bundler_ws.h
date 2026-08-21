#pragma once

#include <stddef.h>

int v6_bundler_ws_accept_key(const char* client_key, char* out,
                             size_t out_size);
int v6_bundler_ws_encode_text_frame(const char* text, size_t text_len,
                                    unsigned char* out, size_t out_cap,
                                    size_t* out_len);
