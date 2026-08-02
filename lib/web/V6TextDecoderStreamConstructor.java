public final class V6TextDecoderStreamConstructor
    extends V6Object implements V6NativeConstructor {
  private static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  private static byte[] bytesOf(V6Value v) {
    if (v.tag() == V6Value.TAG_OBJ && v.ref() instanceof V6Buffer)
      return ((V6Buffer)v.ref()).toBytes();
    return new byte[0];
  }

  @Override
  public V6Value construct(V6Value[] args) {
    String label = args.length > 0 && !args[0].isUndefined()
                       ? args[0].toString()
                       : "utf-8";
    String encoding = V6TextDecoderConstructor.normalizeLabel(label);
    V6StringDecoderObject decoder = new V6StringDecoderObject(encoding);

    V6Object transformer = new V6Object();
    transformer.set("transform", fn((t, a) -> {
                      byte[] bytes = bytesOf(V6Value.argAt(a, 0));
                      V6Object controller = (V6Object)V6Value.argAt(a, 1).ref();
                      String s = decoder.write(bytes);
                      if (!s.isEmpty())
                        controller.get("enqueue").asCallable().call(
                            objValue(controller), new V6Value[] {str(s)});
                      return UNDEF;
                    }));
    transformer.set("flush", fn((t, a) -> {
                      V6Object controller = (V6Object)V6Value.argAt(a, 0).ref();
                      String s = decoder.end(new byte[0]);
                      if (!s.isEmpty())
                        controller.get("enqueue").asCallable().call(
                            objValue(controller), new V6Value[] {str(s)});
                      return UNDEF;
                    }));

    V6Value result = new V6TransformStreamConstructor().construct(
        new V6Value[] {objValue(transformer)});
    V6Object resultObj = (V6Object)result.ref();
    resultObj.defineGetter("encoding", (t, a) -> str(encoding));
    return result;
  }
}
