public final class V6TransformStreamConstructor
    extends V6Object implements V6NativeConstructor {
  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  @Override
  public V6Value construct(V6Value[] args) {
    V6Value transformerVal = V6Value.argAt(args, 0);
    V6Callable transformFn = null;
    V6Callable flushFn = null;
    V6Callable startFn = null;
    if (transformerVal.tag() == V6Value.TAG_OBJ &&
        transformerVal.ref() instanceof V6Object) {
      V6Object tr = (V6Object)transformerVal.ref();
      V6Value tv = tr.get("transform");
      if (tv.tag() == V6Value.TAG_FUNC)
        transformFn = tv.asCallable();
      V6Value fv = tr.get("flush");
      if (fv.tag() == V6Value.TAG_FUNC)
        flushFn = fv.asCallable();
      V6Value sv = tr.get("start");
      if (sv.tag() == V6Value.TAG_FUNC)
        startFn = sv.asCallable();
    }
    final V6Callable fTransform = transformFn;
    final V6Callable fFlush = flushFn;

    V6ReadableStreamObject readable =
        V6ReadableStreamConstructor.newStream(objValue(new V6Object()));
    V6Object readableController = readable.controller;

    V6Object sink = new V6Object();
    sink.set("write", fn((t, a) -> {
               V6Value chunk = V6Value.argAt(a, 0);
               if (fTransform != null)
                 return fTransform.call(
                     transformerVal,
                     new V6Value[] {chunk, objValue(readableController)});
               readable.enqueue(chunk);
               return UNDEF;
             }));
    sink.set("close", fn((t, a) -> {
               if (fFlush != null) {
                 V6Value r =
                     fFlush.call(transformerVal,
                                 new V6Value[] {objValue(readableController)});
                 V6Promise.resolved(r).addCallbacks(
                     (okVal)
                         -> readable.closeStream(),
                     (errVal) -> readable.errorStream(errVal));
               } else {
                 readable.closeStream();
               }
               return UNDEF;
             }));
    sink.set("abort", fn((t, a) -> {
               readable.errorStream(V6Value.argAt(a, 0));
               return UNDEF;
             }));

    V6WritableStreamObject writable =
        V6WritableStreamConstructor.newStream(objValue(sink));

    if (startFn != null)
      startFn.call(transformerVal,
                   new V6Value[] {objValue(readableController)});

    V6Object result = new V6Object();
    result.set("readable", objValue(readable));
    result.set("writable", objValue(writable));
    return objValue(result);
  }
}
