#include "v6/jar.h"

#include <stdlib.h>
#include <string.h>

static uint32_t crc32_of(const uint8_t* data, size_t len) {
  uint32_t crc = 0xffffffffu;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
      crc = (crc >> 1) ^ (0xedb88320u & mask);
    }
  }
  return ~crc;
}

static void buf_u16le(buf* b, uint16_t v) {
  buf_u8(b, (uint8_t)v);
  buf_u8(b, (uint8_t)(v >> 8));
}

static void buf_u32le(buf* b, uint32_t v) {
  buf_u8(b, (uint8_t)v);
  buf_u8(b, (uint8_t)(v >> 8));
  buf_u8(b, (uint8_t)(v >> 16));
  buf_u8(b, (uint8_t)(v >> 24));
}

static void write_manifest(buf* man, const char* main_class) {
  buf_bytes(man, (const uint8_t*)"Manifest-Version: 1.0\r\n", 23);
  buf_bytes(man, (const uint8_t*)"Main-Class: ", 12);
  buf_bytes(man, (const uint8_t*)main_class, strlen(main_class));
  buf_bytes(man, (const uint8_t*)"\r\n\r\n", 4);
}

void jar_write(buf* out, const jar_entry* entries, size_t count,
               const char* main_class) {
  buf manifest;
  buf_init(&manifest);
  write_manifest(&manifest, main_class);

  size_t total = count + 1;
  jar_entry* all = malloc(total * sizeof(jar_entry));
  all[0].name = "META-INF/MANIFEST.MF";
  all[0].data = manifest.data;
  all[0].len = manifest.len;
  for (size_t i = 0; i < count; i++)
    all[i + 1] = entries[i];

  size_t* offsets = malloc(total * sizeof(size_t));
  uint32_t* crcs = malloc(total * sizeof(uint32_t));

  for (size_t i = 0; i < total; i++) {
    offsets[i] = out->len;
    crcs[i] = crc32_of(all[i].data, all[i].len);
    size_t name_len = strlen(all[i].name);

    buf_u32le(out, 0x04034b50u);
    buf_u16le(out, 20);
    buf_u16le(out, 0);
    buf_u16le(out, 0);
    buf_u16le(out, 0);
    buf_u16le(out, 0x0021);
    buf_u32le(out, crcs[i]);
    buf_u32le(out, (uint32_t)all[i].len);
    buf_u32le(out, (uint32_t)all[i].len);
    buf_u16le(out, (uint16_t)name_len);
    buf_u16le(out, 0);
    buf_bytes(out, (const uint8_t*)all[i].name, name_len);
    buf_bytes(out, all[i].data, all[i].len);
  }

  size_t cd_start = out->len;

  for (size_t i = 0; i < total; i++) {
    size_t name_len = strlen(all[i].name);

    buf_u32le(out, 0x02014b50u);
    buf_u16le(out, 20);
    buf_u16le(out, 20);
    buf_u16le(out, 0);
    buf_u16le(out, 0);
    buf_u16le(out, 0);
    buf_u16le(out, 0x0021);
    buf_u32le(out, crcs[i]);
    buf_u32le(out, (uint32_t)all[i].len);
    buf_u32le(out, (uint32_t)all[i].len);
    buf_u16le(out, (uint16_t)name_len);
    buf_u16le(out, 0);
    buf_u16le(out, 0);
    buf_u16le(out, 0);
    buf_u16le(out, 0);
    buf_u32le(out, 0);
    buf_u32le(out, (uint32_t)offsets[i]);
    buf_bytes(out, (const uint8_t*)all[i].name, name_len);
  }

  size_t cd_size = out->len - cd_start;

  buf_u32le(out, 0x06054b50u);
  buf_u16le(out, 0);
  buf_u16le(out, 0);
  buf_u16le(out, (uint16_t)total);
  buf_u16le(out, (uint16_t)total);
  buf_u32le(out, (uint32_t)cd_size);
  buf_u32le(out, (uint32_t)cd_start);
  buf_u16le(out, 0);

  free(all);
  free(offsets);
  free(crcs);
  buf_free(&manifest);
}
