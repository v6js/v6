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
    PROTOTYPE.set("fill", fn((thisArg, args) -> {
                    V6Uint8ArrayObject self = (V6Uint8ArrayObject)thisArg.ref();
                    byte value = (byte)(int)V6Value.argAt(args, 0).toNumber();
                    int len = self.data.length;
                    int start = clampIndex(V6Value.argAt(args, 1), 0, len);
                    int end = clampIndex(V6Value.argAt(args, 2), len, len);
                    V6TypedArraySimd.fill(self.data, start, end, value);
                    return thisArg;
                  }));
    PROTOTYPE.set("copyWithin", fn((thisArg, args) -> {
                    V6Uint8ArrayObject self = (V6Uint8ArrayObject)thisArg.ref();
                    int len = self.data.length;
                    int target = clampIndex(V6Value.argAt(args, 0), 0, len);
                    int start = clampIndex(V6Value.argAt(args, 1), 0, len);
                    int end = clampIndex(V6Value.argAt(args, 2), len, len);
                    int count = Math.min(end - start, len - target);
                    if (count > 0)
                      System.arraycopy(self.data, start, self.data, target,
                                       count);
                    return thisArg;
                  }));
    PROTOTYPE.set("set", fn((thisArg, args) -> {
                    V6Uint8ArrayObject self = (V6Uint8ArrayObject)thisArg.ref();
                    V6Value src = V6Value.argAt(args, 0);
                    int offset = (int)V6Value.argAt(args, 1).toNumber();
                    if (src.tag() == V6Value.TAG_OBJ &&
                        src.ref() instanceof V6Uint8ArrayObject other) {
                      System.arraycopy(other.data, 0, self.data, offset,
                                       other.data.length);
                    } else if (src.tag() == V6Value.TAG_OBJ) {
                      V6Object arr = (V6Object)src.ref();
                      int n = (int)arr.get("length").num();
                      for (int i = 0; i < n; i++)
                        self.data[offset + i] =
                            (byte)(int)arr.get(Integer.toString(i)).toNumber();
                    }
                    return new V6Value(V6Value.TAG_UNDEF, 0, null);
                  }));
    PROTOTYPE.set("slice", fn((thisArg, args) -> {
                    V6Uint8ArrayObject self = (V6Uint8ArrayObject)thisArg.ref();
                    int len = self.data.length;
                    int start = clampIndex(V6Value.argAt(args, 0), 0, len);
                    int end = clampIndex(V6Value.argAt(args, 1), len, len);
                    int count = Math.max(0, end - start);
                    byte[] out = new byte[count];
                    if (count > 0)
                      System.arraycopy(self.data, start, out, 0, count);
                    return objValue(new V6Uint8ArrayObject(out));
                  }));
  }

  private static int clampIndex(V6Value v, int dflt, int len) {
    if (v == null || v.tag() == V6Value.TAG_UNDEF)
      return dflt;
    int n = (int)v.toNumber();
    if (n < 0)
      n = Math.max(0, len + n);
    return Math.max(0, Math.min(n, len));
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
