#pragma once

#include "v6/buffer.h"

#include <stddef.h>
#include <stdint.h>

typedef struct jar_entry {
  const char* name;
  const uint8_t* data;
  size_t len;
} jar_entry;

void jar_write(buf* out, const jar_entry* entries, size_t count,
               const char* main_class);
