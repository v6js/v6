#include "v6/parser.h"

#include "v6/module.h"
#include "v6/internal.h"

#include "v6/closures.h"

void emit_wrap_generator(compiler* c) {
  uint16_t ascall_idx =
      cf_methodref(c->cf, "V6Value", "asCallable", "()LV6Callable;");
  op_emit2(c->m, op_invokevirtual, ascall_idx);
  uint16_t tmp_slot = c->next_local_slot++;
  emit_astore(c->m, tmp_slot);
  uint16_t genfn_cls = cf_class(c->cf, "V6GeneratorFunction");
  uint16_t genfn_ctor =
      cf_methodref(c->cf, "V6GeneratorFunction", "<init>", "(LV6Callable;)V");
  op_emit2(c->m, op_new, genfn_cls);
  op_emit(c->m, op_dup);
  emit_aload(c->m, tmp_slot);
  op_emit2(c->m, op_invokespecial, genfn_ctor);
  emit_box_ref_computed(c, V6_TAG_FUNC);
}

void emit_wrap_async(compiler* c) {
  uint16_t ascall_idx =
      cf_methodref(c->cf, "V6Value", "asCallable", "()LV6Callable;");
  op_emit2(c->m, op_invokevirtual, ascall_idx);
  uint16_t tmp_slot = c->next_local_slot++;
  emit_astore(c->m, tmp_slot);
  uint16_t asyncfn_cls = cf_class(c->cf, "V6AsyncFunction");
  uint16_t asyncfn_ctor =
      cf_methodref(c->cf, "V6AsyncFunction", "<init>", "(LV6Callable;)V");
  op_emit2(c->m, op_new, asyncfn_cls);
  op_emit(c->m, op_dup);
  emit_aload(c->m, tmp_slot);
  op_emit2(c->m, op_invokespecial, asyncfn_ctor);
  emit_box_ref_computed(c, V6_TAG_FUNC);
}

void emit_wrap_async_generator(compiler* c) {
  uint16_t ascall_idx =
      cf_methodref(c->cf, "V6Value", "asCallable", "()LV6Callable;");
  op_emit2(c->m, op_invokevirtual, ascall_idx);
  uint16_t tmp_slot = c->next_local_slot++;
  emit_astore(c->m, tmp_slot);
  uint16_t asyncgenfn_cls = cf_class(c->cf, "V6AsyncGeneratorFunction");
  uint16_t asyncgenfn_ctor = cf_methodref(c->cf, "V6AsyncGeneratorFunction",
                                          "<init>", "(LV6Callable;)V");
  op_emit2(c->m, op_new, asyncgenfn_cls);
  op_emit(c->m, op_dup);
  emit_aload(c->m, tmp_slot);
  op_emit2(c->m, op_invokespecial, asyncgenfn_ctor);
  emit_box_ref_computed(c, V6_TAG_FUNC);
}
