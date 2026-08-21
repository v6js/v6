#pragma once

#include <stddef.h>

typedef int (*v6_bundler_resolve_hook)(void* state, const char* importer_dir,
                                       const char* specifier, char* out_path,
                                       size_t out_size);
typedef char* (*v6_bundler_transform_hook)(void* state, const char* path,
                                           char* source, size_t source_len,
                                           size_t* out_len);
typedef char* (*v6_bundler_finalize_hook)(void* state, char* output,
                                          size_t output_len, size_t* out_len);
typedef void (*v6_bundler_emit_hook)(void* state, const char* outdir);

typedef struct v6_bundler_extension {
  const char* name;
  void* state;
  v6_bundler_resolve_hook resolve;
  v6_bundler_transform_hook transform;
  v6_bundler_finalize_hook finalize;
  v6_bundler_emit_hook emit;
} v6_bundler_extension;

#define v6_bundler_max_extensions 16

typedef struct v6_bundler_extension_set {
  v6_bundler_extension items[v6_bundler_max_extensions];
  int count;
} v6_bundler_extension_set;

void v6_bundler_extension_set_init(v6_bundler_extension_set* set);
void v6_bundler_extension_set_add(v6_bundler_extension_set* set,
                                  v6_bundler_extension ext);

int v6_bundler_extension_run_resolve(v6_bundler_extension_set* set,
                                     const char* importer_dir,
                                     const char* specifier, char* out_path,
                                     size_t out_size);
char* v6_bundler_extension_run_transform(v6_bundler_extension_set* set,
                                         const char* path, char* source,
                                         size_t source_len, size_t* out_len);
char* v6_bundler_extension_run_finalize(v6_bundler_extension_set* set,
                                        char* output, size_t output_len,
                                        size_t* out_len);
void v6_bundler_extension_run_emit(v6_bundler_extension_set* set,
                                   const char* outdir);
