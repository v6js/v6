#include "v6/bundler_ext_public.h"
#include "v6/bundler_fsutil.h"

static void public_emit(void* state_v, const char* outdir) {
  const char* public_dir = (const char*)state_v;
  v6_bundler_copy_dir_recursive(public_dir, outdir);
}

v6_bundler_extension v6_bundler_public_extension(const char* public_dir) {
  v6_bundler_extension ext;
  ext.name = "public";
  ext.state = (void*)public_dir;
  ext.resolve = NULL;
  ext.transform = NULL;
  ext.finalize = NULL;
  ext.emit = public_emit;
  return ext;
}
