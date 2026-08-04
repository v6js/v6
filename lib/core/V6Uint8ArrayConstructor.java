public final class V6Uint8ArrayConstructor
    extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = new V6Object();

  public V6Uint8ArrayConstructor() {
    set("prototype", objValue(PROTOTYPE));
    set("from", fn((thisArg,
                    args) -> objValue(new V6Uint8ArrayObject(fromArgs(args)))));
    set("of", fn((thisArg, args) -> {
          byte[] bytes = new byte[args.length];
          for (int i = 0; i < args.length; i++)
            bytes[i] = (byte)(int)args[i].toNumber();
          return objValue(new V6Uint8ArrayObject(bytes));
        }));
  }

  @Override
  public V6Value construct(V6Value[] args) {
    V6Value first = V6Value.argAt(args, 0);
    if (first.tag() == V6Value.TAG_NUM)
      return objValue(
          new V6Uint8ArrayObject(new byte[Math.max(0, (int)first.toNumber())]));
    return objValue(new V6Uint8ArrayObject(fromArgs(args)));
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }

  private static byte[] fromArgs(V6Value[] args) {
    V6Value first = V6Value.argAt(args, 0);
    if (first.tag() == V6Value.TAG_OBJ && first.ref() instanceof
                                              V6ArrayBufferObject)
      return ((V6ArrayBufferObject)first.ref()).data;
    if (first.tag() == V6Value.TAG_OBJ && first.ref() instanceof
                                              V6Uint8ArrayObject)
      return ((V6Uint8ArrayObject)first.ref()).toBytes();
    if (first.tag() == V6Value.TAG_STR) {
      String s = first.toString();
      byte[] out = new byte[s.length()];
      for (int i = 0; i < s.length(); i++)
        out[i] = (byte)s.charAt(i);
      return out;
    }
    if (first.tag() == V6Value.TAG_OBJ) {
      V6Object arr = (V6Object)first.ref();
      int n = (int)arr.get("length").num();
      byte[] out = new byte[n];
      for (int i = 0; i < n; i++)
        out[i] = (byte)(int)arr.get(Integer.toString(i)).toNumber();
      return out;
    }
    return new byte[0];
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }
}
