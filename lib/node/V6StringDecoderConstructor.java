public final class V6StringDecoderConstructor
    extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();

  public V6StringDecoderConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, PROTOTYPE));
  }

  @Override
  public V6Value construct(V6Value[] args) {
    String enc =
        args.length > 0 && !args[0].isUndefined() ? args[0].toString() : "utf8";
    V6StringDecoderObject o = new V6StringDecoderObject(enc);
    o.setProto(PROTOTYPE);
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  private static byte[] bytesOf(V6Value v) {
    if (v.tag() == V6Value.TAG_OBJ && v.ref() instanceof V6Buffer)
      return ((V6Buffer)v.ref()).toBytes();
    if (v.isUndefined())
      return new byte[0];
    return v.toString().getBytes(java.nio.charset.StandardCharsets.UTF_8);
  }

  private static V6Object buildPrototype() {
    V6Object o = new V6Object();
    o.set("write", fn((t, a)
                          -> str(((V6StringDecoderObject)t.ref())
                                     .write(bytesOf(V6Value.argAt(a, 0))))));
    o.set("end", fn((t, a)
                        -> str(((V6StringDecoderObject)t.ref())
                                   .end(bytesOf(V6Value.argAt(a, 0))))));
    return o;
  }
}
